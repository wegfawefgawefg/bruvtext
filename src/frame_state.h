#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "bruvtext/bruvtext.h"

namespace bruvtext
{
constexpr std::uint32_t kMaxQueuedTextItems = 256;
constexpr std::uint32_t kMaxShapedRuns = 256;
constexpr std::uint32_t kMaxShapedGlyphs = 8192;
constexpr std::uint32_t kMaxDrawGlyphs = kMaxShapedGlyphs;
constexpr std::uint32_t kMaxDrawBatches = 512;

struct QueuedText
{
    bool active = false;
    FontId font = 0;
    std::string text;
    float x = 0.0f;
    float y = 0.0f;
    float pixelSize = 16.0f;
    float scale = 1.0f;
    float colorR = 1.0f;
    float colorG = 1.0f;
    float colorB = 1.0f;
    float colorA = 1.0f;
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
    float x = 0.0f;
    float y = 0.0f;
    float pixelSize = 16.0f;
    float scale = 1.0f;
    float colorR = 1.0f;
    float colorG = 1.0f;
    float colorB = 1.0f;
    float colorA = 1.0f;
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
    float colorR = 1.0f;
    float colorG = 1.0f;
    float colorB = 1.0f;
    float colorA = 1.0f;
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
    std::array<QueuedText, kMaxQueuedTextItems> queuedText = {};
    std::uint32_t queuedTextCount = 0;
    std::array<ShapedRun, kMaxShapedRuns> shapedRuns = {};
    std::uint32_t shapedRunCount = 0;
    std::array<ShapedGlyph, kMaxShapedGlyphs> shapedGlyphs = {};
    std::uint32_t shapedGlyphCount = 0;
    std::array<DrawGlyph, kMaxDrawGlyphs> drawGlyphs = {};
    std::uint32_t drawGlyphCount = 0;
    std::array<DrawBatch, kMaxDrawBatches> drawBatches = {};
    std::uint32_t drawBatchCount = 0;
};

void BeginFrame(FrameState& frame);
bool QueueText(FrameState& frame, const DrawTextCmd& cmd);
}
