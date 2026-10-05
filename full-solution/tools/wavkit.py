"""Small dependency-free WAV helpers shared by the project tools.

Everything here works on plain Python lists of floats in [-1.0, 1.0] and uses
only the standard library, so students can run the tools with a bare Python 3
install (no numpy, no scipy).
"""

from __future__ import annotations

import array
import wave

#: Sample rate the firmware runs at, in Hz. Both the ADC front end and the
#: R-2R playback path are clocked at this rate.
FIRMWARE_RATE_HZ = 8000


def read_wav(path: str) -> tuple[list[float], int]:
    """Read a PCM WAV file and return (mono samples in [-1, 1], sample rate)."""
    with wave.open(path, "rb") as w:
        channels = w.getnchannels()
        width = w.getsampwidth()
        rate = w.getframerate()
        raw = w.readframes(w.getnframes())

    if width == 1:
        # 8-bit WAV is unsigned by definition.
        data = [(b - 128) / 128.0 for b in raw]
    elif width == 2:
        a = array.array("h")
        a.frombytes(raw)
        data = [s / 32768.0 for s in a]
    elif width == 3:
        data = []
        for i in range(0, len(raw), 3):
            v = int.from_bytes(raw[i:i + 3], "little", signed=True)
            data.append(v / 8388608.0)
    elif width == 4:
        a = array.array("i")
        a.frombytes(raw)
        data = [s / 2147483648.0 for s in a]
    else:
        raise ValueError(f"unsupported sample width: {width} bytes")

    if channels > 1:
        data = [
            sum(data[i:i + channels]) / channels
            for i in range(0, len(data) - channels + 1, channels)
        ]
    return data, rate


def write_wav(path: str, samples: list[float], rate: int) -> None:
    """Write mono 16-bit PCM. Samples outside [-1, 1] are clipped, not wrapped."""
    a = array.array("h", (max(-32768, min(32767, int(round(s * 32767.0)))) for s in samples))
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(a.tobytes())


def resample(samples: list[float], src_rate: int, dst_rate: int) -> list[float]:
    """Linear-interpolation resampler.

    Linear interpolation is not a great anti-alias filter, so callers that
    downsample from a high rate should low-pass first (see `lowpass`). For the
    short, already band-limited speech clips this project deals with, the
    residual aliasing sits well below the 250 Hz - 3500 Hz analysis band.
    """
    if src_rate == dst_rate or not samples:
        return list(samples)
    ratio = src_rate / dst_rate
    out_len = int(len(samples) / ratio)
    out = []
    for i in range(out_len):
        pos = i * ratio
        j = int(pos)
        frac = pos - j
        s0 = samples[j]
        s1 = samples[j + 1] if j + 1 < len(samples) else s0
        out.append(s0 + (s1 - s0) * frac)
    return out


def lowpass(samples: list[float], rate: int, cutoff_hz: float) -> list[float]:
    """One-pole IIR low-pass, applied forward then backward (zero phase).

    Gentle (-6 dB/octave per pass, -12 dB/octave total) but enough to keep the
    worst aliasing out of a 48 kHz -> 8 kHz decimation.
    """
    if not samples or cutoff_hz >= rate / 2:
        return list(samples)
    import math
    alpha = 1.0 - math.exp(-2.0 * math.pi * cutoff_hz / rate)

    def pass_once(xs):
        y = 0.0
        out = []
        for x in xs:
            y += alpha * (x - y)
            out.append(y)
        return out

    return pass_once(pass_once(samples)[::-1])[::-1]


def normalize(samples: list[float], peak: float = 0.95) -> list[float]:
    """Scale so the largest absolute sample equals `peak`. Silence passes through."""
    m = max((abs(s) for s in samples), default=0.0)
    if m < 1e-9:
        return list(samples)
    g = peak / m
    return [s * g for s in samples]


def remove_dc(samples: list[float]) -> list[float]:
    """Subtract the mean, so the 6-bit quantiser centres on code 31/32."""
    if not samples:
        return []
    mean = sum(samples) / len(samples)
    return [s - mean for s in samples]


def quantize_6bit(samples: list[float]) -> list[int]:
    """Map [-1.0, 1.0] onto the 6-bit unsigned DAC codes 0..63.

    The R-2R ladder on PD2..PD7 is unipolar: code 0 is 0 V and code 63 is
    ~Vcc*63/64. Silence must therefore land mid-scale, which is the boundary
    between codes 31 and 32. Using a 31.5 scale factor makes -1.0 -> 0,
    0.0 -> 31.5 (rounds to 32) and +1.0 -> 63, i.e. a symmetric mapping with no
    wasted codes at either rail.
    """
    out = []
    for s in samples:
        code = int(round((s + 1.0) * 31.5))
        out.append(0 if code < 0 else 63 if code > 63 else code)
    return out


def silence(seconds: float, rate: int = FIRMWARE_RATE_HZ) -> list[float]:
    """`seconds` worth of digital silence at `rate`."""
    return [0.0] * int(round(seconds * rate))
