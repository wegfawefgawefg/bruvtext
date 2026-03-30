#include "vulkan_demo.h"

#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace demo
{
namespace
{
struct QueueFamilySelection
{
    std::uint32_t graphics = UINT32_MAX;
    std::uint32_t present = UINT32_MAX;
};

QueueFamilySelection FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    QueueFamilySelection selection{};

    std::uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());

    for (std::uint32_t familyIndex = 0; familyIndex < familyCount; ++familyIndex)
    {
        if ((families[familyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
        {
            selection.graphics = familyIndex;
        }

        VkBool32 presentSupported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, familyIndex, surface, &presentSupported);
        if (presentSupported == VK_TRUE)
        {
            selection.present = familyIndex;
        }

        if (selection.graphics != UINT32_MAX && selection.present != UINT32_MAX)
        {
            break;
        }
    }

    return selection;
}

bool HasSwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    std::uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount == 0)
    {
        return false;
    }

    std::uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    return presentModeCount > 0;
}

std::uint32_t FindMemoryType(
    VkPhysicalDevice physicalDevice,
    std::uint32_t typeFilter,
    VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    for (std::uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        if ((typeFilter & (1u << i)) != 0 &&
            (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    return UINT32_MAX;
}

VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
    for (const VkSurfaceFormatKHR& format : formats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return format;
        }
    }
    return formats.front();
}

VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes)
{
    for (VkPresentModeKHR mode : modes)
    {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return mode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D ChooseExtent(SDL_Window* window, const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        return capabilities.currentExtent;
    }

    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);

    VkExtent2D extent{};
    extent.width = std::clamp(
        static_cast<std::uint32_t>(std::max(width, 1)),
        capabilities.minImageExtent.width,
        capabilities.maxImageExtent.width);
    extent.height = std::clamp(
        static_cast<std::uint32_t>(std::max(height, 1)),
        capabilities.minImageExtent.height,
        capabilities.maxImageExtent.height);
    return extent;
}

bool ReadBinaryFile(const std::string& path, std::vector<char>& outBytes)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input.is_open())
    {
        return false;
    }

    const std::streamsize size = input.tellg();
    if (size <= 0)
    {
        return false;
    }

    outBytes.resize(static_cast<std::size_t>(size));
    input.seekg(0, std::ios::beg);
    input.read(outBytes.data(), size);
    return input.good();
}

void DestroyBuffer(VulkanDemo& demo, VkBuffer& buffer, VkDeviceMemory& memory)
{
    if (buffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(demo.device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }
    if (memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(demo.device, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
}

bool CreateBuffer(
    VulkanDemo& demo,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& outBuffer,
    VkDeviceMemory& outMemory)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(demo.device, &bufferInfo, nullptr, &outBuffer) != VK_SUCCESS)
    {
        return false;
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetBufferMemoryRequirements(demo.device, outBuffer, &memoryRequirements);
    const std::uint32_t memoryTypeIndex = FindMemoryType(
        demo.physicalDevice,
        memoryRequirements.memoryTypeBits,
        properties);
    if (memoryTypeIndex == UINT32_MAX)
    {
        vkDestroyBuffer(demo.device, outBuffer, nullptr);
        outBuffer = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;
    if (vkAllocateMemory(demo.device, &allocInfo, nullptr, &outMemory) != VK_SUCCESS)
    {
        vkDestroyBuffer(demo.device, outBuffer, nullptr);
        outBuffer = VK_NULL_HANDLE;
        return false;
    }

    if (vkBindBufferMemory(demo.device, outBuffer, outMemory, 0) != VK_SUCCESS)
    {
        vkDestroyBuffer(demo.device, outBuffer, nullptr);
        vkFreeMemory(demo.device, outMemory, nullptr);
        outBuffer = VK_NULL_HANDLE;
        outMemory = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

bool EnsureVertexBufferCapacity(VulkanDemo& demo, std::size_t neededBytes)
{
    if (neededBytes == 0)
    {
        neededBytes = sizeof(TextVertex) * 6;
    }
    if (demo.vertexBuffer != VK_NULL_HANDLE && demo.vertexBufferSize >= neededBytes)
    {
        return true;
    }

    DestroyBuffer(demo, demo.vertexBuffer, demo.vertexMemory);
    demo.vertexBufferSize = std::max<std::size_t>(neededBytes, 64 * 1024);
    return CreateBuffer(
        demo,
        demo.vertexBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        demo.vertexBuffer,
        demo.vertexMemory);
}

bool EnsureAtlasUploadBufferCapacity(VulkanDemo& demo, std::size_t neededBytes)
{
    if (neededBytes == 0)
    {
        neededBytes = 4;
    }
    if (demo.atlasUploadBuffer != VK_NULL_HANDLE && demo.atlasUploadBufferSize >= neededBytes)
    {
        return true;
    }

    DestroyBuffer(demo, demo.atlasUploadBuffer, demo.atlasUploadMemory);
    demo.atlasUploadBufferSize = std::max<std::size_t>(neededBytes, 4 * 1024 * 1024);
    return CreateBuffer(
        demo,
        demo.atlasUploadBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        demo.atlasUploadBuffer,
        demo.atlasUploadMemory);
}

void DestroyAtlasPage(VulkanDemo& demo, AtlasGpuPage& page)
{
    if (page.descriptorSet != VK_NULL_HANDLE && demo.descriptorPool != VK_NULL_HANDLE)
    {
        vkFreeDescriptorSets(demo.device, demo.descriptorPool, 1, &page.descriptorSet);
        page.descriptorSet = VK_NULL_HANDLE;
    }
    if (page.view != VK_NULL_HANDLE)
    {
        vkDestroyImageView(demo.device, page.view, nullptr);
        page.view = VK_NULL_HANDLE;
    }
    if (page.image != VK_NULL_HANDLE)
    {
        vkDestroyImage(demo.device, page.image, nullptr);
        page.image = VK_NULL_HANDLE;
    }
    if (page.memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(demo.device, page.memory, nullptr);
        page.memory = VK_NULL_HANDLE;
    }
    page = {};
}

bool CreateAtlasPageResources(
    VulkanDemo& demo,
    AtlasGpuPage& page,
    const bruvtext::AtlasPageView& atlasPage)
{
    page.width = atlasPage.width;
    page.height = atlasPage.height;
    page.font = atlasPage.font;
    page.pixelSize = atlasPage.pixelSize;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent.width = atlasPage.width;
    imageInfo.extent.height = atlasPage.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(demo.device, &imageInfo, nullptr, &page.image) != VK_SUCCESS)
    {
        return false;
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetImageMemoryRequirements(demo.device, page.image, &memoryRequirements);
    const std::uint32_t memoryTypeIndex = FindMemoryType(
        demo.physicalDevice,
        memoryRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memoryTypeIndex == UINT32_MAX)
    {
        DestroyAtlasPage(demo, page);
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;
    if (vkAllocateMemory(demo.device, &allocInfo, nullptr, &page.memory) != VK_SUCCESS)
    {
        DestroyAtlasPage(demo, page);
        return false;
    }

    if (vkBindImageMemory(demo.device, page.image, page.memory, 0) != VK_SUCCESS)
    {
        DestroyAtlasPage(demo, page);
        return false;
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = page.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(demo.device, &viewInfo, nullptr, &page.view) != VK_SUCCESS)
    {
        DestroyAtlasPage(demo, page);
        return false;
    }

    VkDescriptorSetAllocateInfo allocSetInfo{};
    allocSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocSetInfo.descriptorPool = demo.descriptorPool;
    allocSetInfo.descriptorSetCount = 1;
    allocSetInfo.pSetLayouts = &demo.descriptorSetLayout;
    if (vkAllocateDescriptorSets(demo.device, &allocSetInfo, &page.descriptorSet) != VK_SUCCESS)
    {
        DestroyAtlasPage(demo, page);
        return false;
    }

    VkDescriptorImageInfo imageDescriptor{};
    imageDescriptor.sampler = demo.atlasSampler;
    imageDescriptor.imageView = page.view;
    imageDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = page.descriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageDescriptor;
    vkUpdateDescriptorSets(demo.device, 1, &write, 0, nullptr);

    page.active = true;
    return true;
}

void DestroySwapchain(VulkanDemo& demo)
{
    if (demo.device == VK_NULL_HANDLE)
    {
        return;
    }

    for (VkFramebuffer framebuffer : demo.swapchainFramebuffers)
    {
        if (framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(demo.device, framebuffer, nullptr);
        }
    }
    demo.swapchainFramebuffers.clear();

    for (VkImageView view : demo.swapchainImageViews)
    {
        if (view != VK_NULL_HANDLE)
        {
            vkDestroyImageView(demo.device, view, nullptr);
        }
    }
    demo.swapchainImageViews.clear();

    if (!demo.commandBuffers.empty())
    {
        vkFreeCommandBuffers(
            demo.device,
            demo.commandPool,
            static_cast<std::uint32_t>(demo.commandBuffers.size()),
            demo.commandBuffers.data());
        demo.commandBuffers.clear();
    }

    if (demo.swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(demo.device, demo.swapchain, nullptr);
        demo.swapchain = VK_NULL_HANDLE;
    }

    demo.swapchainImages.clear();
    demo.imageInitialized.clear();
}

bool CreateSwapchain(VulkanDemo& demo)
{
    VkSurfaceCapabilitiesKHR capabilities{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(demo.physicalDevice, demo.surface, &capabilities);

    std::uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(demo.physicalDevice, demo.surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(demo.physicalDevice, demo.surface, &formatCount, formats.data());

    std::uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(demo.physicalDevice, demo.surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        demo.physicalDevice,
        demo.surface,
        &presentModeCount,
        presentModes.data());

    const VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(formats);
    const VkPresentModeKHR presentMode = ChoosePresentMode(presentModes);
    const VkExtent2D extent = ChooseExtent(demo.window, capabilities);

    std::uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0)
    {
        imageCount = std::min(imageCount, capabilities.maxImageCount);
    }

    const std::array<std::uint32_t, 2> familyIndices{
        demo.graphicsQueueFamily,
        demo.presentQueueFamily,
    };

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = demo.surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (demo.graphicsQueueFamily != demo.presentQueueFamily)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = static_cast<std::uint32_t>(familyIndices.size());
        createInfo.pQueueFamilyIndices = familyIndices.data();
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(demo.device, &createInfo, nullptr, &demo.swapchain) != VK_SUCCESS)
    {
        return false;
    }

    demo.swapchainFormat = surfaceFormat.format;
    demo.swapchainExtent = extent;

    vkGetSwapchainImagesKHR(demo.device, demo.swapchain, &imageCount, nullptr);
    demo.swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(demo.device, demo.swapchain, &imageCount, demo.swapchainImages.data());
    demo.imageInitialized.assign(imageCount, false);
    demo.commandBuffers.resize(imageCount, VK_NULL_HANDLE);
    demo.swapchainImageViews.resize(imageCount, VK_NULL_HANDLE);

    for (std::uint32_t i = 0; i < imageCount; ++i)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = demo.swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = demo.swapchainFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(demo.device, &viewInfo, nullptr, &demo.swapchainImageViews[i]) != VK_SUCCESS)
        {
            return false;
        }
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = demo.commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = imageCount;
    return vkAllocateCommandBuffers(demo.device, &allocInfo, demo.commandBuffers.data()) == VK_SUCCESS;
}

bool CreateRenderPass(VulkanDemo& demo)
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = demo.swapchainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    return vkCreateRenderPass(demo.device, &renderPassInfo, nullptr, &demo.renderPass) == VK_SUCCESS;
}

bool CreateSwapchainFramebuffers(VulkanDemo& demo)
{
    demo.swapchainFramebuffers.resize(demo.swapchainImageViews.size(), VK_NULL_HANDLE);
    for (std::size_t i = 0; i < demo.swapchainImageViews.size(); ++i)
    {
        const VkImageView attachment = demo.swapchainImageViews[i];
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = demo.renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &attachment;
        framebufferInfo.width = demo.swapchainExtent.width;
        framebufferInfo.height = demo.swapchainExtent.height;
        framebufferInfo.layers = 1;
        if (vkCreateFramebuffer(demo.device, &framebufferInfo, nullptr, &demo.swapchainFramebuffers[i]) != VK_SUCCESS)
        {
            return false;
        }
    }
    return true;
}

bool CreateDescriptorResources(VulkanDemo& demo)
{
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(demo.device, &layoutInfo, nullptr, &demo.descriptorSetLayout) != VK_SUCCESS)
    {
        return false;
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = kDemoMaxAtlasPages;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = kDemoMaxAtlasPages;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    if (vkCreateDescriptorPool(demo.device, &poolInfo, nullptr, &demo.descriptorPool) != VK_SUCCESS)
    {
        return false;
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    return vkCreateSampler(demo.device, &samplerInfo, nullptr, &demo.atlasSampler) == VK_SUCCESS;
}

bool CreatePipeline(VulkanDemo& demo)
{
    std::vector<char> vertCode;
    std::vector<char> fragCode;
    if (!ReadBinaryFile(std::string(BRUVTEXT_SHADER_DIR) + "/text.vert.spv", vertCode) ||
        !ReadBinaryFile(std::string(BRUVTEXT_SHADER_DIR) + "/text.frag.spv", fragCode))
    {
        return false;
    }

    auto createModule = [&](const std::vector<char>& code, VkShaderModule& outModule) -> bool {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const std::uint32_t*>(code.data());
        return vkCreateShaderModule(demo.device, &createInfo, nullptr, &outModule) == VK_SUCCESS;
    };

    VkShaderModule vertModule = VK_NULL_HANDLE;
    VkShaderModule fragModule = VK_NULL_HANDLE;
    if (!createModule(vertCode, vertModule) || !createModule(fragCode, fragModule))
    {
        if (vertModule != VK_NULL_HANDLE) vkDestroyShaderModule(demo.device, vertModule, nullptr);
        if (fragModule != VK_NULL_HANDLE) vkDestroyShaderModule(demo.device, fragModule, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "main";

    const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{vertStage, fragStage};

    const std::array<VkVertexInputBindingDescription, 1> bindings{{
        {0, sizeof(TextVertex), VK_VERTEX_INPUT_RATE_VERTEX},
    }};
    const std::array<VkVertexInputAttributeDescription, 5> attributes{{
        {0, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<std::uint32_t>(offsetof(TextVertex, x))},
        {1, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<std::uint32_t>(offsetof(TextVertex, localU))},
        {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<std::uint32_t>(offsetof(TextVertex, drawX))},
        {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<std::uint32_t>(offsetof(TextVertex, atlasX))},
        {4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<std::uint32_t>(offsetof(TextVertex, r))},
    }};

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = static_cast<std::uint32_t>(bindings.size());
    vertexInput.pVertexBindingDescriptions = bindings.data();
    vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.blendEnable = VK_TRUE;
    blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &blendAttachment;

    const std::array<VkDynamicState, 2> dynamicStates{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &demo.descriptorSetLayout;
    if (vkCreatePipelineLayout(demo.device, &layoutInfo, nullptr, &demo.pipelineLayout) != VK_SUCCESS)
    {
        vkDestroyShaderModule(demo.device, vertModule, nullptr);
        vkDestroyShaderModule(demo.device, fragModule, nullptr);
        return false;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<std::uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = demo.pipelineLayout;
    pipelineInfo.renderPass = demo.renderPass;
    pipelineInfo.subpass = 0;

    const bool ok =
        vkCreateGraphicsPipelines(demo.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &demo.pipeline) == VK_SUCCESS;
    vkDestroyShaderModule(demo.device, vertModule, nullptr);
    vkDestroyShaderModule(demo.device, fragModule, nullptr);
    return ok;
}

bool RecreateSwapchain(VulkanDemo& demo)
{
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(demo.window, &width, &height);
    if (width <= 0 || height <= 0)
    {
        return true;
    }

    vkDeviceWaitIdle(demo.device);
    DestroySwapchain(demo);
    return CreateSwapchain(demo) && CreateSwapchainFramebuffers(demo);
}

void AppendGlyphVertices(
    std::vector<TextVertex>& outVertices,
    const bruvtext::DrawGlyphView& glyph,
    std::uint32_t framebufferWidth,
    std::uint32_t framebufferHeight)
{
    const float left = (glyph.x0 / static_cast<float>(framebufferWidth)) * 2.0f - 1.0f;
    const float right = (glyph.x1 / static_cast<float>(framebufferWidth)) * 2.0f - 1.0f;
    const float top = (glyph.y0 / static_cast<float>(framebufferHeight)) * 2.0f - 1.0f;
    const float bottom = (glyph.y1 / static_cast<float>(framebufferHeight)) * 2.0f - 1.0f;

    const float atlasX = static_cast<float>(glyph.atlasX);
    const float atlasY = static_cast<float>(glyph.atlasY);
    const float atlasWidth = static_cast<float>(glyph.atlasWidth);
    const float atlasHeight = static_cast<float>(glyph.atlasHeight);

    const float drawX = glyph.x0;
    const float drawY = glyph.y0;
    const float drawWidth = glyph.x1 - glyph.x0;
    const float drawHeight = glyph.y1 - glyph.y0;

    const TextVertex v0{
        left, top,
        0.0f, 0.0f,
        drawX, drawY, drawWidth, drawHeight,
        atlasX, atlasY, atlasWidth, atlasHeight,
        glyph.color.r, glyph.color.g, glyph.color.b, glyph.color.a};
    const TextVertex v1{
        right, top,
        1.0f, 0.0f,
        drawX, drawY, drawWidth, drawHeight,
        atlasX, atlasY, atlasWidth, atlasHeight,
        glyph.color.r, glyph.color.g, glyph.color.b, glyph.color.a};
    const TextVertex v2{
        right, bottom,
        1.0f, 1.0f,
        drawX, drawY, drawWidth, drawHeight,
        atlasX, atlasY, atlasWidth, atlasHeight,
        glyph.color.r, glyph.color.g, glyph.color.b, glyph.color.a};
    const TextVertex v3{
        left, bottom,
        0.0f, 1.0f,
        drawX, drawY, drawWidth, drawHeight,
        atlasX, atlasY, atlasWidth, atlasHeight,
        glyph.color.r, glyph.color.g, glyph.color.b, glyph.color.a};

    outVertices.push_back(v0);
    outVertices.push_back(v1);
    outVertices.push_back(v2);
    outVertices.push_back(v0);
    outVertices.push_back(v2);
    outVertices.push_back(v3);
}

bool BuildVertexBuffer(VulkanDemo& demo, const bruvtext::Context& textContext)
{
    demo.cpuVertices.clear();
    demo.cpuVertices.reserve(bruvtext::GetDrawGlyphCount(textContext) * 6);

    const bruvtext::DrawGlyphView* glyphs = bruvtext::GetDrawGlyphs(textContext);
    for (std::size_t i = 0; i < bruvtext::GetDrawGlyphCount(textContext); ++i)
    {
        AppendGlyphVertices(demo.cpuVertices, glyphs[i], demo.swapchainExtent.width, demo.swapchainExtent.height);
    }

    const std::size_t neededBytes = demo.cpuVertices.size() * sizeof(TextVertex);
    if (!EnsureVertexBufferCapacity(demo, neededBytes))
    {
        return false;
    }
    if (neededBytes == 0)
    {
        return true;
    }

    void* mapped = nullptr;
    if (vkMapMemory(demo.device, demo.vertexMemory, 0, neededBytes, 0, &mapped) != VK_SUCCESS)
    {
        return false;
    }
    std::memcpy(mapped, demo.cpuVertices.data(), neededBytes);
    vkUnmapMemory(demo.device, demo.vertexMemory);
    return true;
}

bool EnsureAtlasGpuPages(
    VulkanDemo& demo,
    const bruvtext::Context& textContext,
    std::vector<std::uint32_t>& dirtyPages)
{
    dirtyPages.clear();
    const std::size_t pageCount = bruvtext::GetAtlasPageCount(textContext);
    const bruvtext::AtlasPageView* pages = bruvtext::GetAtlasPages(textContext);

    for (std::size_t i = 0; i < pageCount; ++i)
    {
        const bruvtext::AtlasPageView& atlasPage = pages[i];
        AtlasGpuPage& gpuPage = demo.atlasPages[i];
        const bool needsRecreate =
            !gpuPage.active ||
            gpuPage.width != atlasPage.width ||
            gpuPage.height != atlasPage.height ||
            gpuPage.font != atlasPage.font ||
            gpuPage.pixelSize != atlasPage.pixelSize;
        if (needsRecreate)
        {
            DestroyAtlasPage(demo, gpuPage);
            if (!CreateAtlasPageResources(demo, gpuPage, atlasPage))
            {
                return false;
            }
        }
        if (atlasPage.dirty || !gpuPage.initialized)
        {
            dirtyPages.push_back(static_cast<std::uint32_t>(i));
        }
    }

    return true;
}

bool UploadAtlasPage(
    VulkanDemo& demo,
    VkCommandBuffer commandBuffer,
    std::size_t uploadOffset,
    const bruvtext::AtlasPageView& page,
    AtlasGpuPage& gpuPage)
{
    const std::size_t uploadBytes =
        static_cast<std::size_t>(page.rowPitch) *
        static_cast<std::size_t>(page.height);
    if (!EnsureAtlasUploadBufferCapacity(demo, uploadBytes))
    {
        return false;
    }

    void* mapped = nullptr;
    if (vkMapMemory(demo.device, demo.atlasUploadMemory, uploadOffset, uploadBytes, 0, &mapped) != VK_SUCCESS)
    {
        return false;
    }
    std::memcpy(mapped, page.pixels, uploadBytes);
    vkUnmapMemory(demo.device, demo.atlasUploadMemory);

    VkImageMemoryBarrier toTransfer{};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.oldLayout = gpuPage.initialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = gpuPage.image;
    toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransfer.subresourceRange.baseMipLevel = 0;
    toTransfer.subresourceRange.levelCount = 1;
    toTransfer.subresourceRange.baseArrayLayer = 0;
    toTransfer.subresourceRange.layerCount = 1;
    toTransfer.srcAccessMask = gpuPage.initialized ? VK_ACCESS_SHADER_READ_BIT : 0;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        gpuPage.initialized ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &toTransfer);

    VkBufferImageCopy copyRegion{};
    copyRegion.bufferOffset = uploadOffset;
    copyRegion.bufferRowLength = page.rowPitch / 4;
    copyRegion.bufferImageHeight = page.height;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent.width = page.width;
    copyRegion.imageExtent.height = page.height;
    copyRegion.imageExtent.depth = 1;
    vkCmdCopyBufferToImage(
        commandBuffer,
        demo.atlasUploadBuffer,
        gpuPage.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &copyRegion);

    VkImageMemoryBarrier toShaderRead = toTransfer;
    toShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toShaderRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &toShaderRead);

    gpuPage.initialized = true;
    return true;
}

bool RecordRenderPass(
    VulkanDemo& demo,
    VkCommandBuffer commandBuffer,
    std::uint32_t imageIndex,
    const bruvtext::Context& textContext,
    float timeSeconds)
{
    const float pulse = 0.5f + 0.5f * std::sin(timeSeconds * 1.7f);
    VkClearValue clearValue{};
    clearValue.color.float32[0] = 0.06f + 0.05f * pulse;
    clearValue.color.float32[1] = 0.08f + 0.03f * pulse;
    clearValue.color.float32[2] = 0.11f + 0.04f * pulse;
    clearValue.color.float32[3] = 1.0f;

    VkImageMemoryBarrier toColor{};
    toColor.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toColor.oldLayout = demo.imageInitialized[imageIndex] ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_UNDEFINED;
    toColor.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toColor.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toColor.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toColor.image = demo.swapchainImages[imageIndex];
    toColor.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toColor.subresourceRange.baseMipLevel = 0;
    toColor.subresourceRange.levelCount = 1;
    toColor.subresourceRange.baseArrayLayer = 0;
    toColor.subresourceRange.layerCount = 1;
    toColor.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &toColor);

    VkRenderPassBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = demo.renderPass;
    beginInfo.framebuffer = demo.swapchainFramebuffers[imageIndex];
    beginInfo.renderArea.extent = demo.swapchainExtent;
    beginInfo.clearValueCount = 1;
    beginInfo.pClearValues = &clearValue;
    vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.width = static_cast<float>(demo.swapchainExtent.width);
    viewport.height = static_cast<float>(demo.swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = demo.swapchainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, demo.pipeline);

    const VkDeviceSize vertexOffset = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &demo.vertexBuffer, &vertexOffset);

    const bruvtext::DrawBatchView* batches = bruvtext::GetDrawBatches(textContext);
    for (std::size_t batchIndex = 0; batchIndex < bruvtext::GetDrawBatchCount(textContext); ++batchIndex)
    {
        const bruvtext::DrawBatchView& batch = batches[batchIndex];
        if (batch.atlasPage >= demo.atlasPages.size())
        {
            continue;
        }
        const AtlasGpuPage& page = demo.atlasPages[batch.atlasPage];
        if (!page.active || page.descriptorSet == VK_NULL_HANDLE)
        {
            continue;
        }

        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            demo.pipelineLayout,
            0,
            1,
            &page.descriptorSet,
            0,
            nullptr);
        vkCmdDraw(
            commandBuffer,
            batch.glyphCount * 6,
            1,
            batch.firstGlyph * 6,
            0);
    }

    vkCmdEndRenderPass(commandBuffer);

    VkImageMemoryBarrier toPresent = toColor;
    toPresent.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    toPresent.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toPresent.dstAccessMask = 0;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &toPresent);

    demo.imageInitialized[imageIndex] = true;
    return true;
}
}

bool InitializeVulkanDemo(VulkanDemo& demo, SDL_Window* window)
{
    demo.window = window;

    std::uint32_t extensionCount = 0;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (extensions == nullptr || extensionCount == 0)
    {
        return false;
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "bruvtext-demo";
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    appInfo.pEngineName = "bruvtext";
    appInfo.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledExtensionCount = extensionCount;
    instanceInfo.ppEnabledExtensionNames = extensions;
    if (vkCreateInstance(&instanceInfo, nullptr, &demo.instance) != VK_SUCCESS)
    {
        return false;
    }

    if (!SDL_Vulkan_CreateSurface(window, demo.instance, nullptr, &demo.surface))
    {
        return false;
    }

    std::uint32_t physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(demo.instance, &physicalDeviceCount, nullptr);
    if (physicalDeviceCount == 0)
    {
        return false;
    }

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    vkEnumeratePhysicalDevices(demo.instance, &physicalDeviceCount, physicalDevices.data());
    for (VkPhysicalDevice candidate : physicalDevices)
    {
        const QueueFamilySelection families = FindQueueFamilies(candidate, demo.surface);
        if (families.graphics == UINT32_MAX || families.present == UINT32_MAX)
        {
            continue;
        }
        if (!HasSwapchainSupport(candidate, demo.surface))
        {
            continue;
        }

        demo.physicalDevice = candidate;
        demo.graphicsQueueFamily = families.graphics;
        demo.presentQueueFamily = families.present;
        break;
    }

    if (demo.physicalDevice == VK_NULL_HANDLE)
    {
        return false;
    }

    const float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    queueInfos.reserve(demo.graphicsQueueFamily == demo.presentQueueFamily ? 1 : 2);

    VkDeviceQueueCreateInfo graphicsInfo{};
    graphicsInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphicsInfo.queueFamilyIndex = demo.graphicsQueueFamily;
    graphicsInfo.queueCount = 1;
    graphicsInfo.pQueuePriorities = &queuePriority;
    queueInfos.push_back(graphicsInfo);

    if (demo.graphicsQueueFamily != demo.presentQueueFamily)
    {
        VkDeviceQueueCreateInfo presentInfo = graphicsInfo;
        presentInfo.queueFamilyIndex = demo.presentQueueFamily;
        queueInfos.push_back(presentInfo);
    }

    const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueInfos.size());
    deviceInfo.pQueueCreateInfos = queueInfos.data();
    deviceInfo.enabledExtensionCount = 1;
    deviceInfo.ppEnabledExtensionNames = deviceExtensions;
    if (vkCreateDevice(demo.physicalDevice, &deviceInfo, nullptr, &demo.device) != VK_SUCCESS)
    {
        return false;
    }

    vkGetDeviceQueue(demo.device, demo.graphicsQueueFamily, 0, &demo.graphicsQueue);
    vkGetDeviceQueue(demo.device, demo.presentQueueFamily, 0, &demo.presentQueue);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = demo.graphicsQueueFamily;
    if (vkCreateCommandPool(demo.device, &poolInfo, nullptr, &demo.commandPool) != VK_SUCCESS)
    {
        return false;
    }

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (vkCreateSemaphore(demo.device, &semaphoreInfo, nullptr, &demo.imageAvailable) != VK_SUCCESS)
    {
        return false;
    }
    if (vkCreateSemaphore(demo.device, &semaphoreInfo, nullptr, &demo.renderFinished) != VK_SUCCESS)
    {
        return false;
    }

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vkCreateFence(demo.device, &fenceInfo, nullptr, &demo.inFlightFence) != VK_SUCCESS)
    {
        return false;
    }

    return CreateSwapchain(demo) &&
        CreateDescriptorResources(demo) &&
        CreateRenderPass(demo) &&
        CreatePipeline(demo) &&
        CreateSwapchainFramebuffers(demo);
}

void ShutdownVulkanDemo(VulkanDemo& demo)
{
    if (demo.device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(demo.device);
    }

    for (AtlasGpuPage& page : demo.atlasPages)
    {
        DestroyAtlasPage(demo, page);
    }
    DestroyBuffer(demo, demo.vertexBuffer, demo.vertexMemory);
    DestroyBuffer(demo, demo.atlasUploadBuffer, demo.atlasUploadMemory);

    DestroySwapchain(demo);

    if (demo.pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(demo.device, demo.pipeline, nullptr);
    }
    if (demo.pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(demo.device, demo.pipelineLayout, nullptr);
    }
    if (demo.renderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(demo.device, demo.renderPass, nullptr);
    }
    if (demo.atlasSampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(demo.device, demo.atlasSampler, nullptr);
    }
    if (demo.descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(demo.device, demo.descriptorPool, nullptr);
    }
    if (demo.descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(demo.device, demo.descriptorSetLayout, nullptr);
    }
    if (demo.inFlightFence != VK_NULL_HANDLE)
    {
        vkDestroyFence(demo.device, demo.inFlightFence, nullptr);
    }
    if (demo.renderFinished != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(demo.device, demo.renderFinished, nullptr);
    }
    if (demo.imageAvailable != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(demo.device, demo.imageAvailable, nullptr);
    }
    if (demo.commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(demo.device, demo.commandPool, nullptr);
    }
    if (demo.device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(demo.device, nullptr);
    }
    if (demo.surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(demo.instance, demo.surface, nullptr);
    }
    if (demo.instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(demo.instance, nullptr);
    }

    demo = {};
}

bool RenderVulkanDemoFrame(VulkanDemo& demo, bruvtext::Context& textContext, float timeSeconds)
{
    if (demo.device == VK_NULL_HANDLE || demo.swapchain == VK_NULL_HANDLE)
    {
        return false;
    }

    int width = 0;
    int height = 0;
    SDL_GetWindowSize(demo.window, &width, &height);
    if (width <= 0 || height <= 0)
    {
        SDL_Delay(16);
        return true;
    }

    if (!BuildVertexBuffer(demo, textContext))
    {
        return false;
    }

    vkWaitForFences(demo.device, 1, &demo.inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(demo.device, 1, &demo.inFlightFence);

    std::uint32_t imageIndex = 0;
    const VkResult acquireResult = vkAcquireNextImageKHR(
        demo.device,
        demo.swapchain,
        UINT64_MAX,
        demo.imageAvailable,
        VK_NULL_HANDLE,
        &imageIndex);
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return RecreateSwapchain(demo);
    }
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
    {
        return false;
    }

    std::vector<std::uint32_t> dirtyAtlasPages;
    if (!EnsureAtlasGpuPages(demo, textContext, dirtyAtlasPages))
    {
        return false;
    }

    std::size_t totalUploadBytes = 0;
    const bruvtext::AtlasPageView* atlasViews = bruvtext::GetAtlasPages(textContext);
    for (std::uint32_t pageIndex : dirtyAtlasPages)
    {
        totalUploadBytes +=
            static_cast<std::size_t>(atlasViews[pageIndex].rowPitch) *
            static_cast<std::size_t>(atlasViews[pageIndex].height);
    }
    if (!EnsureAtlasUploadBufferCapacity(demo, totalUploadBytes))
    {
        return false;
    }

    VkCommandBuffer commandBuffer = demo.commandBuffers[imageIndex];
    vkResetCommandBuffer(commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        return false;
    }

    std::size_t uploadOffset = 0;
    for (std::uint32_t pageIndex : dirtyAtlasPages)
    {
        if (!UploadAtlasPage(
                demo,
                commandBuffer,
                uploadOffset,
                atlasViews[pageIndex],
                demo.atlasPages[pageIndex]))
        {
            return false;
        }
        uploadOffset +=
            static_cast<std::size_t>(atlasViews[pageIndex].rowPitch) *
            static_cast<std::size_t>(atlasViews[pageIndex].height);
    }

    if (!RecordRenderPass(demo, commandBuffer, imageIndex, textContext, timeSeconds))
    {
        return false;
    }

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        return false;
    }

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &demo.imageAvailable;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &demo.renderFinished;
    if (vkQueueSubmit(demo.graphicsQueue, 1, &submitInfo, demo.inFlightFence) != VK_SUCCESS)
    {
        return false;
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &demo.renderFinished;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &demo.swapchain;
    presentInfo.pImageIndices = &imageIndex;

    const VkResult presentResult = vkQueuePresentKHR(demo.presentQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
    {
        return RecreateSwapchain(demo);
    }
    if (presentResult != VK_SUCCESS)
    {
        return false;
    }

    bruvtext::ClearAtlasDirtyFlags(textContext);
    return true;
}
}
