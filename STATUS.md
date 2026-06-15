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
  *(Round 8: reverted — bipolar press was not worth it; Press is unipolar
  again, the free deformation gesture.)*

## Round 8 (2026-06-13) — Manual servo rebuilt bidirectional; press reverted

- **Press reverted to unipolar** (0..1) free deformation gesture (bipolar
  added little).
- **Manual is bidirectional again**: a position control scrubbing along the
  Ricci flow. Up = reverse flow (sharpen), DOWN = forward flow (relax —
  walks back down the flow, smoothing). My earlier only-up + Memory-owns-down
  design (to dodge jitter) broke down-smoothing — this restores it.
- **Jitter fixed at the source**: the servo now drives **RMS curvature**
  (smooth) instead of max|K-Kbar| (a max that hops between vertices = the
  jitter), toward a reachable target (0.5·sharp²; reverse-flow plateau is
  ~0.6-0.78 across presets, so it settles instead of chasing). Memory
  disabled in Manual (the bidirectional servo owns position).

## Round 9 (2026-06-13) — strike-responsive deformation (CHECKPOINT 3 passed)

Julian's flagship roadmap idea, the thesis payoff: the performance reshapes
the instrument.
- **StrikeQueue** (`src/engine/StrikeQueue.h`): lock-free SPSC ring, audio
  thread (note-on) -> geometry thread. Audio-thread safe (fixed storage, no
  locks/alloc; full = drop). TSan-clean threaded test + FIFO test.
- On note-on the audio thread pushes {strikeParam, velocity}; the geometry
  thread drains and applies a focused velocity-scaled dent (flowPress, sigma
  1.8) at the strike point. Heals per Memory — at Memory = 1 the object
  scars from being played; with Memory < 1 dents fade between notes.
- **Strike Deform** param (0..1, default 0 = off). Most alive in Global Flow
  voice mode (ringing notes retune as the dent forms); in Snapshot it shapes
  subsequent notes.
- Best heard in the visualizer: play and watch the surface dent under each hit.

## Round 10 (2026-06-13) — strike dent direction, ripple, Memory in Manual

- **Strike Deform is now an inward dent** (negative press), not a convex
  bulge — matches "dent", and spectrally distinct (local shrink raises local
  pitch). Renamed "Strike Dent" in the UI.
- **Strike Ripple** (new param): each note-on injects a mean-free localized
  displacement that propagates across the surface as a damped graph wave
  (`RicciFlow::rippleStrike`/`rippleStep`) — a transient on top of the base
  metric, folded into the eigensolve and the visualizer (visible spreading
  ring). Weak spring toward zero so it always returns to rest (the DC mode
  has no Laplacian restoring force otherwise — caught by a test). speed 6,
  damp 2.0 defaults; clamped ±0.6 so u+ripple stays a valid metric.
- **Memory now works in Manual mode**, scaled by (1-sharpness)^2: full at
  neutral sharpness (heals presses/strike-dents), tapering to none as you
  sharpen (so it doesn't fight a held sharp state).
- gates: 20 Catch2 cases (+ripple return-to-rest, strike-queue FIFO/threaded),
  pluginval 10, FFT oracle <1 cent, ASan + TSan clean.

## Round 11 (2026-06-13) — strike kick, ripple amplitude, extreme-sharpness crash

- **Crash fix (extreme sharpness reset)**: at high concentration the
  eigensolver fails / the metric degenerates; the geometry thread was
  catching the exception and reloading the preset ("curls to an extreme then
  resets"). Now `resolve()` and `rayleighUpdate()` swallow eigensolve/
  degenerate-metric failures and COAST on the last good spectrum (per the
  engine's never-glitch rule) — the object holds at the limit instead of
  resetting. Reproduced + guarded by a new soak test.
- **Strike Kick** (was Strike Dent): the brutal clean inward dent is replaced
  by a gentle, mean-free, smooth-random localized deformation (varied per
  hit via seed), self-correcting (scale-neutral, heals per Memory). More
  varied/interesting than a fixed dent.
- **Ripple amplitude raised** for testing the limit: injection 0.4 -> 1.4,
  clamp +/-0.6 -> +/-1.0 (degenerate combos just coast now, so safe to push).
- gates: 21 Catch2 cases, pluginval 10, FFT oracle <1 cent, ASan + TSan clean.

## Round 12 (2026-06-14) — Mandelbulb preset

- New **Mandelbulb (alien)** preset: marching-cubes the mandelbulb escape
  field (power 8), largest connected component, Taubin-smoothed, ~3053 verts,
  genus 4. Spiky self-similar geometry -> dense clustered/inharmonic spectrum
  with localized eigenfunctions (hyper-expressive strike point; SHARPEN goes
  alien). `prototype/meshes.py::mandelbulb`, committed as
  `assets/manifolds/mandelbulb.obj` + baked into the plugin via BinaryData.
- Presets are now OBJ-aware generally (`presetNeedsObj`); plugin/editor/tools
  select the right asset (genus2 vs mandelbulb).
- gates: 21 Catch2 cases incl. mandelbulb eigenvalues vs Python (1e-6),
  pluginval 10, FFT oracle <1 cent, ASan clean.

## Round 13 (2026-06-14) — Ripple Speed param; 4D (3-torus) prototype

- **Ripple Speed** param (modulatable): maps to the strike-ripple wave speed
  (2..14), so the propagation rate can be automated/LFO'd.
- **4D object prototyped in Python first** (audition before the plugin port,
  per project methodology; Julian found the mandelbulb underwhelming so we
  de-risk). `prototype/manifold4d.py`: flat 3-torus T^3, Freudenthal tet mesh
  of a periodic cube, 3D FEM Laplacian (PSD, L*const=0 verified), spectrum
  matches the 3D lattice structure (exact shell multiplicities; eigenvalues
  ~3% low on a coarse 12^3 grid). Audition in `renders/checkpoint4d/`:
  measured spectral-density exponent ~3.0 (3-torus) vs ~2.0 (2-torus) — the
  Weyl dimensional signature, audibly denser highs.
- AWAITING JULIAN'S EAR: if the 3-torus character is compelling, next run
  ports T^3 into the plugin (tet-mesh code path + rethought visualizer, since
  a 3-manifold has no 2D surface — likely a slice or edge-cloud; flow would
  be conformal/Yamabe, not Ricci).

## Round 14 (2026-06-14) — 4D object in the plugin (3-torus T^3)

Julian auditioned the Python prototype and approved the port. Now playable.
- **3D FEM Laplacian** (`geometry/TetMesh`): Freudenthal tet mesh of the
  periodic cube, barycentric-gradient stiffness + lumped mass; per-tet
  conformal scaling (stiffness*s, mass*s^3) makes it intrinsic/gesture-able.
  Validated: PSD null mode, exact plane waves, eigenvalues vs Python (1e-6).
- **TetManifold backend**: conformal factor + the full gesture vocabulary one
  dimension up (press / strike kick / ripple / memory), with conformal
  *diffusion* flow (relax smooths toward uniform, sharpen concentrates) in
  place of Ricci. RicciFlow left untouched (zero regression risk).
- **GeometryService** dispatches 2-manifold (RicciFlow) vs 4D (TetManifold)
  via `is4D_`. Presets **"3-Torus (4D)"** and **"3-Torus aniso (4D)"** (12^3).
- **Visualizer**: wireframe path — the lattice rendered as edges + nodes lit
  by curvature concentration, breathing radially with the conformal factor,
  rotatable, click-a-node to strike.
- gates: 24 Catch2 cases (incl. 3-torus FEM PSD/plane-wave/Python-match),
  pluginval 10, FFT oracle (2D intact), ASan clean, 30-min 4D flow soak clean.

## Round 15 (2026-06-14) — viz lag, oblique 3-torus, comb upgrade, manual extremity

- **Wireframe lag fixed** (it was render cost, not debug mode): the editor now
  repaints the manifold view only when geometry changes (frameId), and the
  4D wireframe draws a subsampled ~8^3 lattice instead of all ~1700 nodes +
  thousands of tet edges. Sound still uses the full 12^3 mesh.
- **Oblique 3-torus preset** ("3-Torus oblique (4D)"): TetMesh generalized to
  an arbitrary lattice basis (rhombohedral / "isosceles", equal-length basis
  vectors at 60deg); generalized periodic minimal-image. Distinct shell
  structure from the cube — audibly different. Plane-wave-exactness test added.
- **Comb upgraded**: near-total notches at the top (x0.002, was x0.03) +
  **Comb Freq** param (notch spacing over mode index, wide .. every-other-mode),
  mirrored in the spectrum view.
- **Manual sharpness reaches further**: target 0.5 -> 0.9*sharp^2 with a
  stall guard that settles at the metric's actual ceiling (resolve() already
  coasts, so the limit no longer crashes).
- gates: 25 Catch2 cases, pluginval 10, FFT oracle, ASan clean.

## Round 16 (2026-06-14) — perpetually-morphing object (Morph) SHIPPED

The "impossible substance": a conformal wave that perpetually travels across
the manifold so the metric never settles and the spectrum continuously morphs.
- Phase field = base first eigenfunction phi_1 normalized to [-pi,pi]; its
  gradient is a natural travel axis on any manifold (orbits a cycle on the
  torus, sweeps pole-to-pole on the sphere). morph_i = depth*cos(theta_i -
  phase), mean-free, added to the metric on top of u_/ripple_ (perpetual, not
  healed by Memory). Added to both backends (RicciFlow + TetManifold).
- Params: **Morph Rate** (bipolar = speed + direction) and **Morph Depth**.
  Geometry thread advances the phase each tick; coexists with every other
  gesture; bounded by construction (can't diverge).
- Measured spectral swing over a morph cycle: ~55% (2-torus), ~104% (3-torus)
  — strong, cyclic, audible motion. Works on every preset incl. 4D.
- gates: 26 Catch2 cases (incl. morph-perpetual-and-finite), pluginval 10,
  ASan clean, seal verified.

(Particle-field resonance remains a separate-engine/project idea, not a
bolt-on.)

## Round 17 (2026-06-14) — Ripple Size, Morph Angle, two-column UI

- **Ripple Size** param: injection footprint (tight sharp/high-freq .. broad),
  amplitude scaled up at the tight end for more extreme ripples. rippleStrike
  now takes sigma (both backends).
- **Morph Angle** param: rotates the morph travel axis in the phi_1/phi_2
  plane (captured at load); different sweep directions across the manifold.
  Verified non-trivial (distinct trajectories at 0 vs pi/2).
- **UI: two-column slider panel** (was running out of vertical space) +
  larger default window (1000x620); narrower value boxes.
- gates: 26 Catch2 cases, pluginval 10, FFT oracle, ASan clean, seal ok.

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
