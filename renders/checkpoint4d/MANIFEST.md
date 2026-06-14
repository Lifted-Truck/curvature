# 4D-object audition — the flat 3-torus T^3

Prototype (Python) of a genuinely 3-dimensional resonator, to judge by ear
before committing to the plugin port. The engine is intrinsic, so a
tet-meshed 3-manifold sonifies through the same eigenvalue -> spectrum ->
modal-render path as the surfaces; only the Laplacian construction changes
(3D finite-element instead of the 2D cotan).

All three struck at the same fundamental (110 Hz), same damping/mallet, so
the only difference you hear is the manifold's dimension/shape.

- `3torus_cube.wav` — T^3, equal sides (1:1:1). Highly degenerate, dense
  shells. Measured spectral-density exponent ~3.0 (a surface is ~2.0).
- `3torus_aniso.wav` — T^3, unequal sides (1:1.32:1.71). Degeneracy broken,
  so the dense shells spread into a thicker, shimmering cluster.
- `2torus_golden.wav` — the existing flat 2-torus (1:1.618) for A/B. Same
  fundamental; notice how much sparser the high partials are.

What to listen for: the 3-torus should sound *thicker / denser in the highs*
than the surface — more partials packed into the same range (Weyl's law:
mode count grows like f^3 in a volume vs f^2 on a surface). It is an object
that cannot exist as a surface.

Decision: if this character is compelling, the next run ports T^3 into the
plugin (a tet-mesh code path + a rethought visualizer — a 3-manifold has no
2D surface to show; likely a slice or edge-cloud). If not, we drop it cheaply.
