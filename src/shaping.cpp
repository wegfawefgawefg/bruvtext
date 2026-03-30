#include "shaping.h"

#include <algorithm>
#include <cmath>

#include <harfbuzz/hb.h>
#include <harfbuzz/hb-ft.h>

namespace bruvtext
{
namespace
{
std::uint32_t ClampPixelSize(float size)
{
    const float rounded = std::round(size);
    const float clamped = std::clamp(rounded, 8.0f, 128.0f);
    return static_cast<std::uint32_t>(clamped);
}

void AppendCachedRun(
    FrameState& frame,
    const QueuedText& item,
    std::uint32_t rasterPixelSize,
    const CachedShapedRun& cached)
{
    const std::uint32_t firstGlyph = static_cast<std::uint32_t>(frame.shapedGlyphs.size());
    frame.shapedRuns.emplace_back();
    ShapedRun& run = frame.shapedRuns.back();
    run.active = true;
    run.font = item.font;
    run.text = item.text;
    run.position = item.position;
    run.pixelSize = item.pixelSize;
    run.scale = item.scale;
    run.color = item.color;
    run.rasterPixelSize = rasterPixelSize;
    run.firstGlyph = firstGlyph;
    run.glyphCount = static_cast<std::uint32_t>(cached.glyphs.size());

    frame.shapedGlyphs.reserve(frame.shapedGlyphs.size() + cached.glyphs.size());
    for (const ShapedGlyph& cachedGlyph : cached.glyphs)
    {
        frame.shapedGlyphs.push_back(cachedGlyph);
        ShapedGlyph& out = frame.shapedGlyphs.back();
        out.active = true;
        out.x += item.position.x;
        out.y += item.position.y;
    }
}
}

bool ShapeQueuedText(
    FrameState& frame,
    const FontRegistry& fonts,
    ShapeCache& cache)
{
    frame.shapedRuns.clear();
    frame.shapedGlyphs.clear();
    frame.shapedRuns.reserve(frame.queuedText.size());

    for (const QueuedText& item : frame.queuedText)
    {
        if (!item.active)
        {
            continue;
        }

        const RegisteredFont* font = FindFontById(fonts, item.font);
        if (font == nullptr || font->face == nullptr || font->hbFont == nullptr)
        {
            continue;
        }

        const std::uint32_t rasterPixelSize = ClampPixelSize(item.pixelSize);
        if (const CachedShapedRun* cached =
                FindCachedShapedRun(cache, item.font, rasterPixelSize, item.text))
        {
            AppendCachedRun(frame, item, rasterPixelSize, *cached);
            continue;
        }

        FT_Set_Pixel_Sizes(font->face, 0, rasterPixelSize);
        hb_ft_font_changed(font->hbFont);
        hb_ft_font_set_load_flags(font->hbFont, kFtLoadFlags);
        hb_font_set_scale(
            font->hbFont,
            static_cast<int>(rasterPixelSize * 64),
            static_cast<int>(rasterPixelSize * 64)
        );
        hb_font_set_ppem(font->hbFont, rasterPixelSize, rasterPixelSize);

        hb_buffer_t* buffer = hb_buffer_create();
        if (buffer == nullptr)
        {
            return false;
        }

        hb_buffer_add_utf8(buffer, item.text.c_str(), static_cast<int>(item.text.size()), 0, static_cast<int>(item.text.size()));
        hb_buffer_guess_segment_properties(buffer);
        hb_shape(font->hbFont, buffer, nullptr, 0);

        unsigned int glyphCount = 0;
        hb_glyph_info_t* glyphInfo = hb_buffer_get_glyph_infos(buffer, &glyphCount);
        hb_glyph_position_t* glyphPos = hb_buffer_get_glyph_positions(buffer, &glyphCount);

        CachedShapedRun cachedRun{};
        cachedRun.font = item.font;
        cachedRun.rasterPixelSize = rasterPixelSize;
        cachedRun.text = item.text;
        cachedRun.glyphs.reserve(glyphCount);

        float penX = 0.0f;
        float penY = 0.0f;
        for (unsigned int glyphIndex = 0; glyphIndex < glyphCount; ++glyphIndex)
        {
            cachedRun.glyphs.emplace_back();
            ShapedGlyph& out = cachedRun.glyphs.back();
            out.active = true;
            out.glyphIndex = glyphInfo[glyphIndex].codepoint;
            out.cluster = glyphInfo[glyphIndex].cluster;
            out.x = penX + static_cast<float>(glyphPos[glyphIndex].x_offset) / 64.0f;
            out.y = penY - static_cast<float>(glyphPos[glyphIndex].y_offset) / 64.0f;
            out.advanceX = static_cast<float>(glyphPos[glyphIndex].x_advance) / 64.0f;
            out.advanceY = static_cast<float>(glyphPos[glyphIndex].y_advance) / 64.0f;

            if (FT_Load_Glyph(font->face, out.glyphIndex, kFtLoadFlags) != FT_Err_Ok)
            {
                hb_buffer_destroy(buffer);
                return false;
            }
            if (FT_Render_Glyph(font->face->glyph, FT_RENDER_MODE_NORMAL) != FT_Err_Ok)
            {
                hb_buffer_destroy(buffer);
                return false;
            }

            const FT_GlyphSlot slot = font->face->glyph;
            out.bearingX = slot->bitmap_left;
            out.bearingY = slot->bitmap_top;
            out.width = slot->bitmap.width;
            out.height = slot->bitmap.rows;

            penX += out.advanceX;
            penY -= out.advanceY;
        }

        hb_buffer_destroy(buffer);
        const CachedShapedRun& stored =
            StoreCachedShapedRun(cache, std::move(cachedRun));
        AppendCachedRun(frame, item, rasterPixelSize, stored);
    }

    return true;
}
}
