# Project status

**Phase 1 — COMPLETE (2026-06-10). Holding at CHECKPOINT 1: play it in Ableton.**

CHECKPOINT 0 passed earlier the same day (see git history for the Phase 0
verdict details): genus taxonomy audible, drift coherence confirmed, aesthetic
direction set toward implausible/"metaphysical" objects (SOPHIE reference) —
Julian found the first implausible demos promising but "not as alien as I was
imagining"; Phase 2 should push harder.

## What exists

- **C++ plugin** (JUCE 8.0.6 + CMake, VST3 + Standalone, macOS-first):
  - `src/geometry/` — intrinsic cotan Laplacian (edge lengths only),
    Spectra shift-invert eigensolver, presets ported 1:1 from Python
    (icosphere, flat lattice tori 1:1 / 1:1.618 / 8:1 harmonic, genus-2 OBJ
    via BinaryData).
  - `src/engine/` — `SpectrumFrame` (ratio + coupling tables), wait-free
    SPSC triple-buffer `SpectrumBus`, `GeometryService`, 8-voice
    `VoiceManager` with oldest/releasing stealing.
  - `src/dsp/ModalBank.h` — coupled-form (rotation) resonators, fixed-size
    state, linear coefficient ramps per 64-sample control interval.
  - `src/plugin/` — APVTS params (manifold, strike point, mode count 8–128,
    mallet, T60, tilt **(-1..2, negative = anti-acoustic)**, release, gain),
    geometry worker thread, generic editor.
  - The VST3 auto-installs to `~/Library/Audio/Plug-Ins/VST3/` on build.
- **`tools/render_offline`** — headless render through the real ModalVoice
  DSP; keeps spectral oracles alive (CLAUDE.md invariant).
- **Phase 0 Python rig** retained as reference oracle (`prototype/`).

## Phase 1 gates — all pass

- pluginval strictness 10: **SUCCESS**
- C++ eigenvalues vs Phase 0 Python dumps (`tests/data/spectra/`):
  tori ≤ 1e-8 rel, icosphere/genus2 ≤ 1e-6 rel
- FFT oracle on C++ render (`prototype/oracles_cpp.py`): 14 partials within
  1 cent, worst 0.000
- Catch2 suite (9 cases incl. plane-wave exactness, harmonic-series check,
  voice stability under tilt sweeps, SPSC torn-read stress): pass
- ASan+UBSan: tests + genus-2 render clean. TSan: SPSC stress clean.
- Audio-thread allocations: none by construction (fixed-size voice/frame
  state, wait-free bus, no locks/system calls in `processBlock`); verified
  by code audit, not yet by malloc-canary instrumentation.

## Known Phase 1 scope notes

- Voices snapshot the spectrum at note-on (snapshot mode); global-flow mode
  arrives with Phase 2 when frames actually evolve.
- MPE deferred to Phase 3 (decision: custom voice manager keeps door open).
- Pitch bend not yet wired.
- Geometry thread re-solves only on parameter change (static phase); the
  FIFO path is exercised exactly as the Phase 2 architecture requires.

## CHECKPOINT 1 — what Julian checks (in Ableton Live 12)

1. Plugin loads, plays, automates, and saves/restores state in a Live set.
2. Latency/voice feel; strike-point sweep (nodal lines should audibly
   reshape timbre); manifold switching.
3. Negative tilt on a real keyboard — first playable taste of the
   implausible direction.

## Next (pending checkpoint verdict)

Phase 2 — Ricci flow modulation (the actual thesis): Chow–Luo combinatorial
flow on the geometry thread, RELAX/SHARPEN, global-vs-snapshot voice mode,
perturbation fast path between full re-solves, bow excitation. Given the
aesthetic direction: SHARPEN ranges and metric-morph targets get first-class
treatment.
