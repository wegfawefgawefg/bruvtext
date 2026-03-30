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
}

bool ShapeQueuedText(
    FrameState& frame,
    const FontRegistry& fonts)
{
    frame.shapedRunCount = 0;
    frame.shapedGlyphCount = 0;

    for (std::uint32_t i = 0; i < frame.queuedTextCount; ++i)
    {
        const QueuedText& item = frame.queuedText[i];
        if (!item.active)
        {
            continue;
        }

        const RegisteredFont* font = FindFontById(fonts, item.font);
        if (font == nullptr || font->face == nullptr || font->hbFont == nullptr)
        {
            continue;
        }

        if (frame.shapedRunCount >= frame.shapedRuns.size())
        {
            return false;
        }

        const std::uint32_t rasterPixelSize = ClampPixelSize(item.pixelSize);
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

        if (frame.shapedGlyphCount + glyphCount > frame.shapedGlyphs.size())
        {
            hb_buffer_destroy(buffer);
            return false;
        }

        ShapedRun& run = frame.shapedRuns[frame.shapedRunCount++];
        run.active = true;
        run.font = item.font;
        run.text = item.text;
        run.x = item.x;
        run.y = item.y;
        run.pixelSize = item.pixelSize;
        run.scale = item.scale;
        run.colorR = item.colorR;
        run.colorG = item.colorG;
        run.colorB = item.colorB;
        run.colorA = item.colorA;
        run.rasterPixelSize = rasterPixelSize;
        run.firstGlyph = frame.shapedGlyphCount;
        run.glyphCount = glyphCount;

        float penX = item.x;
        float penY = item.y;
        for (unsigned int glyphIndex = 0; glyphIndex < glyphCount; ++glyphIndex)
        {
            ShapedGlyph& out = frame.shapedGlyphs[frame.shapedGlyphCount++];
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
    }

    return true;
}
}
