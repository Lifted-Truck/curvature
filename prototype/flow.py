"""Phase 0 'crude flow': metric interpolation in place of full Ricci flow.

Per the proposal (section 5, Phase 0), the perceptual claim under test is
*correlated drift* — all partials moving because one object's metric is
changing — not the specific Ricci dynamics. So:

  1. conformally perturb the base metric with a smooth random field
     (l'_ij = exp((u_i + u_j)/2) * l_ij — a curvature-bumped start state)
  2. relax the log edge lengths exponentially back to the base metric
     (the base mesh stands in for the precomputed uniformized target)
  3. sample the spectrum along the way, tracking modes across frames by
     mass-weighted eigenvector overlap so trajectories are continuous
     through eigenvalue crossings

Log-space interpolation keeps the path conformal-ish and (at sane
amplitudes) inside the valid-metric cone; validity is asserted every frame.

Also provides the decorrelated control for claim 2's A/B: the same per-mode
drift trajectories, but permuted across modes, sign-flipped, and time-lagged
at random — matched marginal drift statistics, broken cross-mode coherence.
"""

from __future__ import annotations

import numpy as np

from .laplacian import cotan_laplacian, face_cotans_and_areas, solve_modes
from .meshes import Mesh


def conformal_perturbation(mesh: Mesh, amplitude: float = 0.5,
                           smoothing_iters: int = 25, seed: int = 0) -> np.ndarray:
    """Smooth random per-vertex conformal factors u, scaled so the perturbed
    metric stays valid (all faces satisfy the triangle inequality)."""
    rng = np.random.default_rng(seed)
    u = rng.standard_normal(mesh.n_vertices)

    # smooth by repeated neighbor averaging (combinatorial heat steps)
    from scipy.sparse import coo_matrix
    pairs = np.concatenate([mesh.F[:, [0, 1]], mesh.F[:, [1, 2]], mesh.F[:, [2, 0]]])
    pairs = np.unique(np.sort(pairs, axis=1), axis=0)
    n = mesh.n_vertices
    A = coo_matrix((np.ones(len(pairs)), (pairs[:, 0], pairs[:, 1])), shape=(n, n))
    A = (A + A.T).tocsr()
    deg = np.asarray(A.sum(axis=1)).ravel()
    deg[deg == 0] = 1
    for _ in range(smoothing_iters):
        u = 0.5 * u + 0.5 * (A @ u) / deg

    u -= u.mean()
    u *= amplitude / max(np.abs(u).max(), 1e-12)

    while not _metric_valid(mesh, _apply_conformal(mesh, u)):
        u *= 0.7
    return u


def _apply_conformal(mesh: Mesh, u: np.ndarray) -> np.ndarray:
    F = mesh.F
    scale = np.stack([  # edge opposite corner j connects the other two corners
        np.exp(0.5 * (u[F[:, 1]] + u[F[:, 2]])),
        np.exp(0.5 * (u[F[:, 2]] + u[F[:, 0]])),
        np.exp(0.5 * (u[F[:, 0]] + u[F[:, 1]])),
    ], axis=1)
    return mesh.edge_lengths * scale


def _metric_valid(mesh: Mesh, lengths: np.ndarray) -> bool:
    try:
        face_cotans_and_areas(lengths)
        return True
    except ValueError:
        return False


def spectrum_schedule(mesh: Mesh, lengths_at, times: np.ndarray, k: int = 96):
    """Eigenvalue/eigenvector trajectories along an arbitrary metric schedule.

    lengths_at(t) -> per-face edge lengths at time t. Returns
    (lam_traj[n_frames, k], phi_frames list of (n, k)) with rows mode-tracked
    against frame 0 by mass-weighted eigenvector overlap.
    """
    lam_traj = np.empty((len(times), k))
    phi_frames = []
    prev_phi = None
    _, M = cotan_laplacian(mesh)
    Mdiag = M.diagonal()

    for fi, t in enumerate(times):
        L, M = cotan_laplacian(mesh, lengths_at(t))
        lam, phi = solve_modes(L, M, k=k)
        if prev_phi is not None:
            lam, phi = _match_modes(prev_phi, lam, phi, Mdiag)
        prev_phi = phi
        lam_traj[fi] = lam
        phi_frames.append(phi)

    return lam_traj, phi_frames


def spectrum_trajectory(mesh: Mesh, u_start: np.ndarray, duration: float,
                        n_frames: int = 25, tau: float = 2.5, k: int = 96):
    """Eigenvalue trajectories under exponential metric relaxation.

    Returns (times[n_frames], lam_traj[n_frames, k]) with rows mode-tracked
    against frame 0 by mass-weighted eigenvector overlap.
    """
    log_base = np.log(mesh.edge_lengths)
    log_start = np.log(_apply_conformal(mesh, u_start))

    times = np.linspace(0.0, duration, n_frames)

    def lengths_at(t):
        alpha = np.exp(-t / tau)
        return np.exp(log_base + alpha * (log_start - log_base))

    lam_traj, _ = spectrum_schedule(mesh, lengths_at, times, k=k)
    return times, lam_traj


def _match_modes(prev_phi: np.ndarray, lam: np.ndarray, phi: np.ndarray,
                 Mdiag: np.ndarray):
    """Greedy assignment of current modes to previous frame's mode slots by
    |Phi_prev^T M Phi_cur| overlap, so trajectories stay continuous through
    eigenvalue crossings."""
    overlap = np.abs(prev_phi.T @ (Mdiag[:, None] * phi))  # (k_prev, k_cur)
    k = lam.shape[0]
    assign = np.full(k, -1)
    taken = np.zeros(k, dtype=bool)
    # visit pairs in decreasing overlap order
    flat = np.argsort(overlap.ravel())[::-1]
    filled = 0
    for idx in flat:
        slot, cur = divmod(idx, k)
        if assign[slot] == -1 and not taken[cur]:
            assign[slot] = cur
            taken[cur] = True
            filled += 1
            if filled == k:
                break
    return lam[assign], phi[:, assign]


def decorrelate_trajectory(lam_traj: np.ndarray, seed: int = 1) -> np.ndarray:
    """Decorrelated-drift control for the claim-2 A/B.

    Each mode's log-eigenvalue drift d_k(t) = log lam_k(t) - log lam_k(end)
    is taken from the real flow, but reassigned to a random other mode,
    randomly sign-flipped, and circularly time-lagged. Marginal drift
    magnitudes and timescales match the real flow; cross-mode coherence and
    the shared 'one object relaxing' structure do not.
    """
    rng = np.random.default_rng(seed)
    n_frames, k = lam_traj.shape
    log_lam = np.log(lam_traj)
    drift = log_lam - log_lam[-1]                  # (n_frames, k), -> 0 at end

    perm = rng.permutation(k)
    signs = rng.choice([-1.0, 1.0], size=k)
    out = np.empty_like(drift)
    for m in range(k):
        d = drift[:, perm[m]] * signs[m]
        lag = rng.integers(0, n_frames // 3 + 1)
        d = np.concatenate([np.full(lag, d[0]), d[: n_frames - lag]])
        out[:, m] = d
    return np.exp(out + log_lam[-1])
