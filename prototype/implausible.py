"""Implausible-object demos: sounds whose material signifiers physics forbids.

The checkpoint-0 coherence result is the enabling fact: listeners fuse
correlated partials into *one object* regardless of whether that object
could exist. So every material signifier — what the object is, how big it
is, how it absorbs energy — becomes a free compositional parameter, glued
into objecthood by a single evolving metric.

  1. morph_gamelan_to_string — an object that *becomes a different object*
     mid-phrase: a clustered inharmonic torus metric morphing into the 8:1
     harmonic-string metric while being struck. The intermediate metrics are
     curved states no luthier could build.
  2. bell_anti_acoustic — negative damping tilt: the fundamental thump dies
     and the high shimmer outlasts it. Every passive object does the opposite.
  3. rubber_bell — a struck bell whose *size* oscillates during ring-out
     (eigenvalues scale as 1/size^2; the whole spectrum breathes coherently).
  4. comb_damped_genus2 — a "material" that selectively absorbs every other
     mode: long-ring and near-mute partials interleaved across the spectrum.

Renders to renders/checkpoint0_implausible/. Run:
    python -m prototype.implausible
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

from .flow import spectrum_schedule
from .laplacian import cotan_laplacian, face_cotans_and_areas, solve_modes
from .meshes import flat_torus, genus2, icosphere, strike_points
from .render import (RenderParams, _normalize, render_flowing, render_static,
                     write_wav)

K = 96


def _strike_mix(times, lam_traj, phi_frames, vtx, strike_times, duration, p):
    """Several strikes landing on an evolving metric. Each strike couples
    through the eigenfunctions of the metric *as it is at that moment*."""
    out = np.zeros(int(duration * p.sr))
    for s in strike_times:
        fi = int(np.argmin(np.abs(times - s)))
        seg = render_flowing(times - s, lam_traj, phi_frames[fi][vtx],
                             duration - s, p, normalize=False)
        i0 = int(s * p.sr)
        out[i0: i0 + len(seg)] += seg
    return _normalize(out, p.peak_db)


def morph_gamelan_to_string(p: RenderParams):
    """Fixed connectivity, metric A (clustered inharmonic torus) morphing to
    metric B (8:1 harmonic torus, area-matched so the morph is shape, not size)."""
    nx, ny = 48, 12
    mesh = flat_torus(nx, ny, 2.0, 1.2)
    log_a = np.log(mesh.edge_lengths)
    lengths_b = flat_torus(nx, ny, 8.0, 1.0).edge_lengths
    _, area_a = face_cotans_and_areas(mesh.edge_lengths)
    _, area_b = face_cotans_and_areas(lengths_b)
    log_b = np.log(lengths_b * np.sqrt(area_a.sum() / area_b.sum()))

    duration = 12.0

    def lengths_at(t):
        x = np.clip(t / duration, 0.0, 1.0)
        alpha = x * x * (3 - 2 * x)  # smoothstep
        return np.exp((1 - alpha) * log_a + alpha * log_b)

    times = np.linspace(0.0, duration, 31)
    lam_traj, phi_frames = spectrum_schedule(mesh, lengths_at, times, k=K)
    vtx = strike_points(mesh)[2]
    return _strike_mix(times, lam_traj, phi_frames, vtx,
                       [0.0, 3.0, 6.0, 9.0], duration, p), duration


def rubber_bell(p: RenderParams):
    """Struck icosphere whose global scale wobbles while ringing: a decaying
    size oscillation, lam(t) = lam / s(t)^2. No re-solves needed — uniform
    conformal scaling moves the whole spectrum exactly."""
    mesh = icosphere(3)
    L, M = cotan_laplacian(mesh)
    lam, phi = solve_modes(L, M, k=K)
    duration = 8.0
    times = np.linspace(0.0, duration, 65)
    s = 1.0 + 0.10 * np.sin(2 * np.pi * 1.3 * times) * np.exp(-times / 2.5)
    lam_traj = lam[None, :] / s[:, None] ** 2
    vtx = strike_points(mesh)[2]
    return render_flowing(times, lam_traj, phi[vtx], duration, p), duration


def main(out_dir: str = "renders/checkpoint0_implausible") -> None:
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    manifest = [
        "# Implausible objects — material signifiers physics forbids",
        "",
        "Coherence (checkpoint-0 claim 2) is what keeps these reading as",
        "*objects* even though no such object can exist.",
        "",
    ]

    def emit(name, audio, sr, note):
        write_wav(out / f"{name}.wav", audio, sr)
        manifest.append(f"- `{name}.wav` — {note}")
        print(f"  {name}")

    print("rendering implausible objects:")

    p = RenderParams(t60=6.0, tilt=0.8, mallet_cutoff=2600.0, f_note=165.0)
    audio, _ = morph_gamelan_to_string(p)
    emit("morph_gamelan_to_string", audio, p.sr,
         "struck 4x while its metric morphs from clustered-inharmonic to exact "
         "harmonic series: one object *becoming a different instrument* mid-phrase")

    mesh = icosphere(3)
    L, M = cotan_laplacian(mesh)
    lam, phi = solve_modes(L, M, k=K)
    vtx = strike_points(mesh)[2]
    p = RenderParams(t60=2.5, tilt=-0.6, mallet_cutoff=9000.0, f_note=440.0)
    emit("bell_anti_acoustic", render_static(lam, phi[vtx], 10.0, p), p.sr,
         "negative damping tilt: lows die first, shimmer outlasts the thump — "
         "decay structure of a time-reversed material")

    p = RenderParams(t60=7.0, tilt=0.6, mallet_cutoff=3000.0, f_note=220.0)
    audio, _ = rubber_bell(p)
    emit("rubber_bell", audio, p.sr,
         "bronze attack, balloon behavior: the object's size oscillates during "
         "ring-out and the whole spectrum breathes as one")

    mesh = genus2()
    L, M = cotan_laplacian(mesh)
    lam, phi = solve_modes(L, M, k=K)
    vtx = strike_points(mesh)[2]
    p = RenderParams(t60=8.0, tilt=0.0, mallet_cutoff=4000.0, f_note=146.8)
    t60_comb = np.where(np.arange(K) % 2 == 0, 8.0, 0.22)
    emit("comb_damped_genus2", render_static(lam, phi[vtx], 9.0, p, t60_per_mode=t60_comb), p.sr,
         "a substance that absorbs every other mode: interleaved long-ring and "
         "near-mute partials — selective absorption no homogeneous material has")

    (out / "MANIFEST.md").write_text("\n".join(manifest) + "\n")
    print(f"wrote {len(list(out.glob('*.wav')))} WAVs to {out}/")


if __name__ == "__main__":
    if len(sys.argv) > 2:
        sys.exit(f"usage: python -m prototype.implausible [out_dir]  (got extra args {sys.argv[2:]})")
    main(*sys.argv[1:2])
