#pragma once

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <vector>

#include "bruvtext/bruvtext.h"

namespace bruvtext
{
struct Context;
}

namespace demo
{
constexpr std::uint32_t kDemoMaxAtlasPages = 64;

struct TextVertex
{
    float x = 0.0f;
    float y = 0.0f;
    float localU = 0.0f;
    float localV = 0.0f;
    float drawX = 0.0f;
    float drawY = 0.0f;
    float drawWidth = 0.0f;
    float drawHeight = 0.0f;
    float atlasX = 0.0f;
    float atlasY = 0.0f;
    float atlasWidth = 0.0f;
    float atlasHeight = 0.0f;
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct AtlasGpuPage
{
    bool active = false;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t font = 0;
    std::uint32_t pixelSize = 0;
    bool initialized = false;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
};

struct VulkanDemo
{
    SDL_Window* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    std::uint32_t graphicsQueueFamily = UINT32_MAX;
    std::uint32_t presentQueueFamily = UINT32_MAX;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent{};
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    std::vector<VkFramebuffer> swapchainFramebuffers;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<bool> imageInitialized;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkSampler atlasSampler = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
    std::size_t vertexBufferSize = 0;
    std::vector<TextVertex> cpuVertices;
    VkBuffer atlasUploadBuffer = VK_NULL_HANDLE;
    VkDeviceMemory atlasUploadMemory = VK_NULL_HANDLE;
    std::size_t atlasUploadBufferSize = 0;
    std::array<AtlasGpuPage, kDemoMaxAtlasPages> atlasPages = {};
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkSemaphore imageAvailable = VK_NULL_HANDLE;
    VkSemaphore renderFinished = VK_NULL_HANDLE;
    VkFence inFlightFence = VK_NULL_HANDLE;
};

bool InitializeVulkanDemo(VulkanDemo& demo, SDL_Window* window);
void ShutdownVulkanDemo(VulkanDemo& demo);
bool RenderVulkanDemoFrame(VulkanDemo& demo, bruvtext::Context& textContext, float timeSeconds);
}
