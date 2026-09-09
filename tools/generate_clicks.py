#!/usr/bin/env python3
"""Procedurally generate the 10 stock click samples shipped with the Click plugin.

Every sample is synthesised from scratch (no third-party audio is used), so the
resulting WAV files are original content and safe to redistribute with the
plugin. Run this from the repository root:

    python3 tools/generate_clicks.py

Output: resources/clicks/*.wav  (48 kHz, mono, 24-bit PCM)
"""

from __future__ import annotations

import math
import struct
import wave
from pathlib import Path

import numpy as np

SAMPLE_RATE = 48_000
OUT_DIR = Path(__file__).resolve().parent.parent / "resources" / "clicks"

rng = np.random.default_rng(20240517)


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

def t_axis(ms: float) -> np.ndarray:
    """Time axis in seconds for a buffer of the given length in milliseconds."""
    return np.arange(int(SAMPLE_RATE * ms / 1000.0), dtype=np.float64) / SAMPLE_RATE


def env_ad(t: np.ndarray, attack_ms: float, decay_ms: float, curve: float = 1.0) -> np.ndarray:
    """Attack/decay envelope. `curve` > 1 makes the decay tail snappier."""
    attack = np.clip(t / max(attack_ms / 1000.0, 1e-6), 0.0, 1.0)
    decay = np.exp(-t / max(decay_ms / 1000.0, 1e-6)) ** curve
    return attack * decay


def biquad(x: np.ndarray, b0, b1, b2, a1, a2) -> np.ndarray:
    """Direct-form-I biquad. Small buffers, so a Python loop is fast enough."""
    y = np.zeros_like(x)
    x1 = x2 = y1 = y2 = 0.0
    for n in range(len(x)):
        xn = x[n]
        yn = b0 * xn + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        y[n] = yn
        x2, x1 = x1, xn
        y2, y1 = y1, yn
    return y


def _rbj(kind: str, freq: float, q: float):
    w0 = 2.0 * math.pi * freq / SAMPLE_RATE
    cos_w0, sin_w0 = math.cos(w0), math.sin(w0)
    alpha = sin_w0 / (2.0 * q)

    if kind == "lowpass":
        b0, b1, b2 = (1 - cos_w0) / 2, 1 - cos_w0, (1 - cos_w0) / 2
    elif kind == "highpass":
        b0, b1, b2 = (1 + cos_w0) / 2, -(1 + cos_w0), (1 + cos_w0) / 2
    elif kind == "bandpass":  # constant peak gain
        b0, b1, b2 = alpha, 0.0, -alpha
    else:
        raise ValueError(kind)

    a0, a1, a2 = 1 + alpha, -2 * cos_w0, 1 - alpha
    return b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0


def lowpass(x, freq, q=0.707):
    return biquad(x, *_rbj("lowpass", freq, q))


def highpass(x, freq, q=0.707):
    return biquad(x, *_rbj("highpass", freq, q))


def bandpass(x, freq, q=1.0):
    return biquad(x, *_rbj("bandpass", freq, q))


def noise(n: int) -> np.ndarray:
    return rng.uniform(-1.0, 1.0, n)


def finalise(x: np.ndarray, peak_db: float = -1.0) -> np.ndarray:
    """DC-block, normalise to `peak_db`, and force a clean zero ending."""
    x = x - np.mean(x)

    # short fade-out over the last 2 ms so the sample can never pop on release
    fade = min(int(SAMPLE_RATE * 0.002), len(x))
    if fade > 1:
        x[-fade:] *= np.linspace(1.0, 0.0, fade)

    peak = np.max(np.abs(x))
    if peak > 0.0:
        x = x / peak * (10.0 ** (peak_db / 20.0))
    return x


# ---------------------------------------------------------------------------
# the ten stock clicks
# ---------------------------------------------------------------------------

def studio_beep() -> np.ndarray:
    """The classic DAW metronome beep: pure 1 kHz sine, fast exponential decay."""
    t = t_axis(90)
    tone = np.sin(2 * np.pi * 1000.0 * t)
    return finalise(tone * env_ad(t, 0.7, 22.0))


def wood_block() -> np.ndarray:
    """Resonant wooden knock: two body modes excited by a tight noise burst."""
    t = t_axis(120)
    exciter = noise(len(t)) * env_ad(t, 0.2, 1.6)
    body = bandpass(exciter, 860.0, 9.0) + 0.55 * bandpass(exciter, 1_960.0, 12.0)
    body += 0.35 * np.sin(2 * np.pi * 880.0 * t) * env_ad(t, 0.4, 26.0)
    return finalise(body * env_ad(t, 0.2, 55.0))


def rimshot() -> np.ndarray:
    """Sharp rim hit: bright noise crack plus a ringing shell tone."""
    t = t_axis(110)
    crack = highpass(noise(len(t)), 1_800.0) * env_ad(t, 0.15, 4.5)
    shell = np.sin(2 * np.pi * 1_620.0 * t) * env_ad(t, 0.3, 14.0)
    ring = bandpass(noise(len(t)), 3_400.0, 7.0) * env_ad(t, 0.2, 9.0)
    return finalise(crack + 0.7 * shell + 0.4 * ring)


def cowbell() -> np.ndarray:
    """808-style cowbell: two detuned pulse waves through a band-pass."""
    t = t_axis(220)
    sq_a = np.sign(np.sin(2 * np.pi * 587.0 * t))
    sq_b = np.sign(np.sin(2 * np.pi * 845.0 * t))
    mixed = bandpass(sq_a + sq_b, 2_640.0, 2.2)
    return finalise(mixed * env_ad(t, 0.5, 48.0, curve=1.3))


def stick_count() -> np.ndarray:
    """Drumsticks counting in: very short, very bright noise transient."""
    t = t_axis(60)
    burst = highpass(noise(len(t)), 2_600.0) * env_ad(t, 0.1, 2.2)
    burst += 0.3 * bandpass(noise(len(t)), 5_200.0, 3.0) * env_ad(t, 0.1, 4.0)
    return finalise(burst)


def clave() -> np.ndarray:
    """Clave: single high resonant tone with a fast, dry decay."""
    t = t_axis(130)
    tone = np.sin(2 * np.pi * 2_500.0 * t) + 0.25 * np.sin(2 * np.pi * 5_000.0 * t)
    attack = noise(len(t)) * env_ad(t, 0.1, 1.0) * 0.4
    return finalise((tone + attack) * env_ad(t, 0.3, 20.0, curve=1.2))


def soft_tick() -> np.ndarray:
    """Quiet, unobtrusive tick for headphone click tracks."""
    t = t_axis(45)
    tone = np.sin(2 * np.pi * 4_000.0 * t)
    return finalise(lowpass(tone * env_ad(t, 0.3, 6.0), 7_000.0), peak_db=-3.0)


def noise_burst() -> np.ndarray:
    """Band-limited noise pop - cuts through a dense mix without being tonal."""
    t = t_axis(70)
    burst = bandpass(noise(len(t)), 1_400.0, 1.1) * env_ad(t, 0.2, 5.5)
    return finalise(burst)


def metronome_bell() -> np.ndarray:
    """Mechanical metronome bell: inharmonic partials, longer ring."""
    t = t_axis(600)
    partials = [(1.0, 1.0), (2.76, 0.55), (5.40, 0.33), (8.93, 0.18), (13.34, 0.09)]
    out = np.zeros_like(t)
    for ratio, amp in partials:
        decay = 240.0 / (1.0 + 0.55 * ratio)
        out += amp * np.sin(2 * np.pi * 660.0 * ratio * t) * env_ad(t, 0.4, decay)
    strike = highpass(noise(len(t)), 4_000.0) * env_ad(t, 0.1, 2.0) * 0.35
    return finalise(out + strike)


def digital_blip() -> np.ndarray:
    """Synthetic blip with a downward pitch sweep - easy to spot in a busy cue."""
    t = t_axis(80)
    sweep = 2_200.0 * np.exp(-t / 0.012) + 700.0
    phase = 2 * np.pi * np.cumsum(sweep) / SAMPLE_RATE
    pulse = np.sign(np.sin(phase)) * 0.6 + np.sin(phase) * 0.4
    return finalise(lowpass(pulse * env_ad(t, 0.2, 11.0), 9_000.0))


CLICKS = [
    ("01_studio_beep", studio_beep),
    ("02_wood_block", wood_block),
    ("03_rimshot", rimshot),
    ("04_cowbell", cowbell),
    ("05_stick_count", stick_count),
    ("06_clave", clave),
    ("07_soft_tick", soft_tick),
    ("08_noise_burst", noise_burst),
    ("09_metronome_bell", metronome_bell),
    ("10_digital_blip", digital_blip),
]


def write_wav_24(path: Path, samples: np.ndarray) -> None:
    """Write a mono 24-bit PCM WAV file."""
    clipped = np.clip(samples, -1.0, 1.0)
    ints = np.round(clipped * 8_388_607.0).astype(np.int32)

    payload = bytearray()
    for value in ints:
        payload += struct.pack("<i", int(value))[:3]

    with wave.open(str(path), "wb") as handle:
        handle.setnchannels(1)
        handle.setsampwidth(3)
        handle.setframerate(SAMPLE_RATE)
        handle.writeframes(bytes(payload))


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for name, factory in CLICKS:
        data = factory()
        target = OUT_DIR / f"{name}.wav"
        write_wav_24(target, data)
        peak = float(np.max(np.abs(data)))
        print(
            f"{target.name:<26} {len(data):>6} frames  "
            f"{len(data) / SAMPLE_RATE * 1000:>6.1f} ms  peak {20 * math.log10(peak):+.1f} dBFS"
        )


if __name__ == "__main__":
    main()
