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

## Status: Phase 0 complete → awaiting CHECKPOINT 0

The Python audition rig is built, all objective oracles pass, and the
CHECKPOINT 0 listening matrix is rendered in
[`renders/checkpoint0/`](renders/checkpoint0/) (see its
[MANIFEST.md](renders/checkpoint0/MANIFEST.md) for what to listen for).
Current state in detail: [STATUS.md](STATUS.md).

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
