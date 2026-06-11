// SpectrumBus: geometry thread -> audio thread, latest SpectrumFrame.
// Audio-thread rules (CLAUDE.md): readLatest() does no allocation, takes no
// locks, never blocks. If the producer has published nothing new, the
// consumer keeps its current frame — audio coasts on the last spectrum.
#pragma once

#include "SpectrumFrame.h"
#include "TripleBuffer.h"

namespace curv {

using SpectrumBus = TripleBuffer<SpectrumFrame>;

} // namespace curv
