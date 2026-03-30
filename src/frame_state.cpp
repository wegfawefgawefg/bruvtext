#include "frame_state.h"

namespace bruvtext
{
namespace
{
constexpr std::size_t kDefaultQueuedTextReserve = 512;
constexpr std::size_t kDefaultShapedRunReserve = 512;
constexpr std::size_t kDefaultShapedGlyphReserve = 16384;
constexpr std::size_t kDefaultDrawGlyphReserve = kDefaultShapedGlyphReserve;
constexpr std::size_t kDefaultDrawBatchReserve = 1024;
}

void BeginFrame(FrameState& frame)
{
    if (frame.queuedText.capacity() < kDefaultQueuedTextReserve)
    {
        frame.queuedText.reserve(kDefaultQueuedTextReserve);
    }
    if (frame.shapedRuns.capacity() < kDefaultShapedRunReserve)
    {
        frame.shapedRuns.reserve(kDefaultShapedRunReserve);
    }
    if (frame.shapedGlyphs.capacity() < kDefaultShapedGlyphReserve)
    {
        frame.shapedGlyphs.reserve(kDefaultShapedGlyphReserve);
    }
    if (frame.drawGlyphs.capacity() < kDefaultDrawGlyphReserve)
    {
        frame.drawGlyphs.reserve(kDefaultDrawGlyphReserve);
    }
    if (frame.drawBatches.capacity() < kDefaultDrawBatchReserve)
    {
        frame.drawBatches.reserve(kDefaultDrawBatchReserve);
    }

    frame.queuedText.clear();
    frame.shapedRuns.clear();
    frame.shapedGlyphs.clear();
    frame.drawGlyphs.clear();
    frame.drawBatches.clear();
}

bool QueueText(FrameState& frame, const DrawTextCmd& cmd)
{
    if (cmd.font == 0 || cmd.text.empty())
    {
        return false;
    }

    frame.queuedText.emplace_back();
    QueuedText& item = frame.queuedText.back();
    item.active = true;
    item.font = cmd.font;
    item.text = std::string(cmd.text);
    item.position = cmd.position;
    item.pixelSize = cmd.pixelSize;
    item.scale = cmd.scale;
    item.color = cmd.color;
    return true;
}
}
