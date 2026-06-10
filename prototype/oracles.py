"""Phase 0 objective oracles (section 5 of the proposal).

  1. topology      — generated meshes have the claimed genus
  2. icosphere     — lowest eigenvalues cluster into (2l+1)-fold degenerate
                     groups whose means follow l(l+1) within tolerance
  3. torus_exact   — eigsh spectrum of the flat lattice torus matches the
                     exact discrete plane-wave spectrum to solver precision
  4. fft_peaks     — FFT peaks of a rendered static WAV sit within 1 cent of
                     the computed c*sqrt(lambda_k) targets (quadratic
                     sub-bin peak interpolation; only modes that are
                     resolvable, i.e. >= 2 Hz from their neighbors, with
                     non-negligible gain, are scored)

Run:  python -m prototype.oracles      (exit code 0 = all pass)
"""

from __future__ import annotations

import sys

import numpy as np

from .laplacian import cotan_laplacian, solve_modes
from .meshes import flat_torus, genus2, icosphere, torus_lattice_spectrum, strike_points
from .render import RenderParams, mode_frequencies, render_static

CENT = 2 ** (1 / 1200)


def oracle_topology():
    checks = [(icosphere(2), 0), (flat_torus(24, 18), 1), (genus2(), 2)]
    for mesh, g in checks:
        actual = mesh.genus()
        if actual != g:
            return False, f"{mesh.name}: genus {actual} != {g}"
    return True, f"genus 0/1/2 confirmed via Euler characteristic"


def oracle_icosphere(subdiv: int = 4, n_groups: int = 8, tol: float = 0.05):
    """Continuum sphere spectrum is l(l+1)/R^2 with multiplicity 2l+1.
    The discrete icosphere approximates this for low modes: check the group
    structure and the group-mean ratios."""
    mesh = icosphere(subdiv)
    L, M = cotan_laplacian(mesh)
    k = sum(2 * l + 1 for l in range(1, n_groups + 1))
    lam, _ = solve_modes(L, M, k=k)

    idx = 0
    for l in range(1, n_groups + 1):
        group = lam[idx: idx + 2 * l + 1]
        idx += 2 * l + 1
        spread = (group.max() - group.min()) / group.mean()
        if spread > tol:
            return False, f"l={l} group spread {spread:.3f} > {tol} (degeneracy broken)"
        ratio = group.mean() / lam[:3].mean()        # vs l=1 group mean
        expected = l * (l + 1) / 2.0
        if abs(ratio / expected - 1) > tol:
            return False, f"l={l} mean ratio {ratio:.3f} vs l(l+1)/2={expected:.3f}"
    return True, f"{n_groups} degenerate groups follow l(l+1) within {tol*100:.0f}%"


def oracle_torus_exact(nx: int = 32, ny: int = 24, a: float = 1.0, b: float = 1.618,
                       k: int = 64, rtol: float = 1e-6):
    mesh = flat_torus(nx, ny, a, b)
    L, M = cotan_laplacian(mesh)
    lam, _ = solve_modes(L, M, k=k)
    exact = torus_lattice_spectrum(nx, ny, a, b, k + 1)[1:]  # drop trivial 0
    err = np.max(np.abs(lam - exact) / exact)
    if err > rtol:
        return False, f"max relative error {err:.2e} > {rtol:.0e}"
    return True, f"{k} eigenvalues match exact discrete lattice spectrum (max rel err {err:.1e})"


def oracle_fft_peaks(tol_cents: float = 1.0):
    """Render a long, lightly-damped strike and verify partial frequencies."""
    mesh = icosphere(3)
    L, M = cotan_laplacian(mesh)
    lam, phi = solve_modes(L, M, k=48)
    strike = strike_points(mesh)[2]

    p = RenderParams(t60=30.0, tilt=0.0, mallet_cutoff=20000.0, f_note=220.0)
    duration = 20.0
    audio = render_static(lam, phi[strike], duration, p)

    targets = mode_frequencies(lam, p.f_note)
    gains = np.abs(phi[strike])
    audible = targets < 0.45 * p.sr
    targets, gains = targets[audible], gains[audible]

    # resolvable = strong enough to stand out and >= 2 Hz from any other target
    uniq = np.unique(np.round(targets, 6))
    sep = np.array([np.min(np.abs(uniq[uniq != np.round(f, 6)] - f), initial=np.inf)
                    for f in targets])
    resolvable = (gains > 0.1 * gains.max()) & (sep >= 2.0)
    if resolvable.sum() < 5:
        return False, f"only {resolvable.sum()} resolvable modes — bad strike point for the oracle"

    win = np.hanning(len(audio))
    spec = np.abs(np.fft.rfft(audio * win))
    df = p.sr / len(audio)

    worst = 0.0
    for f in targets[resolvable]:
        b0 = int(round(f / df))
        lo, hi = b0 - int(1.0 / df), b0 + int(1.0 / df) + 1
        b = lo + int(np.argmax(spec[lo:hi]))
        # quadratic sub-bin interpolation on log magnitude
        y0, y1, y2 = np.log(spec[b - 1: b + 2] + 1e-30)
        delta = 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2)
        f_est = (b + delta) * df
        cents = abs(1200 * np.log2(f_est / f))
        worst = max(worst, cents)
        if cents > tol_cents:
            return False, f"partial at {f:.2f} Hz measured {f_est:.2f} Hz ({cents:.2f} cents off)"
    return True, f"{int(resolvable.sum())} resolvable partials within {tol_cents} cent (worst {worst:.3f})"


ORACLES = [
    ("topology", oracle_topology),
    ("icosphere l(l+1)", oracle_icosphere),
    ("torus exact lattice", oracle_torus_exact),
    ("FFT peak <= 1 cent", oracle_fft_peaks),
]


def main() -> int:
    failed = 0
    for name, fn in ORACLES:
        try:
            ok, msg = fn()
        except Exception as exc:  # an oracle crashing is a failure, not an excuse
            ok, msg = False, f"raised {type(exc).__name__}: {exc}"
        status = "PASS" if ok else "FAIL"
        print(f"[{status}] {name}: {msg}")
        failed += 0 if ok else 1
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
