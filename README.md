# curvature

**A synthesis engine where the instrument is a Riemannian 2-manifold and the
performance is a geometric flow.**

Partial frequencies are eigenvalues of the Laplace–Beltrami operator on a
triangle mesh (`f_k = c·√λ_k`); excitation coupling comes from eigenfunction
values at the strike point; modulation comes from evolving the metric under
(discrete Ricci) flow, so all partials drift as the spectrum of *one changing
object*. The rendering layer is a deliberately conventional modal resonator
bank — all novelty is isolated in the control manifold.

Target deliverable: VST3 instrument (JUCE 8 + CMake) hosted in Ableton
Live 12. Full design and phased build plan: [PROPOSAL.md](PROPOSAL.md).
Working rules for autonomous builds: [CLAUDE.md](CLAUDE.md).

## Status: Phase 1 complete → awaiting CHECKPOINT 1 (play it in Ableton)

CHECKPOINT 0 passed: genus taxonomy and drift coherence both confirmed by
ear. The JUCE plugin (VST3 + Standalone) now builds with all Phase 1 gates
green — pluginval strictness 10, C++ eigenvalues matching the Phase 0 Python
rig, the FFT oracle running against the real C++ DSP, sanitizers clean.
Current state in detail: [STATUS.md](STATUS.md).

## Plugin quickstart

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
ctest --test-dir build

# gates
tools/run_pluginval.sh
.venv/bin/python -m prototype.oracles_cpp build
```

The VST3 installs itself to `~/Library/Audio/Plug-Ins/VST3/`; the Standalone
app lands in `build/curvsynth_plugin_artefacts/Release/Standalone/`.

## Phase 0 quickstart

```sh
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt

# objective gates — exit 0 means all pass
.venv/bin/python -m prototype.oracles

# re-render the listening matrix into renders/checkpoint0/
.venv/bin/python -m prototype.audition_matrix
```

(Comments on their own lines: interactive zsh doesn't strip trailing `#`
comments by default, so same-line comments get passed to the script as
arguments.)

## Layout

```
PROPOSAL.md         design document & phased plan
CLAUDE.md           build/test commands, RT rules, oracle gates, checkpoint protocol
STATUS.md           where the project is right now
prototype/          Phase 0 Python rig (kept permanently as the reference oracle)
assets/manifolds/   preset meshes (OBJ embeddings; metrics are intrinsic)
renders/            audition artifacts per checkpoint
src/, tests/, tools/  C++ plugin (Phase 1+, not yet started)
```
