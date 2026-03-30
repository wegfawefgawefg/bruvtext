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
    item.x = cmd.x;
    item.y = cmd.y;
    item.pixelSize = cmd.pixelSize;
    item.scale = cmd.scale;
    item.colorR = cmd.colorR;
    item.colorG = cmd.colorG;
    item.colorB = cmd.colorB;
    item.colorA = cmd.colorA;
    return true;
}
}
