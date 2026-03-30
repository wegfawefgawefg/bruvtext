#include "frame_state.h"

namespace bruvtext
{
void BeginFrame(FrameState& frame)
{
    frame.queuedTextCount = 0;
    frame.shapedRunCount = 0;
    frame.shapedGlyphCount = 0;
    frame.drawGlyphCount = 0;
    frame.drawBatchCount = 0;
    for (QueuedText& item : frame.queuedText)
    {
        item = {};
    }
}

bool QueueText(FrameState& frame, const DrawTextCmd& cmd)
{
    if (frame.queuedTextCount >= frame.queuedText.size())
    {
        return false;
    }
    if (cmd.font == 0 || cmd.text.empty())
    {
        return false;
    }

    QueuedText& item = frame.queuedText[frame.queuedTextCount++];
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
