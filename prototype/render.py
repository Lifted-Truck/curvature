"""Offline modal renderer (section 3 of the proposal).

Spectrum mapping:  f_k = c * sqrt(lambda_k),  c = f_note / sqrt(lambda_1)
                   (fundamental tracks the first nonzero eigenvalue; the
                   manifold defines interval structure, the keyboard the register)
Excitation:        per-mode energy ~ phi_k(p) at strike vertex p, shaped by a
                   'mallet' lowpass over mode frequency
Damping:           T60_k = T60_global * (f_1 / f_k)^tilt  (section 3.3)

Static renders use fixed frequencies; flowing renders integrate
phase through time-varying frequencies interpolated (cubic, in log space)
from a spectrum trajectory.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np
import soundfile as sf
from scipy.interpolate import CubicSpline

LN_1000 = np.log(1000.0)  # T60 = time to decay 60 dB


@dataclass
class RenderParams:
    sr: int = 48000
    f_note: float = 220.0       # fundamental (Hz) — keyboard register
    t60: float = 5.0            # global T60 (s)
    tilt: float = 0.7           # 0 = all modes ring equally, 1 ~ acoustic-plausible
    mallet_cutoff: float = 2200.0  # Hz; lower = softer mallet
    mallet_order: int = 2
    peak_db: float = -3.0


def mode_frequencies(lam: np.ndarray, f_note: float) -> np.ndarray:
    return f_note * np.sqrt(lam / lam[0])


def _gains(freqs: np.ndarray, phi_at_strike: np.ndarray, p: RenderParams) -> np.ndarray:
    mallet = 1.0 / (1.0 + (freqs / p.mallet_cutoff) ** (2 * p.mallet_order))
    return phi_at_strike * mallet


def _decay_rates(freqs: np.ndarray, p: RenderParams,
                 t60_per_mode: np.ndarray = None) -> np.ndarray:
    """Damping is a design choice, not physics: tilt may be negative
    (highs outlast lows) and t60_per_mode may be any pattern at all."""
    t60_k = p.t60 * (freqs[0] / freqs) ** p.tilt if t60_per_mode is None else t60_per_mode
    return LN_1000 / t60_k


def render_static(lam: np.ndarray, phi_at_strike: np.ndarray, duration: float,
                  p: RenderParams, t60_per_mode: np.ndarray = None,
                  normalize: bool = True) -> np.ndarray:
    freqs = mode_frequencies(lam, p.f_note)
    audible = freqs < 0.45 * p.sr
    gains = _gains(freqs, phi_at_strike, p)[audible]
    rates = _decay_rates(freqs, p, t60_per_mode)[audible]
    freqs = freqs[audible]

    t = np.arange(int(duration * p.sr)) / p.sr
    out = np.zeros_like(t)
    for f, g, r in zip(freqs, gains, rates):
        out += g * np.exp(-r * t) * np.sin(2 * np.pi * f * t)
    return _normalize(out, p.peak_db) if normalize else out


def render_flowing(times: np.ndarray, lam_traj: np.ndarray,
                   phi_at_strike: np.ndarray, duration: float,
                   p: RenderParams, normalize: bool = True) -> np.ndarray:
    """Single strike at t=0; frequencies drift along the trajectory while the
    modes ring out. Gains/decays are set from the t=0 spectrum (the strike
    happens on the start metric)."""
    n = int(duration * p.sr)
    k = lam_traj.shape[1]

    f0 = mode_frequencies(lam_traj[0], p.f_note)
    c = p.f_note / np.sqrt(lam_traj[0][0])
    audible = f0 < 0.45 * p.sr
    gains = _gains(f0, phi_at_strike, p)[audible]
    rates = _decay_rates(f0, p)[audible]
    lam_traj = lam_traj[:, audible]

    # cubic interpolation of log-frequency trajectories onto the sample clock
    spline = CubicSpline(times, np.log(c * np.sqrt(lam_traj)), axis=0)
    t = np.arange(n) / p.sr
    t_clamped = np.clip(t, times[0], times[-1])

    out = np.zeros(n)
    chunk = 1 << 16
    phase = np.zeros(np.count_nonzero(audible))
    for s in range(0, n, chunk):
        e = min(s + chunk, n)
        freqs = np.exp(spline(t_clamped[s:e]))            # (chunk, k_aud)
        dphase = 2 * np.pi * freqs / p.sr
        ph = phase + np.cumsum(dphase, axis=0)
        env = gains * np.exp(-rates * t[s:e, None])
        out[s:e] = np.sum(env * np.sin(ph), axis=1)
        phase = ph[-1]
    return _normalize(out, p.peak_db) if normalize else out


def render_sustained(lam: np.ndarray, phi_at_strike: np.ndarray, duration: float,
                     p: RenderParams, attack: float = 1.0, release: float = 1.5,
                     mod_depth: float = 0.6, mod_rate: float = 1.4,
                     seed: int = 0) -> np.ndarray:
    """Crude bowed/blown excitation preview (the real friction model is
    Phase 2): each mode sustains at its coupling gain, slowly and
    independently modulated by band-limited noise — the stochastic energy of
    a bow without per-sample noise synthesis."""
    rng = np.random.default_rng(seed)
    freqs = mode_frequencies(lam, p.f_note)
    audible = freqs < 0.45 * p.sr
    gains = _gains(freqs, phi_at_strike, p)[audible]
    freqs = freqs[audible]
    k = freqs.shape[0]

    n = int(duration * p.sr)
    t = np.arange(n) / p.sr

    # smooth per-mode gain modulation: cubic spline through random keyframes
    n_keys = max(4, int(duration * mod_rate) + 2)
    key_t = np.linspace(0, duration, n_keys)
    keys = 1.0 + mod_depth * rng.standard_normal((n_keys, k))
    mod = np.clip(CubicSpline(key_t, keys, axis=0)(t), 0.0, None)

    env = np.minimum(1.0, t / attack) * np.minimum(1.0, (duration - t) / release)
    env = np.sin(0.5 * np.pi * np.clip(env, 0, 1))  # cosine-ish edges

    out = np.zeros(n)
    for m in range(k):
        out += gains[m] * mod[:, m] * np.sin(2 * np.pi * freqs[m] * t)
    return _normalize(out * env, p.peak_db)


def _normalize(x: np.ndarray, peak_db: float) -> np.ndarray:
    peak = np.abs(x).max()
    if peak == 0:
        return x
    return x * (10 ** (peak_db / 20.0) / peak)


def write_wav(path: str, audio: np.ndarray, sr: int) -> None:
    sf.write(path, audio.astype(np.float32), sr, subtype="PCM_16")
