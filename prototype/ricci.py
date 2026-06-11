"""Chow-Luo combinatorial Ricci flow (inversive-distance circle packing).

Metric state: per-vertex log radii u_i plus fixed per-edge inversive
distances I_ij captured from the initial metric. Edge lengths are
    l_ij^2 = g_i^2 + g_j^2 + 2 I_ij g_i g_j,   g = exp(u)
so the flow stays inside a conformal class of the *discrete* metric.

Flow:  du_i/dt = rate * (Kbar - K_i),  K_i = 2*pi - sum of corner angles,
Kbar = 2*pi*chi/V (Gauss-Bonnet target; uniform curvature = the topological
attractor). Gauss-Bonnet holds exactly in the discrete setting, so
sum(Kbar - K) = 0 and total log-scale is conserved by the continuous flow.

Reverse flow (SHARPEN) is the same vector field negated, with hard clamps on
|u_i - u_i(0)| and on triangle-inequality stress — the ill-posed direction
is treated as bounded expressive territory (PROPOSAL.md section 7).

Integrator: explicit Euler with step-halving retry on metric validity.
"""

from __future__ import annotations

import numpy as np

from .laplacian import face_cotans_and_areas
from .meshes import Mesh


class RicciFlow:
    def __init__(self, mesh: Mesh, u_clamp: float = 1.5):
        self.mesh = mesh
        self.F = mesh.F
        self.u_clamp = u_clamp

        n = mesh.n_vertices
        # vertex-edge structures from the face list
        pairs = np.concatenate([self.F[:, [1, 2]], self.F[:, [2, 0]], self.F[:, [0, 1]]])
        pairs = np.unique(np.sort(pairs, axis=1), axis=0)
        self.edges = pairs                      # (E, 2)
        self.edge_index = {tuple(e): k for k, e in enumerate(pairs)}

        # initial radii: third of the min incident edge length keeps all
        # inversive distances positive (separated circles)
        base_l = self._edge_lengths_from_faces(mesh.edge_lengths)
        g0 = np.full(n, np.inf)
        for (a, b), l in zip(self.edges, base_l):
            g0[a] = min(g0[a], l / 3.0)
            g0[b] = min(g0[b], l / 3.0)
        self.u0 = np.log(g0)
        self.u = self.u0.copy()

        # inversive distances fixed by the initial metric
        ga, gb = g0[self.edges[:, 0]], g0[self.edges[:, 1]]
        self.inv_dist = (base_l ** 2 - ga ** 2 - gb ** 2) / (2.0 * ga * gb)
        assert np.all(self.inv_dist > 0), "initial radii too large for inversive packing"

        chi = mesh.euler_characteristic()
        self.K_target = 2.0 * np.pi * chi / n

        # face -> edge-row lookup, column j = edge opposite corner j
        self.face_edge = np.empty_like(self.F)
        for fi, (a, b, c) in enumerate(self.F):
            for j, (p, q) in enumerate(((b, c), (c, a), (a, b))):
                self.face_edge[fi, j] = self.edge_index[(min(p, q), max(p, q))]

    def _edge_lengths_from_faces(self, face_lengths: np.ndarray) -> np.ndarray:
        out = np.zeros(len(self.edges))
        for fi, (a, b, c) in enumerate(self.F):
            for j, (p, q) in enumerate(((b, c), (c, a), (a, b))):
                out[self.edge_index[(min(p, q), max(p, q))]] = face_lengths[fi, j]
        return out

    def edge_lengths(self, u: np.ndarray = None) -> np.ndarray:
        """Per-edge lengths from current (or given) log radii."""
        u = self.u if u is None else u
        g = np.exp(u)
        ga, gb = g[self.edges[:, 0]], g[self.edges[:, 1]]
        return np.sqrt(ga ** 2 + gb ** 2 + 2.0 * self.inv_dist * ga * gb)

    def face_lengths(self, u: np.ndarray = None) -> np.ndarray:
        return self.edge_lengths(u)[self.face_edge]

    def curvatures(self, u: np.ndarray = None) -> np.ndarray:
        l = self.face_lengths(u)
        l0, l1, l2 = l[:, 0], l[:, 1], l[:, 2]
        angles = np.stack([
            np.arccos(np.clip((l1**2 + l2**2 - l0**2) / (2*l1*l2), -1, 1)),
            np.arccos(np.clip((l2**2 + l0**2 - l1**2) / (2*l2*l0), -1, 1)),
            np.arccos(np.clip((l0**2 + l1**2 - l2**2) / (2*l0*l1), -1, 1)),
        ], axis=1)
        K = np.full(self.mesh.n_vertices, 2.0 * np.pi)
        np.subtract.at(K, self.F.ravel(), angles.ravel())
        return K

    def curvature_error(self) -> float:
        return float(np.max(np.abs(self.curvatures() - self.K_target)))

    def _valid(self, u: np.ndarray) -> bool:
        try:
            face_cotans_and_areas(self.face_lengths(u))
            return True
        except ValueError:
            return False

    def step(self, dt: float, direction: float = 1.0) -> float:
        """One explicit Euler step (direction -1 = SHARPEN). Halves dt until
        the metric stays valid AND (forward flow only) the curvature error
        does not increase — explicit Euler overshoots when dt crosses the
        packing stiffness, and the monotone guard is both the stability
        criterion and the Phase 2 oracle. Returns the dt taken (0 = stuck)."""
        K = self.curvatures()
        du = direction * (self.K_target - K)
        err_before = float(np.max(np.abs(K - self.K_target)))
        while dt > 1e-7:
            u_new = self.u + dt * du
            u_new = self.u0 + np.clip(u_new - self.u0, -self.u_clamp, self.u_clamp)
            if self._valid(u_new):
                if direction <= 0:
                    self.u = u_new
                    return dt
                err_after = float(np.max(np.abs(self.curvatures(u_new) - self.K_target)))
                if err_after <= err_before + 1e-12:
                    self.u = u_new
                    return dt
            dt *= 0.5
        return 0.0

    def perturb(self, amplitude: float = 0.6, seed: int = 0,
                smoothing_iters: int = 20) -> None:
        """Smooth random conformal kick (curvature concentration), the test
        initial condition for forward-flow convergence."""
        rng = np.random.default_rng(seed)
        w = rng.standard_normal(self.mesh.n_vertices)
        adj = {}
        for a, b in self.edges:
            adj.setdefault(a, []).append(b)
            adj.setdefault(b, []).append(a)
        for _ in range(smoothing_iters):
            w = 0.5 * w + 0.5 * np.array([np.mean(w[adj[i]]) for i in range(len(w))])
        w -= w.mean()
        w *= amplitude / max(np.abs(w).max(), 1e-12)
        u_new = self.u + w
        while not self._valid(u_new):
            w *= 0.7
            u_new = self.u + w
        self.u = u_new
