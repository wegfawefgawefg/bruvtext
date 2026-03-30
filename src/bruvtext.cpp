#include "bruvtext/bruvtext.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "atlas_cache.h"
#include "context.h"
#include "draw_list.h"
#include "frame_state.h"
#include "font_registry.h"
#include "shaping.h"

namespace bruvtext
{
namespace
{
constexpr Version kVersion{0, 1, 0};

constexpr std::array<DemoTextSample, kBuiltInDemoSampleCount> kDemoTextSamples{{
    {"english", "assets/text/english.txt"},
    {"french", "assets/text/french.txt"},
    {"spanish", "assets/text/spanish.txt"},
    {"german", "assets/text/german.txt"},
    {"italian", "assets/text/italian.txt"},
    {"portuguese", "assets/text/portuguese.txt"},
    {"dutch", "assets/text/dutch.txt"},
    {"vietnamese", "assets/text/vietnamese.txt"},
    {"polish", "assets/text/polish.txt"},
    {"turkish", "assets/text/turkish.txt"},
    {"romanian", "assets/text/romanian.txt"},
    {"czech", "assets/text/czech.txt"},
    {"swedish", "assets/text/swedish.txt"},
    {"japanese", "assets/text/japanese.txt"},
    {"simplified_chinese", "assets/text/simplified_chinese.txt"},
    {"traditional_chinese", "assets/text/traditional_chinese.txt"},
    {"korean", "assets/text/korean.txt"},
    {"arabic", "assets/text/arabic.txt"},
    {"hindi", "assets/text/hindi.txt"},
}};

struct ContextImpl : Context
{
    FontRegistry fonts;
    FrameState frame;
    AtlasCache atlas;
    std::array<QueuedTextView, kMaxQueuedTextItems> queuedTextView = {};
    std::array<ShapedGlyphView, kMaxShapedGlyphs> shapedGlyphView = {};
    std::array<ShapedRunView, kMaxShapedRuns> shapedRunView = {};
    std::array<AtlasPageView, kMaxAtlasPages> atlasPageView = {};
    std::array<DrawGlyphView, kMaxDrawGlyphs> drawGlyphView = {};
    std::array<DrawBatchView, kMaxDrawBatches> drawBatchView = {};
};

TextSize MeasureQueuedText(FrameState& frame)
{
    TextSize result{};
    if (frame.shapedRunCount == 0)
    {
        return result;
    }

    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
    bool anyGlyph = false;

    for (std::uint32_t runIndex = 0; runIndex < frame.shapedRunCount; ++runIndex)
    {
        const ShapedRun& run = frame.shapedRuns[runIndex];
        if (!run.active)
        {
            continue;
        }

        const float scale = std::max(run.scale, 0.0f);
        for (std::uint32_t glyphOffset = 0; glyphOffset < run.glyphCount; ++glyphOffset)
        {
            const ShapedGlyph& glyph = frame.shapedGlyphs[run.firstGlyph + glyphOffset];
            if (!glyph.active)
            {
                continue;
            }

            const float x0 = run.position.x +
                (glyph.x + static_cast<float>(glyph.bearingX) - run.position.x) * scale;
            const float y0 = run.position.y +
                (glyph.y - static_cast<float>(glyph.bearingY) - run.position.y) * scale;
            const float x1 = x0 + static_cast<float>(glyph.width) * scale;
            const float y1 = y0 + static_cast<float>(glyph.height) * scale;

            if (!anyGlyph)
            {
                minX = x0;
                minY = y0;
                maxX = x1;
                maxY = y1;
                anyGlyph = true;
                continue;
            }

            minX = std::min(minX, x0);
            minY = std::min(minY, y0);
            maxX = std::max(maxX, x1);
            maxY = std::max(maxY, y1);
        }
    }

    if (!anyGlyph)
    {
        return result;
    }

    result.width = std::max(0.0f, maxX - minX);
    result.height = std::max(0.0f, maxY - minY);
    return result;
}
}

Version GetVersion()
{
    return kVersion;
}

const char* GetBuildInfo()
{
    return "bruvtext scaffold: FreeType + HarfBuzz wired, lazy atlas cache started";
}

std::size_t GetBuiltInDemoTextSampleCount()
{
    return kDemoTextSamples.size();
}

const DemoTextSample* GetBuiltInDemoTextSamples()
{
    return kDemoTextSamples.data();
}

Context* CreateContext(const CreateInfo& createInfo)
{
    auto* context = new ContextImpl();
    context->assetsRoot = std::string(createInfo.assetsRoot);
    context->assetsRootPath = std::filesystem::path(context->assetsRoot);
    if (FT_Init_FreeType(&context->ftLibrary) != FT_Err_Ok)
    {
        delete context;
        return nullptr;
    }
    InitializeAtlasCache(context->atlas);
    return context;
}

void DestroyContext(Context* context)
{
    if (context == nullptr)
    {
        return;
    }
    auto* impl = static_cast<ContextImpl*>(context);
    ShutdownFontRegistry(impl->fonts);
    if (impl->ftLibrary != nullptr)
    {
        FT_Done_FreeType(impl->ftLibrary);
        impl->ftLibrary = nullptr;
    }
    delete impl;
}

FontId RegisterFont(Context& context, const FontDesc& desc)
{
    auto& impl = static_cast<ContextImpl&>(context);
    return bruvtext::RegisterFont(impl.fonts, impl.ftLibrary, impl.assetsRootPath, desc);
}

void ClearAtlasCache(Context& context)
{
    auto& impl = static_cast<ContextImpl&>(context);
    bruvtext::ClearAtlasCache(impl.atlas);
}

void ClearAtlasCacheForFont(Context& context, FontId font)
{
    auto& impl = static_cast<ContextImpl&>(context);
    bruvtext::ClearAtlasCacheForFont(impl.atlas, font);
}

void BeginFrame(Context& context)
{
    auto& impl = static_cast<ContextImpl&>(context);
    bruvtext::BeginFrame(impl.frame);
}

bool DrawText(
    Context& context,
    FontId font,
    std::string_view text,
    float x,
    float y,
    float pixelSize,
    Color color)
{
    return DrawTextEx(context, DrawTextCmd{
        .font = font,
        .text = text,
        .position = {x, y},
        .pixelSize = pixelSize,
        .scale = 1.0f,
        .color = color,
    });
}

bool DrawTextEx(Context& context, const DrawTextCmd& cmd)
{
    auto& impl = static_cast<ContextImpl&>(context);
    if (FindFontById(impl.fonts, cmd.font) == nullptr)
    {
        return false;
    }
    return QueueText(impl.frame, cmd);
}

TextSize MeasureText(Context& context, FontId font, std::string_view text, float pixelSize)
{
    return MeasureTextEx(context, font, text, pixelSize, 1.0f);
}

TextSize MeasureTextEx(Context& context, FontId font, std::string_view text, float pixelSize, float scale)
{
    auto& impl = static_cast<ContextImpl&>(context);
    if (FindFontById(impl.fonts, font) == nullptr)
    {
        return {};
    }

    FrameState frame{};
    if (!QueueText(frame, DrawTextCmd{
            .font = font,
            .text = text,
            .position = {},
            .pixelSize = pixelSize,
            .scale = scale,
        }))
    {
        return {};
    }
    if (!ShapeQueuedText(frame, impl.fonts))
    {
        return {};
    }
    return MeasureQueuedText(frame);
}

float MeasureLineAdvance(Context& context, FontId font, float pixelSize, float scale)
{
    auto& impl = static_cast<ContextImpl&>(context);
    const RegisteredFont* registered = FindFontById(impl.fonts, font);
    if (registered == nullptr || registered->face == nullptr)
    {
        return 0.0f;
    }

    const std::uint32_t rasterPixelSize = static_cast<std::uint32_t>(
        std::clamp(std::round(pixelSize), 8.0f, 128.0f));
    if (FT_Set_Pixel_Sizes(registered->face, 0, rasterPixelSize) != FT_Err_Ok)
    {
        return 0.0f;
    }

    const float lineAdvance =
        static_cast<float>(registered->face->size->metrics.height) / 64.0f;
    return std::max(scale, 0.0f) * lineAdvance;
}

bool EndFrame(Context& context)
{
    auto& impl = static_cast<ContextImpl&>(context);
    if (!ShapeQueuedText(impl.frame, impl.fonts))
    {
        return false;
    }
    if (!CacheShapedGlyphs(impl.atlas, impl.frame, impl.fonts))
    {
        return false;
    }
    return BuildDrawGlyphs(impl.frame, impl.atlas);
}

std::size_t GetQueuedTextCount(const Context& context)
{
    const auto& impl = static_cast<const ContextImpl&>(context);
    return impl.frame.queuedTextCount;
}

const QueuedTextView* GetQueuedTextItems(const Context& context)
{
    auto& impl = const_cast<ContextImpl&>(static_cast<const ContextImpl&>(context));
    for (std::uint32_t i = 0; i < impl.frame.queuedTextCount; ++i)
    {
        const QueuedText& source = impl.frame.queuedText[i];
        QueuedTextView& view = impl.queuedTextView[i];
        view.font = source.font;
        view.text = source.text.c_str();
        view.position = source.position;
        view.pixelSize = source.pixelSize;
        view.scale = source.scale;
        view.color = source.color;
    }
    return impl.queuedTextView.data();
}

std::size_t GetShapedRunCount(const Context& context)
{
    const auto& impl = static_cast<const ContextImpl&>(context);
    return impl.frame.shapedRunCount;
}

const ShapedRunView* GetShapedRuns(const Context& context)
{
    auto& impl = const_cast<ContextImpl&>(static_cast<const ContextImpl&>(context));
    for (std::uint32_t i = 0; i < impl.frame.shapedGlyphCount; ++i)
    {
        const ShapedGlyph& source = impl.frame.shapedGlyphs[i];
        ShapedGlyphView& view = impl.shapedGlyphView[i];
        view.glyphIndex = source.glyphIndex;
        view.cluster = source.cluster;
        view.x = source.x;
        view.y = source.y;
        view.advanceX = source.advanceX;
        view.advanceY = source.advanceY;
    }
    for (std::uint32_t i = 0; i < impl.frame.shapedRunCount; ++i)
    {
        const ShapedRun& source = impl.frame.shapedRuns[i];
        ShapedRunView& view = impl.shapedRunView[i];
        view.font = source.font;
        view.text = source.text.c_str();
        view.position = source.position;
        view.pixelSize = source.pixelSize;
        view.scale = source.scale;
        view.color = source.color;
        view.glyphs = impl.shapedGlyphView.data() + source.firstGlyph;
        view.glyphCount = source.glyphCount;
    }
    return impl.shapedRunView.data();
}

AtlasStats GetAtlasStats(const Context& context)
{
    const auto& impl = static_cast<const ContextImpl&>(context);
    return bruvtext::GetAtlasStats(impl.atlas);
}

std::size_t GetAtlasPageCount(const Context& context)
{
    const auto& impl = static_cast<const ContextImpl&>(context);
    return impl.atlas.pageCount;
}

const AtlasPageView* GetAtlasPages(const Context& context)
{
    auto& impl = const_cast<ContextImpl&>(static_cast<const ContextImpl&>(context));
    for (std::uint32_t i = 0; i < impl.atlas.pageCount; ++i)
    {
        const AtlasPage& source = impl.atlas.pages[i];
        AtlasPageView& view = impl.atlasPageView[i];
        view.pageIndex = i;
        view.font = source.font;
        view.pixelSize = source.pixelSize;
        view.width = source.width;
        view.height = source.height;
        view.rowPitch = source.width * 4;
        view.dirty = source.dirty;
        view.pixels = source.pixels.data();
    }
    return impl.atlasPageView.data();
}

void ClearAtlasDirtyFlags(Context& context)
{
    auto& impl = static_cast<ContextImpl&>(context);
    bruvtext::ClearAtlasDirtyFlags(impl.atlas);
}

std::size_t GetDrawGlyphCount(const Context& context)
{
    const auto& impl = static_cast<const ContextImpl&>(context);
    return impl.frame.drawGlyphCount;
}

const DrawGlyphView* GetDrawGlyphs(const Context& context)
{
    auto& impl = const_cast<ContextImpl&>(static_cast<const ContextImpl&>(context));
    for (std::uint32_t i = 0; i < impl.frame.drawGlyphCount; ++i)
    {
        const DrawGlyph& source = impl.frame.drawGlyphs[i];
        DrawGlyphView& view = impl.drawGlyphView[i];
        view.atlasPage = source.atlasPage;
        view.glyphIndex = source.glyphIndex;
        view.atlasX = source.atlasX;
        view.atlasY = source.atlasY;
        view.atlasWidth = source.atlasWidth;
        view.atlasHeight = source.atlasHeight;
        view.x0 = source.x0;
        view.y0 = source.y0;
        view.x1 = source.x1;
        view.y1 = source.y1;
        view.u0 = source.u0;
        view.v0 = source.v0;
        view.u1 = source.u1;
        view.v1 = source.v1;
        view.color = source.color;
    }
    return impl.drawGlyphView.data();
}

std::size_t GetDrawBatchCount(const Context& context)
{
    const auto& impl = static_cast<const ContextImpl&>(context);
    return impl.frame.drawBatchCount;
}

const DrawBatchView* GetDrawBatches(const Context& context)
{
    auto& impl = const_cast<ContextImpl&>(static_cast<const ContextImpl&>(context));
    for (std::uint32_t i = 0; i < impl.frame.drawBatchCount; ++i)
    {
        const DrawBatch& source = impl.frame.drawBatches[i];
        DrawBatchView& view = impl.drawBatchView[i];
        view.atlasPage = source.atlasPage;
        view.firstGlyph = source.firstGlyph;
        view.glyphCount = source.glyphCount;
    }
    return impl.drawBatchView.data();
}
}
