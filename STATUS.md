# Project status

**Phase 3 (run 1) — COMPLETE (2026-06-11). Holding at CHECKPOINT 3.**

Checkpoint history: CHECKPOINT 0 passed (genus taxonomy + drift coherence;
aesthetic direction = implausible/"metaphysical" objects). CHECKPOINT 1
passed ("It's working!"). CHECKPOINT 2 passed ("working properly" — and the
in-chat flow visualizer was "extremely helpful", which set this run's
priorities).

## Phase 3 run 1: legibility + local gesture

- **In-plugin visualizer** (Phase 4 pulled forward on Julian's feedback):
  custom editor replaces the generic one. Left: manifold with live curvature
  heatmap (coral above target / teal below), conformal bulge, drag-to-rotate,
  **click the surface to set the strike point**. Right: scrolling partial-
  ratio traces + curvature error readout + full parameter panel. 30 Hz
  software rendering; geometry state arrives via a second wait-free triple
  buffer (`VizBus`), editor/message thread is sole consumer.
- **Press** param: continuous localized curvature injection at the strike
  vertex (BFS-bump profile, clamps apply) — a localized SHARPEN that grows a
  resonant deformation under your finger; Relax heals it. Visible live in
  the editor.
- **Damp Comb** param: every other mode's T60 collapses — the selective-
  absorption implausible material from checkpoint 0, now performable.
- **Pitch bend**: +/-2 semitones, whole spectrum rigidly.

## Phase 3 run 1 gates — all pass

- Catch2 suite (14 cases; new: press localization + heal-by-relax): pass
- pluginval strictness 10 (includes editor open/close): SUCCESS
- C++ FFT oracle: 14 partials within 1 cent (worst 0.000)
- ASan+UBSan suite: clean

## What's new in Phase 2

- **Chow–Luo combinatorial Ricci flow** (`src/geometry/RicciFlow.*`,
  prototyped + validated in `prototype/ricci.py` first): inversive-distance
  circle packing, explicit Euler with step-halving on metric validity and
  (forward) on curvature-error monotonicity. Reverse flow (SHARPEN) runs the
  field backwards under hard |u−u0| clamps — bounded expressive territory.
- **Perturbation fast path:** Rayleigh quotients of the stale eigenbasis
  with the current operator at every flow step (~25 ms); scheduled full
  re-solves every 0.6 units of accumulated flow time, mode-matched by
  eigenvector overlap × eigenvalue proximity, then **blended over 8 frames**
  so a re-solve never lands as an audible step.
- **Plugin params:** Flow (Off/Relax/Sharpen), Flow Rate, Kick (each toggle
  = one conformal perturbation), Voices (Snapshot/Global Flow, default
  Global — the instrument itself evolves under your hands), Bow.
- **Bow excitation:** per-control-interval stochastic energy injection
  weighted by the strike coupling pattern ("simple stochastic model first" —
  refine by ear).
- `render_offline` gained `--flow/--flow-rate/--kick/--bow` so the offline
  oracle path exercises the full flow machinery.

## Phase 2 gates — all pass

- Forward flow: max |K_i − K̄| monotonically decreasing on **all five
  presets** (Python oracle: genus 0/1/2; C++ test: icosphere + golden torus)
- Eigenvalue trajectories continuous: ≤5% frame-to-frame ratio change
  through fast-path updates AND scheduled re-solves (Catch2 test)
- **30-min soak × 2** (golden torus and genus-2; 72k steps each, alternating
  Relax/Sharpen/kick): no NaN/denormal/divergence
- Reverse-flow clamps respected (Python oracle + C++ test)
- pluginval strictness 10: SUCCESS (re-run post-Phase 2)
- C++ FFT oracle: 14 partials within 1 cent (worst 0.000)
- ASan+UBSan: full suite clean. TSan: SPSC bus stress clean (threading
  pattern unchanged from Phase 1: single geometry producer, audio consumer).

## Tuning notes for the checkpoint (everything by-ear is provisional)

- Flow Rate maps quadratically to dt (max 0.25/step at 25 ms cadence).
- Kick amplitude fixed at 0.6 conformal units; geometry-thread seed advances
  per kick so each one is a different bump pattern.
- Bow gain scales with amount²; injection at 750 Hz control rate.

## CHECKPOINT 3 — what Julian checks (in Live)

1. The editor: does the manifold view make the instrument legible while
   playing? Click-to-strike workflow; rotate during flow.
2. Press as a gesture (automate it or ride it): localized bump -> localized
   spectral change, healed by Relax.
3. Damp Comb sweep on genus-2; pitch bend feel.

## CHECKPOINT 3 round 6 (2026-06-13) — verdict was "passes" + bug/feel batch

Fixed/added:
- **viz frame-stealing (root cause)**: ManifoldView + SpectrumView were two
  consumers on one SPSC VizBus, stealing frames from each other. Now the
  editor is the sole reader and fans each frame out to both views — fixes
  "press audible but not visible" and intermittent spectrum staleness.
- **gain staging**: excitation scaled by makeup/sqrt(numModes) so brightness
  and mode count no longer push the soft-clip into crunch; far more clean
  headroom. Added an output **level meter** (green/amber/red, 0 dBFS tick).
- **bow rework**: continuous lowpassed-noise friction drive (was stepped
  per-control-interval = the crackle) with a strong low-frequency tilt
  (^1.7) so a round object hums instead of only lighting upper modes.
- **Memory Rate**: own clock, decoupled from Flow Rate (its own knob).
- **Manual sharpness servo**: slewed target + hysteresis deadband — stops
  the jitter/visual wobble while the slider moves.
- **Spectral Warp** (new weirdness param, 0.2–2.5, default 1): f_k =
  f1*(f_k/f1)^warp — breaks the physical sqrt-lambda dispersion. <1 hums
  toward unison, >1 spreads into impossible super-stretched spectra.
  Mirrored in the spectrum view. First answer to "surface more params for
  implausibility."

Deferred (noted for next runs): unfocused sharpening ("Sharpen Spread");
off-center / cursor-anchored zoom; bow still "refine by ear". Aesthetic note:
Julian finds it rich for industrial/physical sounds but not yet surprising/
"metaphysical" — wants more weirdness params (warp is a start); expects
modulation + keytracking + MPE to help bridge.

## Checkpoint 3 round 7 (2026-06-13) — Manual/Memory model + warp liveness

- **Manual servo reworked into a sharpness FLOOR**: it only sharpens *up*
  toward its target, never forces deviation down. Press/Kick now stack
  freely on top to full extremes (the previous servo relaxed any excess,
  capping press and acting like a stuck Memory). Gated by Flow Rate (Flow
  Rate 0 freezes). Fixes: press constrained in Manual, reversion with Flow
  Rate off, "Memory stuck on" feel.
- **Memory owns the return-to-smooth direction in all modes** (re-enabled in
  Manual): Memory = 1 → sharpening/press/kick permanent; < 1 → springs back
  at Memory Rate, balancing against the servo's up-push. Trade-off to note:
  at Memory = 1, lowering Sharpness doesn't actively un-sharpen (use Memory
  < 1 or Reset Shape).
- **Spectral Warp liveness tied to the Voices switch**: Global Flow = warp
  live (sweepable while ringing); Snapshot = warp frozen per-note (the old
  behavior Julian wanted back). No new param.
- **Press is now bipolar** (range -1..+1): positive concentrates curvature
  (sharp/bright, the original press), negative diffuses it via a local graph
  heat step within the footprint (smooth/round). Smoothing acts on existing
  roughness — sharp presses, kicks, or intrinsic genus-2 curvature.

## Remaining Phase 3 candidates (next runs, order by feedback)

MPE, strike-point-per-note, metric-morph targets as a parameter,
surgery/voice-splitting (stretch), OBJ import UX. Phase 4 polish: nicer
shading, spectrum labels in Hz, curvature-injection by clicking with a
modifier key.

## Roadmap ideas (Julian, pre-checkpoint-3) with feasibility notes

1. **Strike-responsive deformation** — mallet hits dent/ripple the object so
   sound responds to performance. HIGH feasibility, near-term: note-on
   events (vertex + velocity) posted over a lock-free queue to the geometry
   thread -> localized curvature kick (press machinery reused); ripples =
   damped wave equation on the conformal factor at geometry rate. Top
   candidate for the next run.
2. **Shape categories / extrusion** — beams, tuning forks, plates, ripple
   textures. MEDIUM: standalone beam/plate presets are easy (manifolds with
   boundary: cotan Laplacian handles free/clamped edges with minor changes);
   *gluing* appendages onto existing meshes is the hard part (cousin of
   surgery). Phase it: boundary presets first, gluing later.
3. **4th dimension** — genuinely natural here: the engine is intrinsic (no
   embedding needed), so a tet-meshed 3-manifold (3-sphere, 3-torus) can be
   sonified by the same machinery ("the sound of a 4D bell"; mode density
   grows audibly differently — Weyl asymptotics). Full 3D Ricci flow is
   research-grade (Perelman/surgery), but a conformal Yamabe-type flow gives
   the same RELAX/SHARPEN musical behavior. MEDIUM-HIGH effort, real payoff.
4. **Mandelbulb** — EASY as a preset: marching-cubes the isosurface like
   genus2; fractal surface roughness -> dense irregular spectrum with
   strongly localized eigenfunctions (strike point becomes hyper-expressive;
   pairs perfectly with SHARPEN). Cheap win next run.
