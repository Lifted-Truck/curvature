"""Timbre-palette demo: how far the engine moves from 'metallic clang'
without changing the synthesis model at all — only damping design,
excitation class, geometry-as-harmonicity, register, and mode count.

Renders to renders/checkpoint0_palette/. Run:
    python -m prototype.palette
"""

from __future__ import annotations

import sys
from pathlib import Path

from .laplacian import cotan_laplacian, solve_modes
from .meshes import flat_torus, genus2, icosphere, strike_points
from .render import RenderParams, render_static, render_sustained, write_wav

K = 96


def _modes(mesh, k=K):
    L, M = cotan_laplacian(mesh)
    lam, phi = solve_modes(L, M, k=k)
    return lam, phi, strike_points(mesh)


def main(out_dir: str = "renders/checkpoint0_palette") -> None:
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    manifest = [
        "# Timbre palette — same engine, different corners of parameter space",
        "",
        "Axes demonstrated: damping (T60/tilt), mallet, register, mode count,",
        "geometry-as-harmonicity (elongated torus -> harmonic series), and",
        "sustained excitation (crude Phase 2 bow preview).",
        "",
    ]

    def emit(name, audio, sr, note):
        write_wav(out / f"{name}.wav", audio, sr)
        manifest.append(f"- `{name}.wav` — {note}")
        print(f"  {name}")

    # reference family: what checkpoint0 already sounds like
    lam_i, phi_i, st_i = _modes(icosphere(3))
    lam_g, phi_g, st_g = _modes(genus2())
    lam_t, phi_t, st_t = _modes(flat_torus(32, 24, 1.0, 1.618))
    # elongated torus: first ~8 modes are an exact harmonic series (n=0 row of
    # the lattice spectrum dominates when b << a)
    lam_s, phi_s, st_s = _modes(flat_torus(96, 12, 8.0, 1.0))

    print("rendering palette:")

    p = RenderParams(t60=0.35, tilt=1.3, mallet_cutoff=4000.0, f_note=330.0)
    emit("wood_icosphere", render_static(lam_i, phi_i[st_i[2]], 3.0, p), p.sr,
         "T60 0.35s, steep tilt: marimba/woodblock family — damping alone leaves 'metal'")

    p = RenderParams(t60=0.9, tilt=0.8, mallet_cutoff=600.0, f_note=65.0)
    emit("membrane_torus", render_static(lam_t, phi_t[st_t[1]], 3.0, p), p.sr,
         "low register, soft mallet, sub-second decay: tom/membrane family")

    p = RenderParams(t60=14.0, tilt=0.25, mallet_cutoff=700.0, f_note=55.0)
    emit("gong_genus2", render_static(lam_g, phi_g[st_g[2]], 14.0, p), p.sr,
         "genus-2 density at 55 Hz with 14 s ring: dark gong/tam-tam wash")

    p = RenderParams(t60=6.0, tilt=1.0, mallet_cutoff=8000.0, f_note=880.0)
    emit("glass_icosphere_12modes", render_static(lam_i[:12], phi_i[st_i[0]][:12], 6.0, p), p.sr,
         "only 12 modes, high register: glass/chime/music-box — mode count as a simplicity knob")

    p = RenderParams(t60=4.0, tilt=1.1, mallet_cutoff=2500.0, f_note=110.0)
    emit("string_torus_8to1", render_static(lam_s, phi_s[st_s[2]], 5.0, p), p.sr,
         "8:1 flat torus: lowest modes form an exact harmonic series -> plucked-string family, from geometry alone")

    p = RenderParams(mallet_cutoff=1200.0, f_note=110.0)
    emit("drone_genus2_bowed", render_sustained(lam_g, phi_g[st_g[0]], 16.0, p, mod_depth=0.7), p.sr,
         "sustained excitation of the dense genus-2 spectrum: shimmer drone/pad (crude Phase 2 bow preview)")

    p = RenderParams(mallet_cutoff=900.0, f_note=110.0)
    emit("organ_torus_8to1_bowed", render_sustained(lam_s, phi_s[st_s[0]], 12.0, p,
                                                    mod_depth=0.35, mod_rate=0.8), p.sr,
         "sustained harmonic-series torus: organ/bowed-string family")

    (out / "MANIFEST.md").write_text("\n".join(manifest) + "\n")
    print(f"wrote {len(list(out.glob('*.wav')))} WAVs to {out}/")


if __name__ == "__main__":
    if len(sys.argv) > 2:
        sys.exit(f"usage: python -m prototype.palette [out_dir]  (got extra args {sys.argv[2:]})")
    main(*sys.argv[1:2])
