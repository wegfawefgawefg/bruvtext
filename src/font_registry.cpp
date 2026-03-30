#include "font_registry.h"

#include <filesystem>

namespace bruvtext
{
FontId RegisterFont(
    FontRegistry& registry,
    FT_Library ftLibrary,
    const std::filesystem::path& assetsRoot,
    const FontDesc& desc)
{
    if (registry.count >= registry.fonts.size() || desc.filePath.empty() || ftLibrary == nullptr)
    {
        return 0;
    }

    std::filesystem::path requestedPath = std::filesystem::path(desc.filePath);
    std::filesystem::path resolvedPath = requestedPath.is_absolute() ? requestedPath : (assetsRoot.parent_path() / requestedPath);
    if (!std::filesystem::exists(resolvedPath))
    {
        return 0;
    }

    FT_Face face = nullptr;
    if (FT_New_Face(ftLibrary, resolvedPath.string().c_str(), 0, &face) != FT_Err_Ok)
    {
        return 0;
    }

    if (FT_Set_Pixel_Sizes(face, 0, 16) != FT_Err_Ok)
    {
        FT_Done_Face(face);
        return 0;
    }

    hb_font_t* hbFont = hb_ft_font_create_referenced(face);
    if (hbFont == nullptr)
    {
        FT_Done_Face(face);
        return 0;
    }
    hb_ft_font_set_load_flags(hbFont, kFtLoadFlags);

    RegisteredFont& slot = registry.fonts[registry.count++];
    slot.active = true;
    slot.id = registry.nextId++;
    slot.debugName = std::string(desc.debugName);
    slot.filePath = std::string(desc.filePath);
    slot.resolvedPath = std::move(resolvedPath);
    slot.face = face;
    slot.hbFont = hbFont;
    return slot.id;
}

const RegisteredFont* FindFontById(const FontRegistry& registry, FontId id)
{
    for (const RegisteredFont& font : registry.fonts)
    {
        if (font.active && font.id == id)
        {
            return &font;
        }
    }
    return nullptr;
}

void ShutdownFontRegistry(FontRegistry& registry)
{
    for (RegisteredFont& font : registry.fonts)
    {
        if (font.hbFont != nullptr)
        {
            hb_font_destroy(font.hbFont);
            font.hbFont = nullptr;
        }
        if (font.face != nullptr)
        {
            FT_Done_Face(font.face);
            font.face = nullptr;
        }
        font = {};
    }
    registry.count = 0;
    registry.nextId = 1;
}
}
