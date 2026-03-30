#pragma once

#include "atlas_cache.h"
#include "font_registry.h"
#include "frame_state.h"

namespace bruvtext
{
bool BuildDrawGlyphs(FrameState& frame, const AtlasCache& cache);
}
