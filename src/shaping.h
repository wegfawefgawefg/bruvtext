#pragma once

#include <cstdint>

#include "atlas_cache.h"
#include "frame_state.h"
#include "font_registry.h"

namespace bruvtext
{
bool ShapeQueuedText(
    FrameState& frame,
    const FontRegistry& fonts
);
}
