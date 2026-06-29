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


def perturbed_3torus(grid=12, amp=0.35, seed=0):
    """Flat 3-torus tet mesh with a smooth, periodic random displacement of the
    vertices -> a generic (chaotic) metric on T^3. Connectivity (topology) is
    unchanged; only edge lengths/curvature change. amp is in units of the cell
    size; kept < 0.5 so tets stay non-degenerate."""
    V, tets, sp = flat_3torus(grid, grid, grid, 1.0, 1.0, 1.0)
    dx = sp[0]
    rng = np.random.default_rng(seed)

    # smooth periodic displacement field: a few low Fourier modes per axis
    disp = np.zeros_like(V)
    for _ in range(6):
        k = rng.integers(1, 4, size=3)
        phase = rng.uniform(0, 2 * np.pi, size=3)
        dirn = rng.standard_normal(3)
        dirn /= np.linalg.norm(dirn)
        wave = np.ones(V.shape[0])
        for ax in range(3):
            wave = wave * np.sin(2 * np.pi * k[ax] * V[:, ax] + phase[ax])
        disp += np.outer(wave, dirn)

    disp *= amp * dx / max(np.abs(disp).max(), 1e-9)
    return V + disp, tets, sp


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

    for amp in (0.25, 0.4):
        V, tets, sp = perturbed_3torus(12, amp=amp, seed=1)
        L, M = fem_laplacian_3d(V, tets, sp)
        lam, _ = solve_modes_3d(L, M, k=K)
        print(f"  chaotic 3-manifold a={amp} <r> = {gap_ratio(lam):.3f}   (expect ~0.53 = GOE)")


if __name__ == "__main__":
    main()
