#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <harfbuzz/hb.h>
#include <harfbuzz/hb-ft.h>

#include "bruvtext/bruvtext.h"

namespace bruvtext
{
constexpr std::uint32_t kMaxFonts = 32;
constexpr FT_Int32 kFtLoadFlags = FT_LOAD_DEFAULT | FT_LOAD_TARGET_LIGHT;

struct RegisteredFont
{
    bool active = false;
    FontId id = 0;
    std::string debugName;
    std::string filePath;
    std::filesystem::path resolvedPath;
    FT_Face face = nullptr;
    hb_font_t* hbFont = nullptr;
};

struct FontRegistry
{
    std::array<RegisteredFont, kMaxFonts> fonts = {};
    std::uint32_t count = 0;
    FontId nextId = 1;
};

FontId RegisterFont(
    FontRegistry& registry,
    FT_Library ftLibrary,
    const std::filesystem::path& assetsRoot,
    const FontDesc& desc
);
const RegisteredFont* FindFontById(const FontRegistry& registry, FontId id);
void ShutdownFontRegistry(FontRegistry& registry);
}
