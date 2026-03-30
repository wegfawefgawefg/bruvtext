#include "atlas_cache.h"

#include <algorithm>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace bruvtext
{
namespace
{
constexpr std::uint32_t kAtlasPadding = 1;

void InitializePage(AtlasPage& page)
{
    page = {};
    page.active = true;
    page.width = kAtlasPageWidth;
    page.height = kAtlasPageHeight;
    page.penX = kAtlasPadding;
    page.penY = kAtlasPadding;
    page.rowHeight = 0;
    page.pixels.resize(static_cast<std::size_t>(page.width) * page.height * 4, 0);
}

bool PageMatchesBucket(const AtlasPage& page, FontId font, std::uint32_t pixelSize)
{
    return page.active && page.font == font && page.pixelSize == pixelSize;
}

bool BeginNewRow(AtlasPage& page, std::uint32_t glyphHeight)
{
    page.penX = kAtlasPadding;
    page.penY += page.rowHeight + kAtlasPadding;
    page.rowHeight = 0;
    const std::uint32_t neededHeight = page.penY + glyphHeight + kAtlasPadding;
    return neededHeight <= page.height;
}

bool TryPlaceGlyph(
    AtlasPage& page,
    std::uint32_t glyphWidth,
    std::uint32_t glyphHeight,
    std::uint32_t& outX,
    std::uint32_t& outY
)
{
    if (!page.active)
    {
        InitializePage(page);
    }

    if (glyphWidth + kAtlasPadding * 2 > page.width || glyphHeight + kAtlasPadding * 2 > page.height)
    {
        return false;
    }

    if (page.penX + glyphWidth + kAtlasPadding > page.width)
    {
        if (!BeginNewRow(page, glyphHeight))
        {
            return false;
        }
    }

    if (page.penY + glyphHeight + kAtlasPadding > page.height)
    {
        return false;
    }

    outX = page.penX;
    outY = page.penY;
    page.penX += glyphWidth + kAtlasPadding;
    page.rowHeight = std::max(page.rowHeight, glyphHeight);
    return true;
}

bool PackGlyph(
    AtlasCache& cache,
    FontId font,
    std::uint32_t pixelSize,
    std::uint32_t glyphWidth,
    std::uint32_t glyphHeight,
    std::uint32_t& outPage,
    std::uint32_t& outX,
    std::uint32_t& outY
)
{
    for (std::uint32_t pageIndex = 0; pageIndex < cache.pageCount; ++pageIndex)
    {
        if (!PageMatchesBucket(cache.pages[pageIndex], font, pixelSize))
        {
            continue;
        }
        if (TryPlaceGlyph(cache.pages[pageIndex], glyphWidth, glyphHeight, outX, outY))
        {
            outPage = pageIndex;
            return true;
        }
    }

    if (cache.pageCount >= cache.pages.size())
    {
        return false;
    }

    outPage = cache.pageCount++;
    InitializePage(cache.pages[outPage]);
    cache.pages[outPage].font = font;
    cache.pages[outPage].pixelSize = pixelSize;
    if (!TryPlaceGlyph(cache.pages[outPage], glyphWidth, glyphHeight, outX, outY))
    {
        return false;
    }
    return true;
}

void BlitBitmapToPage(AtlasPage& page, std::uint32_t dstX, std::uint32_t dstY, const FT_Bitmap& bitmap)
{
    page.dirty = true;
    for (std::uint32_t row = 0; row < bitmap.rows; ++row)
    {
        for (std::uint32_t col = 0; col < bitmap.width; ++col)
        {
            const std::uint8_t alpha = bitmap.buffer[row * bitmap.pitch + col];
            const std::size_t pixelIndex =
                (static_cast<std::size_t>(dstY + row) * page.width + (dstX + col)) * 4;
            page.pixels[pixelIndex + 0] = 255;
            page.pixels[pixelIndex + 1] = 255;
            page.pixels[pixelIndex + 2] = 255;
            page.pixels[pixelIndex + 3] = alpha;
        }
    }
}

bool CacheGlyph(
    AtlasCache& cache,
    const RegisteredFont& font,
    std::uint32_t pixelSize,
    std::uint32_t glyphIndex
)
{
    if (cache.glyphCount >= cache.glyphs.size())
    {
        return false;
    }

    FT_Set_Pixel_Sizes(font.face, 0, pixelSize);
    if (FT_Load_Glyph(font.face, glyphIndex, kFtLoadFlags) != FT_Err_Ok)
    {
        return false;
    }
    if (FT_Render_Glyph(font.face->glyph, FT_RENDER_MODE_NORMAL) != FT_Err_Ok)
    {
        return false;
    }

    const FT_GlyphSlot slot = font.face->glyph;
    const std::uint32_t glyphWidth = slot->bitmap.width;
    const std::uint32_t glyphHeight = slot->bitmap.rows;

    CachedGlyph& entry = cache.glyphs[cache.glyphCount++];
    entry = {};
    entry.active = true;
    entry.font = font.id;
    entry.pixelSize = pixelSize;
    entry.glyphIndex = glyphIndex;
    entry.width = glyphWidth;
    entry.height = glyphHeight;
    entry.bearingX = slot->bitmap_left;
    entry.bearingY = slot->bitmap_top;

    if (glyphWidth == 0 || glyphHeight == 0)
    {
        return true;
    }

    std::uint32_t atlasPage = 0;
    std::uint32_t atlasX = 0;
    std::uint32_t atlasY = 0;
    if (!PackGlyph(cache, font.id, pixelSize, glyphWidth, glyphHeight, atlasPage, atlasX, atlasY))
    {
        entry.active = false;
        --cache.glyphCount;
        return false;
    }

    entry.atlasPage = atlasPage;
    entry.x = atlasX;
    entry.y = atlasY;
    BlitBitmapToPage(cache.pages[atlasPage], atlasX, atlasY, slot->bitmap);
    return true;
}
}

void InitializeAtlasCache(AtlasCache& cache)
{
    cache = {};
}

const CachedGlyph* FindCachedGlyph(
    const AtlasCache& cache,
    FontId font,
    std::uint32_t pixelSize,
    std::uint32_t glyphIndex
)
{
    for (const CachedGlyph& glyph : cache.glyphs)
    {
        if (!glyph.active)
        {
            continue;
        }
        if (glyph.font == font && glyph.pixelSize == pixelSize && glyph.glyphIndex == glyphIndex)
        {
            return &glyph;
        }
    }
    return nullptr;
}

bool CacheShapedGlyphs(
    AtlasCache& cache,
    const FrameState& frame,
    const FontRegistry& fonts
)
{
    for (std::uint32_t runIndex = 0; runIndex < frame.shapedRunCount; ++runIndex)
    {
        const ShapedRun& run = frame.shapedRuns[runIndex];
        if (!run.active)
        {
            continue;
        }

        const RegisteredFont* font = FindFontById(fonts, run.font);
        if (font == nullptr || font->face == nullptr)
        {
            continue;
        }

        for (std::uint32_t glyphOffset = 0; glyphOffset < run.glyphCount; ++glyphOffset)
        {
            const ShapedGlyph& glyph = frame.shapedGlyphs[run.firstGlyph + glyphOffset];
            if (!glyph.active)
            {
                continue;
            }
            if (FindCachedGlyph(cache, run.font, run.rasterPixelSize, glyph.glyphIndex) != nullptr)
            {
                continue;
            }
            if (!CacheGlyph(cache, *font, run.rasterPixelSize, glyph.glyphIndex))
            {
                return false;
            }
        }
    }

    return true;
}

AtlasStats GetAtlasStats(const AtlasCache& cache)
{
    AtlasStats stats{};
    stats.pageCount = cache.pageCount;
    stats.glyphCount = cache.glyphCount;
    for (std::uint32_t pageIndex = 0; pageIndex < cache.pageCount; ++pageIndex)
    {
        stats.memoryBytes += cache.pages[pageIndex].pixels.size();
    }
    return stats;
}

void ClearAtlasCache(AtlasCache& cache)
{
    cache = {};
}

void ClearAtlasCacheForFont(AtlasCache& cache, FontId font)
{
    AtlasCache filtered{};
    std::array<std::uint32_t, kMaxAtlasPages> pageRemap{};
    pageRemap.fill(kMaxAtlasPages);

    for (std::uint32_t oldPageIndex = 0; oldPageIndex < cache.pageCount; ++oldPageIndex)
    {
        const AtlasPage& page = cache.pages[oldPageIndex];
        if (!page.active || page.font == font)
        {
            continue;
        }
        if (filtered.pageCount >= filtered.pages.size())
        {
            break;
        }

        const std::uint32_t newPageIndex = filtered.pageCount++;
        filtered.pages[newPageIndex] = page;
        filtered.pages[newPageIndex].dirty = true;
        pageRemap[oldPageIndex] = newPageIndex;
    }

    for (std::uint32_t glyphIndex = 0; glyphIndex < cache.glyphCount; ++glyphIndex)
    {
        const CachedGlyph& glyph = cache.glyphs[glyphIndex];
        if (!glyph.active || glyph.font == font)
        {
            continue;
        }
        if (filtered.glyphCount >= filtered.glyphs.size())
        {
            break;
        }

        CachedGlyph copy = glyph;
        if (copy.width > 0 && copy.height > 0)
        {
            const std::uint32_t remappedPage = pageRemap[copy.atlasPage];
            if (remappedPage >= filtered.pageCount)
            {
                continue;
            }
            copy.atlasPage = remappedPage;
        }
        filtered.glyphs[filtered.glyphCount++] = copy;
    }

    cache = std::move(filtered);
}

void ClearAtlasDirtyFlags(AtlasCache& cache)
{
    for (AtlasPage& page : cache.pages)
    {
        page.dirty = false;
    }
}
}
