"""4D-object prototype: the flat 3-torus T^3 as an audible resonator.

The engine is intrinsic (it only needs the Laplace-Beltrami operator), so a
3-manifold tet-meshed in the fundamental domain sonifies through the same
eigenvalue -> spectrum -> modal-render path as the 2-manifolds. T^3 is the
3D analogue of the flat 2-torus: a periodic cube, meshed into tetrahedra,
with the 3D FEM (cotan-analogue) Laplacian.

Why it should sound different: Weyl's law. Mode count below frequency f
grows like f^2 in 2D but f^3 in 3D, so a 3-torus piles up partials in the
highs far faster than any surface — an audibly "thicker", denser object that
no 2-manifold can be.

Analytic continuum spectrum:  lambda = (2*pi)^2 (l^2/a^2 + m^2/b^2 + n^2/c^2).
Plane waves are exact eigenvectors of the discrete periodic operator, so the
oracle checks the discrete spectrum against them directly.
"""

from __future__ import annotations

import numpy as np
from scipy.sparse import coo_matrix, csr_matrix, diags
from scipy.sparse.linalg import eigsh

# Freudenthal 6-tet split of a unit cube cell, sharing the 000-111 diagonal.
# Each tet = 000, then unit steps along a permutation of the three axes.
_STEP = {"x": (1, 0, 0), "y": (0, 1, 0), "z": (0, 0, 1)}
_PERMS = [("x", "y", "z"), ("x", "z", "y"), ("y", "x", "z"),
          ("y", "z", "x"), ("z", "x", "y"), ("z", "y", "x")]


def _cell_tets():
    tets = []
    for p in _PERMS:
        corner = (0, 0, 0)
        path = [corner]
        for axis in p:
            corner = tuple(corner[i] + _STEP[axis][i] for i in range(3))
            path.append(corner)
        tets.append(path)  # 4 corners (000 -> ... -> 111)
    return tets


def flat_3torus(nx=10, ny=10, nz=10, a=1.0, b=1.0, c=1.0):
    """Build the periodic tetrahedral 3-torus: vertex positions (fundamental
    domain), tets (periodic indices), and per-axis spacing."""
    dx, dy, dz = a / nx, b / ny, c / nz

    def vid(i, j, k):
        return ((i % nx) * ny + (j % ny)) * nz + (k % nz)

    V = np.array([[i * dx, j * dy, k * dz]
                  for i in range(nx) for j in range(ny) for k in range(nz)], dtype=np.float64)

    cell = _cell_tets()
    tets = []
    for i in range(nx):
        for j in range(ny):
            for k in range(nz):
                for path in cell:
                    tets.append([vid(i + ci, j + cj, k + ck) for (ci, cj, ck) in path])
    return V, np.array(tets, dtype=np.int64), (dx, dy, dz)


def fem_laplacian_3d(V, tets, spacing):
    """3D FEM stiffness L (PSD, L*const=0) and lumped mass M for a tet mesh.

    Per tet: gradients of the linear barycentric basis are constant; element
    stiffness K_ij = vol * (grad phi_i . grad phi_j). Edge vectors use the
    periodic spacing so wrap-around tets get correct (non-displacement) geometry.
    """
    dx, dy, dz = spacing
    n = V.shape[0]

    ii, jj, vv = [], [], []
    mass = np.zeros(n)
    box = np.array([V[:, 0].max() + dx, V[:, 1].max() + dy, V[:, 2].max() + dz])

    for tet in tets:
        p = V[tet]
        # wrap edges relative to p[0] into the minimal-image convention
        e = p - p[0]
        e -= box * np.round(e / box)
        M3 = np.stack([e[1], e[2], e[3]], axis=1)  # columns are edge vectors
        det = np.linalg.det(M3)
        vol = abs(det) / 6.0
        if vol < 1e-15:
            continue
        Ginv = np.linalg.inv(M3)          # rows give grad of b1,b2,b3
        grads = np.zeros((4, 3))
        grads[1:] = Ginv                  # grad phi_1, phi_2, phi_3 = rows of inv(M3)
        grads[0] = -grads[1:].sum(axis=0)  # grad phi_0
        for x in range(4):
            for y in range(4):
                ii.append(tet[x]); jj.append(tet[y])
                vv.append(vol * grads[x] @ grads[y])
            mass[tet[x]] += vol / 4.0

    L = csr_matrix(coo_matrix((vv, (ii, jj)), shape=(n, n)))
    return L, diags(mass).tocsr()


def solve_modes_3d(L, M, k=96):
    scale = np.median(M.diagonal()) or 1.0
    lam, phi = eigsh(L, k=k + 1, M=M, sigma=-1e-6 * scale, which="LM")
    order = np.argsort(lam)
    lam, phi = lam[order], phi[:, order]
    return lam[1:], phi[:, 1:]


def analytic_3torus_spectrum(a, b, c, n_each=6):
    vals = []
    for l in range(-n_each, n_each + 1):
        for m in range(-n_each, n_each + 1):
            for nn in range(-n_each, n_each + 1):
                vals.append((2 * np.pi) ** 2 * (l * l / a / a + m * m / b / b + nn * nn / c / c))
    return np.array(sorted(vals))


def weyl_counts(lam):
    """Cumulative mode count — should grow ~lambda^{3/2} for a 3-manifold
    (vs ~lambda for a surface). Returned for the oracle/printout."""
    return np.arange(1, len(lam) + 1)
