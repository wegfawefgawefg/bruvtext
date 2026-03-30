#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "frame_state.h"

namespace bruvtext
{
struct CachedShapedRun
{
    FontId font = 0;
    std::uint32_t rasterPixelSize = 0;
    std::string text = {};
    std::vector<ShapedGlyph> glyphs = {};
    std::uint64_t useTick = 0;
};

struct ShapeCache
{
    std::vector<CachedShapedRun> runs = {};
    std::uint64_t useTick = 1;
};

void InitializeShapeCache(ShapeCache& cache);
const CachedShapedRun* FindCachedShapedRun(
    ShapeCache& cache,
    FontId font,
    std::uint32_t rasterPixelSize,
    std::string_view text);
const CachedShapedRun& StoreCachedShapedRun(
    ShapeCache& cache,
    CachedShapedRun&& run);
}
