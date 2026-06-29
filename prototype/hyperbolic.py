"""Audition prototype: does a chaotic / negatively-curved 3-manifold sound
distinct from the flat 3-torus, and is it worth building a canonical hyperbolic
3-manifold for?

The audible signature of a hyperbolic manifold is GOE eigenvalue statistics
(chaotic geodesics -> level repulsion, no degeneracy -> dense IRREGULAR
shimmer) on top of 3D Weyl density. The flat 3-torus has the density but is
integrable: highly DEGENERATE, regular spectrum. A generic (strongly irregular)
metric on the 3-torus has chaotic geodesics -> GOE -- the same audible essence
as a true hyperbolic manifold, and tractable with the existing tet FEM.

Oracle: the gap-ratio statistic <r>, r_n = min(s_n,s_{n-1})/max(...).
  Poisson / integrable (flat torus):  <r> ~ 0.386
  GOE / chaotic (hyperbolic-like):     <r> ~ 0.530
This is degeneracy-robust and needs no spectral unfolding.

Run:  python -m prototype.hyperbolic
"""

from __future__ import annotations

import numpy as np

from .manifold4d import flat_3torus, fem_laplacian_3d, solve_modes_3d


# Fixed deterministic conformal-factor waves (k integer for periodicity, phase).
# A STRONG high-frequency conformal metric on the 3-torus -> real curvature
# variation -> chaotic geodesics -> GOE statistics (the hyperbolic signature).
# (Vertex displacement is too gentle once the period is correct; conformal
# scaling isn't capped by tet validity, so it reaches full GOE.) Identical in
# Python and C++ so spectra cross-check.
_CHAOS_WAVES = [
    ((1, 0, 0), 0.40), ((0, 1, 0), 1.10), ((0, 0, 1), 2.00), ((1, 1, 0), 0.70),
    ((0, 1, 1), 1.50), ((2, 0, 1), 1.00), ((2, 1, 0), 0.30), ((1, 0, 2), 1.80),
    ((0, 2, 1), 0.90), ((1, 2, 1), 2.20), ((3, 0, 1), 0.50), ((0, 3, 1), 1.30),
    ((2, 2, 1), 0.80),
]
CHAOS_AMP = 1.4


def chaotic_conformal(V, amp=CHAOS_AMP):
    """The fixed chaotic conformal factor sampled at vertices V (normalized to
    +/- amp)."""
    u = np.zeros(V.shape[0])
    for k, phase in _CHAOS_WAVES:
        u += np.sin(2 * np.pi * (k[0] * V[:, 0] + k[1] * V[:, 1] + k[2] * V[:, 2]) + phase)
    return u * (amp / max(np.abs(u).max(), 1e-9))


def chaotic_3torus(grid=12, amp=CHAOS_AMP):
    """Flat 3-torus tet mesh + the fixed chaotic conformal factor (deterministic,
    reproducible in C++). Returns (V, tets, spacing, conformal)."""
    V, tets, sp = flat_3torus(grid, grid, grid, 1.0, 1.0, 1.0)
    return V, tets, sp, chaotic_conformal(V, amp)


def gap_ratio(lam, drop=4):
    """Mean gap-ratio <r>. Drops the lowest few modes (boundary of the
    spectrum) and any exact-zero gaps (degeneracies count as r=0)."""
    s = np.diff(np.sort(lam)[drop:])
    s = s[s > 1e-9]  # collapse exact degeneracies handled below
    # recompute including zero gaps so degeneracy lowers <r>
    g = np.diff(np.sort(lam)[drop:])
    r = []
    for n in range(1, len(g)):
        lo, hi = min(g[n], g[n - 1]), max(g[n], g[n - 1])
        r.append(lo / hi if hi > 1e-12 else 0.0)
    return float(np.mean(r))


def main():
    K = 120
    print("level-spacing gap-ratio <r>  (Poisson 0.386 .. GOE 0.530)\n")

    V, tets, sp = flat_3torus(12, 12, 12, 1.0, 1.0, 1.0)
    L, M = fem_laplacian_3d(V, tets, sp)
    lam_flat, _ = solve_modes_3d(L, M, k=K)
    print(f"  flat cubic 3-torus     <r> = {gap_ratio(lam_flat):.3f}   (expect ~0.39, degenerate)")

    V, tets, sp = flat_3torus(12, 12, 12, 1.0, 1.32, 1.71)
    L, M = fem_laplacian_3d(V, tets, sp)
    lam_aniso, _ = solve_modes_3d(L, M, k=K)
    print(f"  flat aniso 3-torus     <r> = {gap_ratio(lam_aniso):.3f}")

    V, tets, sp, u = chaotic_3torus(12)
    L, M = fem_laplacian_3d(V, tets, sp, period=(1, 1, 1), conformal=u)
    lam, _ = solve_modes_3d(L, M, k=K)
    print(f"  chaotic 3-manifold     <r> = {gap_ratio(lam):.3f}   (expect ~0.53 = GOE)")


if __name__ == "__main__":
    main()
