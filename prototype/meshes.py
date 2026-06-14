"""Mesh generation for the Phase 0 audition rig.

Three preset manifold families:
  - icosphere(subdiv)         : genus 0, near-round
  - flat_torus(nx, ny, a, b)  : genus 1, *intrinsically flat* metric (edge
                                lengths come from the lattice, not from a 3D
                                donut embedding) so the discrete spectrum is
                                exactly the lattice spectrum
  - genus2(resolution)        : genus 2, marching cubes on a smooth-union of
                                two tori, Taubin-smoothed

A Mesh carries vertex positions (for strike-point picking and later
visualization) and faces; the *metric* used everywhere downstream is the
per-edge length array, so flat_torus can override lengths intrinsically.
"""

from __future__ import annotations

from dataclasses import dataclass, field

import numpy as np


@dataclass
class Mesh:
    name: str
    V: np.ndarray            # (n, 3) float64 vertex positions (embedding; may be a quotient-space immersion)
    F: np.ndarray            # (m, 3) int64 triangle vertex indices, CCW
    edge_lengths: np.ndarray = field(default=None)  # (m, 3) per-face edge lengths; row i = lengths opposite F[i,0..2]

    def __post_init__(self):
        if self.edge_lengths is None:
            self.edge_lengths = face_edge_lengths(self.V, self.F)

    @property
    def n_vertices(self) -> int:
        return self.V.shape[0]

    def euler_characteristic(self) -> int:
        edges = set()
        for tri in self.F:
            for a, b in ((tri[0], tri[1]), (tri[1], tri[2]), (tri[2], tri[0])):
                edges.add((min(a, b), max(a, b)))
        return self.n_vertices - len(edges) + self.F.shape[0]

    def genus(self) -> int:
        chi = self.euler_characteristic()
        assert chi % 2 == 0, f"odd Euler characteristic {chi}: not a closed surface"
        return (2 - chi) // 2


def face_edge_lengths(V: np.ndarray, F: np.ndarray) -> np.ndarray:
    """Per-face edge lengths; column j is the length of the edge *opposite* vertex F[:, j]."""
    p0, p1, p2 = V[F[:, 0]], V[F[:, 1]], V[F[:, 2]]
    return np.stack([
        np.linalg.norm(p1 - p2, axis=1),
        np.linalg.norm(p2 - p0, axis=1),
        np.linalg.norm(p0 - p1, axis=1),
    ], axis=1)


# ---------------------------------------------------------------- icosphere

def icosphere(subdiv: int = 3, radius: float = 1.0) -> Mesh:
    t = (1.0 + np.sqrt(5.0)) / 2.0
    V = np.array([
        [-1, t, 0], [1, t, 0], [-1, -t, 0], [1, -t, 0],
        [0, -1, t], [0, 1, t], [0, -1, -t], [0, 1, -t],
        [t, 0, -1], [t, 0, 1], [-t, 0, -1], [-t, 0, 1],
    ], dtype=np.float64)
    F = np.array([
        [0, 11, 5], [0, 5, 1], [0, 1, 7], [0, 7, 10], [0, 10, 11],
        [1, 5, 9], [5, 11, 4], [11, 10, 2], [10, 7, 6], [7, 1, 8],
        [3, 9, 4], [3, 4, 2], [3, 2, 6], [3, 6, 8], [3, 8, 9],
        [4, 9, 5], [2, 4, 11], [6, 2, 10], [8, 6, 7], [9, 8, 1],
    ], dtype=np.int64)

    for _ in range(subdiv):
        V, F = _subdivide(V, F)

    V = radius * V / np.linalg.norm(V, axis=1, keepdims=True)
    return Mesh(f"icosphere{subdiv}", V, F)


def _subdivide(V: np.ndarray, F: np.ndarray):
    midpoint_cache: dict[tuple[int, int], int] = {}
    verts = list(V)

    def midpoint(a: int, b: int) -> int:
        key = (min(a, b), max(a, b))
        if key not in midpoint_cache:
            verts.append((verts[a] + verts[b]) / 2.0)
            midpoint_cache[key] = len(verts) - 1
        return midpoint_cache[key]

    new_faces = []
    for a, b, c in F:
        ab, bc, ca = midpoint(a, b), midpoint(b, c), midpoint(c, a)
        new_faces += [[a, ab, ca], [b, bc, ab], [c, ca, bc], [ab, bc, ca]]
    return np.array(verts), np.array(new_faces, dtype=np.int64)


# ---------------------------------------------------------------- flat torus

def flat_torus(nx: int = 32, ny: int = 24, a: float = 1.0, b: float = 1.0) -> Mesh:
    """Genus-1 lattice torus with an intrinsically flat metric.

    The fundamental domain is an a x b rectangle on an nx x ny periodic grid,
    each cell split into two right triangles. Edge lengths are taken from the
    flat lattice, so the cotan Laplacian sees a genuinely flat metric and its
    spectrum is the exact discrete lattice spectrum (oracle-checkable against
    plane waves). The 3D donut embedding below is for display/strike-picking
    only and plays no metric role.
    """
    dx, dy = a / nx, b / ny

    def vid(i: int, j: int) -> int:
        return (i % nx) * ny + (j % ny)

    faces = []
    for i in range(nx):
        for j in range(ny):
            faces.append([vid(i, j), vid(i + 1, j), vid(i + 1, j + 1)])
            faces.append([vid(i, j), vid(i + 1, j + 1), vid(i, j + 1)])
    F = np.array(faces, dtype=np.int64)

    # display embedding (standard donut)
    R, r = 1.0, 0.4
    theta = 2 * np.pi * np.repeat(np.arange(nx), ny) / nx
    phi = 2 * np.pi * np.tile(np.arange(ny), nx) / ny
    V = np.stack([
        (R + r * np.cos(phi)) * np.cos(theta),
        (R + r * np.cos(phi)) * np.sin(theta),
        r * np.sin(phi),
    ], axis=1)

    diag = np.hypot(dx, dy)
    # face [v, v+x, v+x+y]: edge opposite vertex 0 is the vertical (dy) edge,
    # opposite vertex 1 the diagonal, opposite vertex 2 the horizontal (dx) edge
    lengths = np.empty((F.shape[0], 3))
    lengths[0::2] = [dy, diag, dx]
    # face [v, v+x+y, v+y]: opposite 0 -> horizontal, opposite 1 -> vertical, opposite 2 -> diagonal
    lengths[1::2] = [dx, dy, diag]

    name = f"torus_{a:g}x{b:g}".replace(".", "p")
    return Mesh(name, V, F, edge_lengths=lengths)


def torus_lattice_spectrum(nx: int, ny: int, a: float, b: float, k: int) -> np.ndarray:
    """Exact discrete spectrum of flat_torus via plane waves.

    On the uniform periodic grid both the cotan stiffness L and the lumped
    mass M are translation-invariant, so complex plane waves
    v[(i,j)] = exp(2*pi*1j*(m*i/nx + n*j/ny)) are exact eigenvectors. Returns
    the k smallest generalized eigenvalues (including the trivial 0).
    """
    from .laplacian import cotan_laplacian

    mesh = flat_torus(nx, ny, a, b)
    L, M = cotan_laplacian(mesh)
    Mdiag = M.diagonal()

    i = np.repeat(np.arange(nx), ny)
    j = np.tile(np.arange(ny), nx)
    lams = []
    for m in range(nx):
        for n in range(ny):
            v = np.exp(2j * np.pi * (m * i / nx + n * j / ny))
            Lv = L @ v
            lam = np.real(np.vdot(v, Lv) / np.vdot(v, Mdiag * v))
            residual = np.linalg.norm(Lv - lam * (Mdiag * v)) / max(np.linalg.norm(Lv), 1e-30)
            assert residual < 1e-9 or lam < 1e-12, f"plane wave ({m},{n}) not an eigenvector: residual {residual:.2e}"
            lams.append(lam)
    return np.sort(np.array(lams))[:k]


# ---------------------------------------------------------------- genus 2

def genus2(grid: int = 56) -> Mesh:
    """Genus-2 surface: smooth union of two unit tori, marching cubes, Taubin smoothing."""
    from skimage import measure

    R, r, sep = 1.0, 0.38, 1.25

    def torus_sdf(x, y, z, cx):
        q = np.sqrt((x - cx) ** 2 + y ** 2) - R
        return np.sqrt(q ** 2 + z ** 2) - r

    lim = sep + R + r + 0.3
    xs = np.linspace(-lim, lim, grid)
    ys = np.linspace(-(R + r + 0.3), R + r + 0.3, int(grid * (R + r + 0.3) / lim))
    zs = np.linspace(-(r + 0.25), r + 0.25, max(12, int(grid * (r + 0.25) / lim)))
    X, Y, Z = np.meshgrid(xs, ys, zs, indexing="ij")

    k = 8.0  # smooth-min sharpness
    f1, f2 = torus_sdf(X, Y, Z, -sep), torus_sdf(X, Y, Z, sep)
    field_vals = -np.log(np.exp(-k * f1) + np.exp(-k * f2)) / k

    spacing = (xs[1] - xs[0], ys[1] - ys[0], zs[1] - zs[0])
    verts, faces, _, _ = measure.marching_cubes(field_vals, level=0.0, spacing=spacing)
    verts, faces = _taubin_smooth(verts.astype(np.float64), faces.astype(np.int64))

    mesh = Mesh("genus2", verts, faces)
    assert mesh.genus() == 2, f"expected genus 2, got {mesh.genus()}"
    return mesh


def _taubin_smooth(V: np.ndarray, F: np.ndarray, iters: int = 12,
                   lam: float = 0.5, mu: float = -0.53):
    n = V.shape[0]
    pairs = np.concatenate([F[:, [0, 1]], F[:, [1, 2]], F[:, [2, 0]]])
    pairs = np.unique(np.sort(pairs, axis=1), axis=0)
    from scipy.sparse import coo_matrix
    A = coo_matrix((np.ones(len(pairs)), (pairs[:, 0], pairs[:, 1])), shape=(n, n))
    A = (A + A.T).tocsr()
    deg = np.asarray(A.sum(axis=1)).ravel()
    deg[deg == 0] = 1
    for _ in range(iters):
        for step in (lam, mu):
            V = V + step * ((A @ V) / deg[:, None] - V)
    return V, F


# ---------------------------------------------------------------- mandelbulb

def mandelbulb(grid: int = 64, power: int = 8, iters: int = 6, step_size: int = 1) -> Mesh:
    """Genus-0 mandelbulb isosurface: marching cubes on a signed escape field,
    Taubin-smoothed. Spiky, self-similar geometry -> strongly localized
    eigenfunctions, so the strike point becomes hyper-expressive and SHARPEN
    goes alien. The discrete object is the instrument; we don't care about
    fractal fidelity, only that it's a valid closed surface."""
    from skimage import measure

    lim = 1.25
    xs = np.linspace(-lim, lim, grid)
    X, Y, Z = np.meshgrid(xs, xs, xs, indexing="ij")
    pos = np.stack([X.ravel(), Y.ravel(), Z.ravel()], axis=1)

    z = pos.copy()
    dr = np.ones(len(pos))
    alive = np.ones(len(pos), dtype=bool)  # still inside (never escaped)
    for _ in range(iters):
        r = np.linalg.norm(z, axis=1)
        alive &= r <= 2.0                  # freeze escaped points (no overflow)
        rr = np.clip(r, 1e-9, 2.0)
        theta = np.arccos(np.clip(z[:, 2] / rr, -1, 1)) * power
        phi = np.arctan2(z[:, 1], z[:, 0]) * power
        dr_new = rr ** (power - 1) * power * dr + 1.0
        zr = rr ** power
        z_new = zr[:, None] * np.stack([
            np.sin(theta) * np.cos(phi),
            np.sin(theta) * np.sin(phi),
            np.cos(theta),
        ], axis=1) + pos
        z = np.where(alive[:, None], z_new, z)   # only advance live points
        dr = np.where(alive, dr_new, dr)

    r = np.clip(np.linalg.norm(z, axis=1), 1e-9, 1e6)
    de = 0.5 * np.log(r) * r / dr                  # distance estimate (>=0 outside)
    field = np.where(alive, -np.abs(de) - 1e-3, np.abs(de))  # signed: <0 inside
    field = field.reshape((grid, grid, grid))

    spacing = (xs[1] - xs[0],) * 3
    verts, faces, _, _ = measure.marching_cubes(field, level=0.0, spacing=spacing,
                                                step_size=step_size)
    verts, faces = _largest_component(verts.astype(np.float64), faces.astype(np.int64))
    verts = verts - verts.mean(axis=0)
    verts /= np.linalg.norm(verts, axis=1).max()   # normalize to ~unit radius
    verts, faces = _taubin_smooth(verts, faces, iters=8)

    mesh = Mesh("mandelbulb", verts, faces)
    assert mesh.euler_characteristic() % 2 == 0, "mandelbulb not a closed surface"
    return mesh


def _largest_component(V: np.ndarray, F: np.ndarray):
    """Keep only the largest connected component (drop marching-cubes specks)
    and remove now-unused vertices."""
    n = V.shape[0]
    parent = np.arange(n)

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    for tri in F:
        ra, rb, rc = find(tri[0]), find(tri[1]), find(tri[2])
        parent[rb] = ra
        parent[rc] = ra
    roots = np.array([find(i) for i in range(n)])
    labels, counts = np.unique(roots, return_counts=True)
    keep_root = labels[np.argmax(counts)]
    keep = roots == keep_root

    remap = -np.ones(n, dtype=np.int64)
    remap[keep] = np.arange(int(keep.sum()))
    Fk = F[np.all(keep[F], axis=1)]
    return V[keep], remap[Fk]


# ---------------------------------------------------------------- strike points

def strike_points(mesh: Mesh, count: int = 3) -> list[int]:
    """Deterministic, geometrically-spread strike vertices: an anchor, the
    farthest vertex from it (Euclidean), and the vertex nearest the centroid
    of those two (a 'mid' point)."""
    v0 = 0
    d = np.linalg.norm(mesh.V - mesh.V[v0], axis=1)
    v1 = int(np.argmax(d))
    mid_target = (mesh.V[v0] + mesh.V[v1]) / 2
    order = np.argsort(np.linalg.norm(mesh.V - mid_target, axis=1))
    v2 = next(int(i) for i in order if i not in (v0, v1))
    return [v0, v1, v2][:count]


# ---------------------------------------------------------------- OBJ export

def save_obj(mesh: Mesh, path: str) -> None:
    with open(path, "w") as fh:
        fh.write(f"# {mesh.name}  V={mesh.n_vertices} F={mesh.F.shape[0]} genus={mesh.genus()}\n")
        for v in mesh.V:
            fh.write(f"v {v[0]:.8f} {v[1]:.8f} {v[2]:.8f}\n")
        for f in mesh.F:
            fh.write(f"f {f[0]+1} {f[1]+1} {f[2]+1}\n")
