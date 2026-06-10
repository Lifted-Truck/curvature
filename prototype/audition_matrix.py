"""Render the CHECKPOINT 0 audition matrix.

{sphere, torus 1:1, torus 1:1.618, genus-2} x {3 strike points} x {static, flowing}
plus, per mesh, the claim-2 A/B control: same flow marginals, decorrelated
across modes (strike point 0).

Run:  python -m prototype.audition_matrix [out_dir]
Writes WAVs + a MANIFEST.md describing what to listen for.
"""

from __future__ import annotations

import sys
import time
from pathlib import Path

import numpy as np

from .flow import conformal_perturbation, decorrelate_trajectory, spectrum_trajectory
from .laplacian import cotan_laplacian, solve_modes
from .meshes import Mesh, flat_torus, genus2, icosphere, strike_points
from .render import RenderParams, render_flowing, render_static, write_wav

K_MODES = 96
STATIC_DUR = 7.0
FLOW_DUR = 9.0
FLOW_TAU = 2.5
FLOW_FRAMES = 25
PERTURB_AMP = 0.5


def preset_meshes() -> list[Mesh]:
    return [
        icosphere(3),                      # genus 0, 642 verts
        flat_torus(32, 24, 1.0, 1.0),      # genus 1, square lattice
        flat_torus(32, 24, 1.0, 1.618),    # genus 1, golden aspect
        genus2(),                          # genus 2
    ]


def main(out_dir: str = "renders/checkpoint0") -> None:
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    p = RenderParams()
    manifest = [
        "# CHECKPOINT 0 audition matrix",
        "",
        f"f_note={p.f_note} Hz, T60={p.t60} s, tilt={p.tilt}, mallet={p.mallet_cutoff} Hz, K={K_MODES} modes",
        "",
        "## What to listen for",
        "- **Claim 1 (genus taxonomy):** sphere ~ near-harmonic bell; tori ~ lattice/gamelan-ish;",
        "  genus-2 ~ dense structured shimmer. Do the families sound like different *kinds* of object?",
        "- **Claim 2 (coherence):** `*_flow` (correlated drift, one metric relaxing) vs",
        "  `*_flow_decorrelated` (same drift statistics, scrambled across partials).",
        "  Does the former sound like one object changing and the latter like N sliding sinusoids?",
        "- Strike points s0/s1/s2 are geometrically spread; nodal-line effects should",
        "  change which partials speak.",
        "",
        "## Files",
    ]

    for mesh in preset_meshes():
        t0 = time.time()
        L, M = cotan_laplacian(mesh)
        lam, phi = solve_modes(L, M, k=K_MODES)
        strikes = strike_points(mesh)
        print(f"{mesh.name}: {mesh.n_vertices} verts, genus {mesh.genus()}, "
              f"static solve {time.time()-t0:.1f}s, strikes {strikes}")

        u = conformal_perturbation(mesh, amplitude=PERTURB_AMP, seed=0)
        t0 = time.time()
        times, lam_traj = spectrum_trajectory(mesh, u, FLOW_DUR,
                                              n_frames=FLOW_FRAMES, tau=FLOW_TAU, k=K_MODES)
        print(f"  flow trajectory ({FLOW_FRAMES} frames): {time.time()-t0:.1f}s")

        # strike-point eigenfunction values on the *start* (perturbed) metric for flow renders
        from .laplacian import cotan_laplacian as _cl
        from .flow import _apply_conformal
        L_s, M_s = _cl(mesh, _apply_conformal(mesh, u))
        _, phi_start = solve_modes(L_s, M_s, k=K_MODES)

        lam_traj_decorr = decorrelate_trajectory(lam_traj, seed=1)

        for si, vtx in enumerate(strikes):
            name = f"{mesh.name}_s{si}_static.wav"
            write_wav(out / name, render_static(lam, phi[vtx], STATIC_DUR, p), p.sr)
            manifest.append(f"- `{name}` — static spectrum, strike vertex {vtx}")

            name = f"{mesh.name}_s{si}_flow.wav"
            write_wav(out / name, render_flowing(times, lam_traj, phi_start[vtx], FLOW_DUR, p), p.sr)
            manifest.append(f"- `{name}` — perturbed metric relaxing to base (tau={FLOW_TAU}s)")

        name = f"{mesh.name}_s0_flow_decorrelated.wav"
        write_wav(out / name, render_flowing(times, lam_traj_decorr, phi_start[strikes[0]], FLOW_DUR, p), p.sr)
        manifest.append(f"- `{name}` — A/B control: matched drift marginals, decorrelated across modes")

    (out / "MANIFEST.md").write_text("\n".join(manifest) + "\n")
    print(f"\nwrote {len(list(out.glob('*.wav')))} WAVs to {out}/")


if __name__ == "__main__":
    main(*sys.argv[1:])
