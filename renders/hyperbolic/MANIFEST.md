# Hyperbolic / chaotic 3-manifold — audition before building

The question: is the "hyperbolic 3-manifold" sound family worth building? A
*canonical* closed hyperbolic 3-manifold (Seifert-Weber, Weeks, ...) is a
multi-step geometry project. But its audible essence is **GOE level
statistics** (chaotic geodesics -> level repulsion, no degeneracy -> dense
IRREGULAR shimmer) on top of 3D Weyl density. A generic / strongly irregular
metric on the 3-torus has the same essence and is cheap (reuses the tet
pipeline), so it's a fair audition.

Both struck at 110 Hz, same damping/mallet.

- `3torus_cube.wav` — the flat cubic 3-torus we already ship. Massively
  degenerate: gap-ratio <r> ~ 0.00, 93% of partial gaps are tight clusters
  (shells) -> beating, chord-like.
- `chaotic_hyperbolic.wav` — a chaotic (generic-metric) 3-manifold. All
  degeneracies lifted: <r> ~ 0.43 (past Poisson 0.386, toward GOE 0.530),
  only 24% tight gaps -> partials spread out, dense + irregular shimmer.

Oracle (`prototype/hyperbolic.py`): gap-ratio <r>, Poisson/integrable 0.386
vs GOE/chaotic 0.530. Flat torus 0.00 (sub-Poisson, degenerate) ->
chaotic 0.54 = FULL GOE.

How it's built: a STRONG fixed high-frequency CONFORMAL metric on the 3-torus
(not vertex displacement — that was too gentle once the period was computed
correctly, and an early bug made it look stronger than it was). The conformal
factor isn't capped by tet validity, so it reaches genuine GOE. This IS a
real negatively-/variably-curved 3-manifold spectrum; the only thing a
canonical hyperbolic manifold (Seifert-Weber, Weeks) would add is constant
(rather than varying) curvature — same audible GOE family.

SHIPPED as the preset "Hyperbolic-like (4D)" (Torus3DChaotic).
