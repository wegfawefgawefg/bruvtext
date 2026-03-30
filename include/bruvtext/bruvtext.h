#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace bruvtext
{
using FontId = std::uint32_t;

struct Vec2
{
    float x = 0.0f;
    float y = 0.0f;
};

struct Color
{
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct Version
{
    std::uint32_t major = 0;
    std::uint32_t minor = 0;
    std::uint32_t patch = 0;
};

struct FontDesc
{
    std::string_view debugName = {};
    std::string_view filePath = {};
};

struct CreateInfo
{
    std::string_view assetsRoot = {};
};

struct DrawTextCmd
{
    FontId font = 0;
    std::string_view text = {};
    Vec2 position = {};
    float pixelSize = 16.0f;
    float scale = 1.0f;
    Color color = {};
};

struct QueuedTextView
{
    FontId font = 0;
    const char* text = nullptr;
    Vec2 position = {};
    float pixelSize = 16.0f;
    float scale = 1.0f;
    Color color = {};
};

struct ShapedGlyphView
{
    std::uint32_t glyphIndex = 0;
    std::uint32_t cluster = 0;
    float x = 0.0f;
    float y = 0.0f;
    float advanceX = 0.0f;
    float advanceY = 0.0f;
};

struct ShapedRunView
{
    FontId font = 0;
    const char* text = nullptr;
    Vec2 position = {};
    float pixelSize = 16.0f;
    float scale = 1.0f;
    Color color = {};
    const ShapedGlyphView* glyphs = nullptr;
    std::size_t glyphCount = 0;
};

struct AtlasStats
{
    std::uint32_t pageCount = 0;
    std::uint32_t glyphCount = 0;
    std::uint64_t memoryBytes = 0;
};

struct TextSize
{
    float width = 0.0f;
    float height = 0.0f;
};

struct AtlasPageView
{
    std::uint32_t pageIndex = 0;
    FontId font = 0;
    std::uint32_t pixelSize = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t rowPitch = 0;
    bool dirty = false;
    const std::uint8_t* pixels = nullptr;
};

struct DrawGlyphView
{
    std::uint32_t atlasPage = 0;
    std::uint32_t glyphIndex = 0;
    std::uint32_t atlasX = 0;
    std::uint32_t atlasY = 0;
    std::uint32_t atlasWidth = 0;
    std::uint32_t atlasHeight = 0;
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 0.0f;
    float v1 = 0.0f;
    Color color = {};
};

struct DrawBatchView
{
    std::uint32_t atlasPage = 0;
    std::uint32_t firstGlyph = 0;
    std::uint32_t glyphCount = 0;
};

struct DemoTextSample
{
    std::string_view language = {};
    std::string_view filePath = {};
};

constexpr std::size_t kBuiltInDemoSampleCount = 19;

struct Context;

Version GetVersion();
const char* GetBuildInfo();
std::size_t GetBuiltInDemoTextSampleCount();
const DemoTextSample* GetBuiltInDemoTextSamples();

Context* CreateContext(const CreateInfo& createInfo);
void DestroyContext(Context* context);
FontId RegisterFont(Context& context, const FontDesc& desc);
void ClearAtlasCache(Context& context);
void ClearAtlasCacheForFont(Context& context, FontId font);
void BeginFrame(Context& context);
bool DrawText(
    Context& context,
    FontId font,
    std::string_view text,
    float x,
    float y,
    float pixelSize,
    Color color = {});
bool DrawTextEx(Context& context, const DrawTextCmd& cmd);
TextSize MeasureText(Context& context, FontId font, std::string_view text, float pixelSize);
TextSize MeasureTextEx(Context& context, FontId font, std::string_view text, float pixelSize, float scale);
float MeasureLineAdvance(Context& context, FontId font, float pixelSize, float scale = 1.0f);
bool EndFrame(Context& context);
std::size_t GetQueuedTextCount(const Context& context);
const QueuedTextView* GetQueuedTextItems(const Context& context);
std::size_t GetShapedRunCount(const Context& context);
const ShapedRunView* GetShapedRuns(const Context& context);
AtlasStats GetAtlasStats(const Context& context);
std::size_t GetAtlasPageCount(const Context& context);
const AtlasPageView* GetAtlasPages(const Context& context);
void ClearAtlasDirtyFlags(Context& context);
std::size_t GetDrawGlyphCount(const Context& context);
const DrawGlyphView* GetDrawGlyphs(const Context& context);
std::size_t GetDrawBatchCount(const Context& context);
const DrawBatchView* GetDrawBatches(const Context& context);
}
