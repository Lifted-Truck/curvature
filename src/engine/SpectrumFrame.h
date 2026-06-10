// The geometry->audio contract. Fixed-size POD: lives in preallocated FIFO
// slots, memcpy'd into voices at note-on. Frequencies are stored as ratios
// to the fundamental (f_k = noteHz * ratio_k), so MIDI transposition is an
// audio-thread multiply and the whole spectrum transposes rigidly.
#pragma once

#include <cstdint>

namespace curv {

constexpr int kMaxModes = 128;

struct SpectrumFrame {
    int numModes = 0;
    uint32_t frameId = 0;
    float ratio[kMaxModes] = {};     // sqrt(lambda_k / lambda_1), ratio[0] == 1
    float coupling[kMaxModes] = {};  // phi_k(strikeVertex), peak-normalized, signed
};

} // namespace curv
