#include "draw_list.h"

#include <cmath>

namespace bruvtext
{
bool BuildDrawGlyphs(FrameState& frame, const AtlasCache& cache)
{
    frame.drawGlyphs.clear();
    frame.drawBatches.clear();
    frame.drawGlyphs.reserve(frame.shapedGlyphs.size());

    for (const ShapedRun& run : frame.shapedRuns)
    {
        if (!run.active)
        {
            continue;
        }

        for (std::uint32_t glyphOffset = 0; glyphOffset < run.glyphCount; ++glyphOffset)
        {
            const ShapedGlyph& shaped = frame.shapedGlyphs[run.firstGlyph + glyphOffset];
            if (!shaped.active)
            {
                continue;
            }

            const CachedGlyph* cached =
                FindCachedGlyph(cache, run.font, run.rasterPixelSize, shaped.glyphIndex);
            if (cached == nullptr || !cached->active)
            {
                continue;
            }

            frame.drawGlyphs.emplace_back();
            DrawGlyph& draw = frame.drawGlyphs.back();
            draw = {};
            draw.active = true;
            draw.atlasPage = cached->atlasPage;
            draw.glyphIndex = shaped.glyphIndex;
            draw.atlasX = cached->x;
            draw.atlasY = cached->y;
            draw.atlasWidth = cached->width;
            draw.atlasHeight = cached->height;
            const float scale = std::max(run.scale, 0.0f);
            draw.x0 = run.position.x + (shaped.x + static_cast<float>(shaped.bearingX) - run.position.x) * scale;
            draw.y0 = run.position.y + (shaped.y - static_cast<float>(shaped.bearingY) - run.position.y) * scale;
            draw.x1 = draw.x0 + static_cast<float>(shaped.width) * scale;
            draw.y1 = draw.y0 + static_cast<float>(shaped.height) * scale;

            const bool isNativeScale = std::abs(scale - 1.0f) < 0.001f;
            const bool matchesRasterSize =
                shaped.width == cached->width &&
                shaped.height == cached->height;
            const bool useNativeTexelCenters = isNativeScale && matchesRasterSize;
            if (isNativeScale && matchesRasterSize)
            {
                draw.x0 = std::round(draw.x0);
                draw.y0 = std::round(draw.y0);
                draw.x1 = draw.x0 + static_cast<float>(cached->width);
                draw.y1 = draw.y0 + static_cast<float>(cached->height);
            }

            draw.color = run.color;

            if (cached->width > 0 && cached->height > 0)
            {
                const AtlasPage& page = cache.pages[cached->atlasPage];
                const float insetX = useNativeTexelCenters && cached->width > 1 ? 0.5f : 0.0f;
                const float insetY = useNativeTexelCenters && cached->height > 1 ? 0.5f : 0.0f;
                draw.u0 = (static_cast<float>(cached->x) + insetX) / static_cast<float>(page.width);
                draw.v0 = (static_cast<float>(cached->y) + insetY) / static_cast<float>(page.height);
                draw.u1 =
                    (static_cast<float>(cached->x + cached->width) - insetX) /
                    static_cast<float>(page.width);
                draw.v1 =
                    (static_cast<float>(cached->y + cached->height) - insetY) /
                    static_cast<float>(page.height);
            }

            if (frame.drawBatches.empty())
            {
                frame.drawBatches.emplace_back();
                DrawBatch& batch = frame.drawBatches.back();
                batch.active = true;
                batch.atlasPage = draw.atlasPage;
                batch.firstGlyph = static_cast<std::uint32_t>(frame.drawGlyphs.size() - 1);
                batch.glyphCount = 1;
                continue;
            }

            DrawBatch& lastBatch = frame.drawBatches.back();
            if (lastBatch.atlasPage == draw.atlasPage)
            {
                ++lastBatch.glyphCount;
                continue;
            }

            frame.drawBatches.emplace_back();
            DrawBatch& batch = frame.drawBatches.back();
            batch.active = true;
            batch.atlasPage = draw.atlasPage;
            batch.firstGlyph = static_cast<std::uint32_t>(frame.drawGlyphs.size() - 1);
            batch.glyphCount = 1;
        }
    }

    return true;
}
}
