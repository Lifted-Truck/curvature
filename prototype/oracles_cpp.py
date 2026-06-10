"""Phase 1 oracle: the C++ engine's rendered audio (render_offline, which
runs the real ModalVoice DSP) must put FFT peaks within 1 cent of the
Python-computed c*sqrt(lambda_k) targets from tests/data/spectra/.

Run after building:  .venv/bin/python -m prototype.oracles_cpp [build_dir]
Exit 0 = pass.
"""

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
import soundfile as sf

SR = 48000
F_NOTE = 220.0
TOL_CENTS = 1.0


def main(build_dir: str = "build") -> int:
    binary = Path(build_dir) / "render_offline"
    if not binary.exists():
        print(f"[FAIL] render_offline not built at {binary}")
        return 1

    lam = np.array([float(l) for l in open("tests/data/spectra/icosphere3.txt")
                    if not l.startswith("#")])
    targets_all = F_NOTE * np.sqrt(lam / lam[0])

    with tempfile.TemporaryDirectory() as td:
        wav = Path(td) / "oracle.wav"
        subprocess.run([str(binary), "--preset", "0", "--note-hz", str(F_NOTE),
                        "--strike", "0.42", "--modes", "48", "--t60", "30",
                        "--tilt", "0", "--mallet", "20000", "--seconds", "20",
                        "--sr", str(SR), "--out", str(wav)],
                       check=True, capture_output=True)
        audio, sr = sf.read(wav)

    # match the Python rig's resolvability rule: >= 2 Hz separation
    targets = targets_all[:48]
    targets = targets[targets < 0.45 * sr]
    sep = np.array([np.min(np.abs(np.delete(np.unique(np.round(targets, 6)),
                    np.where(np.unique(np.round(targets, 6)) == np.round(f, 6)))  - f), initial=np.inf)
                    for f in targets])
    resolvable = sep >= 2.0

    win = np.hanning(len(audio))
    spec = np.abs(np.fft.rfft(audio * win))
    df = sr / len(audio)
    floor = spec.max() * 1e-4

    checked, worst = 0, 0.0
    for f in targets[resolvable]:
        b0 = int(round(f / df))
        lo, hi = b0 - int(1.0 / df), b0 + int(1.0 / df) + 1
        b = lo + int(np.argmax(spec[lo:hi]))
        if spec[b] < floor:
            continue  # mode not excited at this strike point (nodal line)
        y0, y1, y2 = np.log(spec[b - 1: b + 2] + 1e-30)
        delta = 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2)
        cents = abs(1200 * np.log2((b + delta) * df / f))
        worst = max(worst, cents)
        checked += 1
        if cents > TOL_CENTS:
            print(f"[FAIL] C++ FFT oracle: partial {f:.2f} Hz off by {cents:.2f} cents")
            return 1

    if checked < 5:
        print(f"[FAIL] C++ FFT oracle: only {checked} partials checkable")
        return 1
    print(f"[PASS] C++ FFT oracle: {checked} partials within {TOL_CENTS} cent of "
          f"c*sqrt(lambda) (worst {worst:.3f})")
    return 0


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:2]))
