"""Intrinsic cotan Laplacian and eigensolver.

Everything is built from per-face edge lengths only (law of cosines + Heron),
never from vertex positions, so a mesh's metric can be edited intrinsically
(flat torus, conformal perturbation, metric-interpolation flow) without
touching the embedding. This is the same contract the C++ geometry thread
will honor in Phase 1+.

Conventions:
  - stiffness L is symmetric positive semidefinite (the "graph-Laplacian"
    sign), L @ const = 0
  - mass M is barycentric-lumped diagonal
  - generalized problem L phi = lambda M phi, eigenvectors M-orthonormal
"""

from __future__ import annotations

import numpy as np
from scipy.sparse import coo_matrix, csr_matrix, diags
from scipy.sparse.linalg import eigsh

from .meshes import Mesh

COT_CLAMP = 1e4  # guard against near-degenerate triangles


def face_cotans_and_areas(lengths: np.ndarray):
    """Cotangents at each face corner and face areas, from edge lengths alone.

    lengths[:, j] is the edge opposite corner j. Law of cosines gives the
    angle at each corner; Heron gives the area.
    """
    l0, l1, l2 = lengths[:, 0], lengths[:, 1], lengths[:, 2]
    s = 0.5 * (l0 + l1 + l2)
    area_sq = s * (s - l0) * (s - l1) * (s - l2)
    if np.any(area_sq <= 0):
        raise ValueError(f"{np.sum(area_sq <= 0)} faces violate the triangle inequality")
    area = np.sqrt(area_sq)

    # cot(angle at corner j) = (sum of adjacent squared lengths - opposite^2) / (4*area)
    cot = np.stack([
        (l1 ** 2 + l2 ** 2 - l0 ** 2),
        (l2 ** 2 + l0 ** 2 - l1 ** 2),
        (l0 ** 2 + l1 ** 2 - l2 ** 2),
    ], axis=1) / (4.0 * area[:, None])
    cot = np.clip(cot, -COT_CLAMP, COT_CLAMP)
    return cot, area


def cotan_laplacian(mesh: Mesh, lengths: np.ndarray = None):
    """Build (L, M) from the mesh connectivity and (optionally overridden) edge lengths."""
    F = mesh.F
    lengths = mesh.edge_lengths if lengths is None else lengths
    cot, area = face_cotans_and_areas(lengths)
    n = mesh.n_vertices

    # edge opposite corner j connects the other two corners with weight cot_j / 2
    ii, jj, ww = [], [], []
    for corner, (a, b) in enumerate(((1, 2), (2, 0), (0, 1))):
        w = 0.5 * cot[:, corner]
        ii.append(F[:, a]); jj.append(F[:, b]); ww.append(w)
        ii.append(F[:, b]); jj.append(F[:, a]); ww.append(w)
    ii, jj, ww = np.concatenate(ii), np.concatenate(jj), np.concatenate(ww)

    W = coo_matrix((ww, (ii, jj)), shape=(n, n)).tocsr()
    L = csr_matrix(diags(np.asarray(W.sum(axis=1)).ravel()) - W)

    mass = np.zeros(n)
    np.add.at(mass, F.ravel(), np.repeat(area / 3.0, 3))
    M = diags(mass).tocsr()
    return L, M


def solve_modes(L, M, k: int = 96):
    """Lowest k nonzero modes of L phi = lambda M phi.

    Shift-invert with a small negative sigma (L is PSD so L - sigma*M is
    positive definite) — the same strategy the Spectra-based C++ solver will
    use. Returns (lam[k], Phi[n,k]) with the trivial constant mode dropped
    and eigenvectors M-orthonormal.
    """
    scale = np.median(M.diagonal()) or 1.0
    lam, phi = eigsh(L, k=k + 1, M=M, sigma=-1e-3 * scale, which="LM")
    order = np.argsort(lam)
    lam, phi = lam[order], phi[:, order]
    assert lam[0] < 1e-8 * max(lam[-1], 1.0), f"expected trivial mode first, got lambda_0={lam[0]:.3e}"
    return lam[1:], phi[:, 1:]
