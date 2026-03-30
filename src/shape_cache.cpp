#include "shape_cache.h"

namespace bruvtext
{
void InitializeShapeCache(ShapeCache& cache)
{
    cache = {};
    cache.useTick = 1;
}

const CachedShapedRun* FindCachedShapedRun(
    ShapeCache& cache,
    FontId font,
    std::uint32_t rasterPixelSize,
    std::string_view text)
{
    for (CachedShapedRun& run : cache.runs)
    {
        if (run.font != font ||
            run.rasterPixelSize != rasterPixelSize ||
            run.text != text)
        {
            continue;
        }

        run.useTick = cache.useTick++;
        return &run;
    }

    return nullptr;
}

const CachedShapedRun& StoreCachedShapedRun(
    ShapeCache& cache,
    CachedShapedRun&& run)
{
    run.useTick = cache.useTick++;
    cache.runs.push_back(std::move(run));
    return cache.runs.back();
}
}
