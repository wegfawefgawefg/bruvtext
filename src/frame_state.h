#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "bruvtext/bruvtext.h"

namespace bruvtext
{
struct QueuedText
{
    bool active = false;
    FontId font = 0;
    std::string text;
    Vec2 position = {};
    float pixelSize = 16.0f;
    float scale = 1.0f;
    Color color = {};
};

struct ShapedGlyph
{
    bool active = false;
    std::uint32_t glyphIndex = 0;
    std::uint32_t cluster = 0;
    float x = 0.0f;
    float y = 0.0f;
    float advanceX = 0.0f;
    float advanceY = 0.0f;
    std::int32_t bearingX = 0;
    std::int32_t bearingY = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct ShapedRun
{
    bool active = false;
    FontId font = 0;
    std::string text;
    Vec2 position = {};
    float pixelSize = 16.0f;
    float scale = 1.0f;
    Color color = {};
    std::uint32_t rasterPixelSize = 0;
    std::uint32_t firstGlyph = 0;
    std::uint32_t glyphCount = 0;
};

struct DrawGlyph
{
    bool active = false;
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

struct DrawBatch
{
    bool active = false;
    std::uint32_t atlasPage = 0;
    std::uint32_t firstGlyph = 0;
    std::uint32_t glyphCount = 0;
};

struct FrameState
{
    std::vector<QueuedText> queuedText = {};
    std::vector<ShapedRun> shapedRuns = {};
    std::vector<ShapedGlyph> shapedGlyphs = {};
    std::vector<DrawGlyph> drawGlyphs = {};
    std::vector<DrawBatch> drawBatches = {};
};

void BeginFrame(FrameState& frame);
bool QueueText(FrameState& frame, const DrawTextCmd& cmd);
}
