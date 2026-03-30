#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "bruvtext/bruvtext.h"
#include "font_registry.h"
#include "frame_state.h"

namespace bruvtext
{
constexpr std::uint32_t kMaxAtlasPages = 64;
constexpr std::uint32_t kAtlasPageWidth = 1024;
constexpr std::uint32_t kAtlasPageHeight = 1024;
constexpr std::uint32_t kMaxCachedGlyphs = 65536;

struct AtlasPage
{
    bool active = false;
    bool dirty = false;
    FontId font = 0;
    std::uint32_t pixelSize = 0;
    std::uint32_t width = kAtlasPageWidth;
    std::uint32_t height = kAtlasPageHeight;
    std::uint32_t penX = 1;
    std::uint32_t penY = 1;
    std::uint32_t rowHeight = 0;
    std::vector<std::uint8_t> pixels;
};

struct CachedGlyph
{
    bool active = false;
    FontId font = 0;
    std::uint32_t pixelSize = 0;
    std::uint32_t glyphIndex = 0;
    std::uint32_t atlasPage = 0;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::int32_t bearingX = 0;
    std::int32_t bearingY = 0;
};

struct AtlasCache
{
    std::array<AtlasPage, kMaxAtlasPages> pages = {};
    std::array<CachedGlyph, kMaxCachedGlyphs> glyphs = {};
    std::uint32_t pageCount = 0;
    std::uint32_t glyphCount = 0;
};

void InitializeAtlasCache(AtlasCache& cache);
const CachedGlyph* FindCachedGlyph(
    const AtlasCache& cache,
    FontId font,
    std::uint32_t pixelSize,
    std::uint32_t glyphIndex
);
bool CacheShapedGlyphs(
    AtlasCache& cache,
    const FrameState& frame,
    const FontRegistry& fonts
);
AtlasStats GetAtlasStats(const AtlasCache& cache);
void ClearAtlasCache(AtlasCache& cache);
void ClearAtlasCacheForFont(AtlasCache& cache, FontId font);
void ClearAtlasDirtyFlags(AtlasCache& cache);
}
