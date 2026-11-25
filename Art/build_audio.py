# Synthesises every sound in the slice with numpy (no samples, no licences). Writes 16-bit mono WAVs.
# Run: blender -b --factory-startup --python Art/build_audio.py
import os, wave, math
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "Audio")
os.makedirs(OUT, exist_ok=True)
SR = 44100
rng = np.random.default_rng(543)


def t_(sec):
    return np.arange(int(sec * SR)) / SR


def write(name, x, peak=0.85):
    x = np.asarray(x, np.float64)
    m = np.max(np.abs(x)) or 1.0
    x = x / m * peak
    with wave.open(os.path.join(OUT, name + ".wav"), "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes((x * 32767).astype(np.int16).tobytes())
    print("WAV", name, round(len(x) / SR, 2))


def lp(x, cutoff):
    a = math.exp(-2 * math.pi * cutoff / SR)
    y = np.empty_like(x); s = 0.0
    for i in range(len(x)):
        s = (1 - a) * x[i] + a * s
        y[i] = s
    return y


def lp_fast(x, cutoff, passes=2):
    # FFT brick-ish low pass (fast for long buffers)
    X = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1 / SR)
    X *= 1.0 / (1.0 + (f / cutoff) ** (2 * passes))
    return np.fft.irfft(X, len(x))


def bp(x, lo, hi):
    X = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1 / SR)
    X *= 1.0 / (1.0 + (f / hi) ** 4) * (1.0 / (1.0 + (lo / np.maximum(f, 1)) ** 4))
    return np.fft.irfft(X, len(x))


def noise(sec):
    return rng.standard_normal(int(sec * SR))


def env(n, a, d, sustain=None):
    t = np.arange(n) / SR
    e = np.minimum(1.0, t / max(a, 1e-4)) * np.exp(-np.maximum(0, t - a) / max(d, 1e-4))
    return e


def loopify(x, fade=1.0):
    """Crossfade the tail into the head so the buffer loops without a click."""
    n = int(fade * SR)
    head, tail = x[:n].copy(), x[-n:].copy()
    w = np.linspace(0, 1, n)
    out = x[:-n].copy()
    out[:n] = head * w + tail * (1 - w)
    return out


def vowel(f0, sec, formants, vib=0.0, jitter=0.0):
    t = t_(sec)
    f = f0 * (1 + vib * np.sin(2 * np.pi * 5.2 * t)) * (1 + jitter * lp_fast(noise(sec), 3))
    ph = np.cumsum(f) / SR * 2 * np.pi
    src = np.zeros_like(t)
    for k in range(1, 30):
        src += np.sin(k * ph) / k
    out = np.zeros_like(t)
    for fc, bw, g in formants:
        out += bp(src, fc - bw, fc + bw) * g
    return out


A = [(700, 110, 1.0), (1220, 120, 0.5), (2600, 160, 0.25)]
O = [(450, 80, 1.0), (800, 90, 0.5), (2830, 150, 0.15)]
E = [(400, 70, 1.0), (2000, 120, 0.4), (2550, 150, 0.25)]
I = [(300, 60, 1.0), (2300, 120, 0.4), (3000, 160, 0.2)]
U = [(330, 60, 1.0), (700, 80, 0.4), (2400, 140, 0.1)]

# ------------------------------------------------------------------ ambiences
sec = 24
wind = lp_fast(noise(sec), 400) * (0.6 + 0.4 * np.sin(2 * np.pi * t_(sec) / 7.3) ** 2)
wind += bp(noise(sec), 600, 1400) * 0.04 * (0.5 + 0.5 * np.sin(2 * np.pi * t_(sec) / 3.1))
crick = np.zeros(int(sec * SR))
for k in range(120):
    s = rng.integers(0, len(crick) - SR)
    tt = t_(0.05)
    chirp = np.sin(2 * np.pi * 4300 * tt) * np.sin(np.pi * tt / 0.05) * 0.04
    crick[s:s + len(chirp)] += chirp * rng.uniform(0.2, 1)
write("amb_night", loopify(wind * 0.8 + crick, 2.0), 0.5)

sec = 20
day = lp_fast(noise(sec), 700) * 0.5 + bp(noise(sec), 2000, 5000) * 0.02
write("amb_day", loopify(day, 2.0), 0.4)

sec = 24
drone = np.sin(2 * np.pi * 41 * t_(sec)) * 0.4 + np.sin(2 * np.pi * 61.5 * t_(sec)) * 0.2 + lp_fast(noise(sec), 120) * 0.6
drips = np.zeros(int(sec * SR))
for k in range(14):
    s = rng.integers(0, len(drips) - SR)
    tt = t_(0.25)
    f = 1800 * np.exp(-tt * 9) + 700
    d = np.sin(np.cumsum(2 * np.pi * f / SR)) * np.exp(-tt * 18) * 0.5
    drips[s:s + len(d)] += d
write("amb_ossuary", loopify(drone + drips, 2.0), 0.55)

sec = 16
room = lp_fast(noise(sec), 250) * 0.4 + np.sin(2 * np.pi * 55 * t_(sec)) * 0.05
write("amb_church", loopify(room, 2.0), 0.3)

sec = 16
lap = np.zeros(int(sec * SR))
for k in range(40):
    s = rng.integers(0, len(lap) - SR)
    x = bp(noise(0.6), 200, 900) * env(int(0.6 * SR), 0.08, 0.2)
    lap[s:s + len(x)] += x * rng.uniform(0.3, 1)
write("amb_water", loopify(lap + lp_fast(noise(sec), 300) * 0.3, 2.0), 0.45)

# ------------------------------------------------------------------ voices (no words; vowel colour only)
# reversed Trisagion: three chant phrases, rendered forwards then each note's envelope flipped so it swells and cuts
notes = [(196, O, 1.2), (220, A, 0.8), (196, I, 1.0), (174.6, O, 1.6), (196, A, 0.9), (220, E, 1.1), (246.9, O, 1.4), (220, A, 1.6)]
phr = []
for f0, fm, d in notes:
    v = vowel(f0, d, fm, vib=0.006, jitter=0.01)
    e = env(len(v), 0.05, d * 0.6)[::-1]           # reversed envelope
    for voice in (1.0, 0.5, 2.0):
        pass
    v2 = vowel(f0 * 0.5, d, fm, vib=0.005) * 0.5
    phr.append((v + v2) * e)
chant = np.concatenate(phr)
chant = chant + np.concatenate([np.zeros(int(0.09 * SR)), chant[:-int(0.09 * SR)]]) * 0.35   # cheap reverb tail
write("chant_reverse", loopify(chant, 0.5), 0.6)

# the boy's hum of the same melody
hum = np.concatenate([vowel(f0 * 2, d, U, vib=0.01) * env(int(d * SR), 0.15, d) for f0, fm, d in notes])
write("hum_child", hum, 0.4)

# whispering voices in the reeds / bright ones: shaped noise in vowel formants, overlapping
sec = 8
wh = np.zeros(int(sec * SR))
for k in range(22):
    d = rng.uniform(0.4, 1.0)
    fm = [A, O, E, I, U][k % 5]
    x = noise(d)
    y = sum(bp(x, fc - bw * 2, fc + bw * 2) * g for fc, bw, g in fm)
    y *= env(len(y), d * 0.3, d * 0.4)
    s = rng.integers(0, len(wh) - len(y))
    wh[s:s + len(y)] += y * rng.uniform(0.3, 1)
write("whisper", loopify(wh, 1.0), 0.6)

# the false voice from the trapdoor: two-syllable call "A-le-xi-os", hoarse and far away
call = np.concatenate([vowel(150, 0.25, A, jitter=0.03), vowel(160, 0.18, E, jitter=0.03), bp(noise(0.08), 3000, 6000) * 0.4,
                       vowel(170, 0.22, I, jitter=0.03), vowel(140, 0.45, O, jitter=0.04) * np.linspace(1, 0.2, int(0.45 * SR))])
call = lp_fast(call, 1600)
call = call + np.concatenate([np.zeros(int(0.12 * SR)), call[:-int(0.12 * SR)]]) * 0.4
write("voice_call", np.concatenate([call, np.zeros(int(0.5 * SR)), call * 0.8]), 0.6)

# bright one speaking: low layered vowels, soft
bv = vowel(98, 2.2, O, vib=0.004) * 0.6 + vowel(147, 2.2, A, vib=0.004) * 0.4
bv *= env(len(bv), 0.3, 1.2)
write("bright_voice", bv, 0.5)

# choir chord when the church is cleansed: D minor -> D major open fifths, warm
sec = 7
ch = np.zeros(int(sec * SR))
for f0, fm in ((146.8, O), (220, A), (293.7, O), (370, E)):
    v = vowel(f0, sec, fm, vib=0.005, jitter=0.004)
    ch += v
ch *= env(len(ch), 1.5, 3.5)
write("choir", ch, 0.6)

# title music: slow drone chord loop
sec = 30
mt = np.zeros(int(sec * SR))
for f0, fm, g in ((73.4, O, 1.0), (110, A, 0.6), (146.8, O, 0.5), (220, U, 0.3)):
    mt += vowel(f0, sec, fm, vib=0.003, jitter=0.002) * g
mt *= 0.7 + 0.3 * np.sin(2 * np.pi * t_(sec) / 15) ** 2
write("mus_title", loopify(mt, 3.0), 0.5)

# ------------------------------------------------------------------ one-shots
def knock_body(f, d, bright=0.3):
    tt = t_(d)
    x = sum(np.sin(2 * np.pi * f * k * tt) * np.exp(-tt * (25 + 10 * k)) / k for k in (1, 2.3, 3.9, 5.4))
    x += bp(noise(d), 800, 4000) * np.exp(-tt * 80) * bright
    return x


write("semantron", knock_body(420, 0.6, 0.6), 0.9)
write("knock", np.concatenate([knock_body(150, 0.25, 0.4), np.zeros(int(0.18 * SR)), knock_body(150, 0.25, 0.4), np.zeros(int(0.18 * SR)), knock_body(150, 0.3, 0.4)]), 0.7)
write("door_bang", knock_body(90, 0.8, 1.0) + lp_fast(noise(0.8), 300) * np.exp(-t_(0.8) * 10), 0.9)
write("step_dirt", bp(noise(0.14), 150, 1200) * env(int(0.14 * SR), 0.005, 0.04), 0.5)
write("step_stone", bp(noise(0.12), 400, 3500) * env(int(0.12 * SR), 0.002, 0.025) + knock_body(240, 0.12, 0) * 0.3, 0.5)
drop = lambda: bp(noise(0.05), 3000, 9000) * env(int(0.05 * SR), 0.001, 0.012)
write("sprinkle", np.sum([np.pad(drop(), (s_, int(0.7 * SR) - s_ - int(0.05 * SR))) for s_ in rng.integers(0, int(0.6 * SR), 30)], axis=0), 0.6)
tt = t_(4.0)
write("cross_hum", loopify(np.sin(2 * np.pi * 220 * tt) * 0.4 + np.sin(2 * np.pi * 330 * tt) * 0.25 + np.sin(2 * np.pi * 440.7 * tt) * 0.15 * (1 + np.sin(2 * np.pi * 0.5 * tt)), 0.5), 0.45)
hb = np.zeros(int(1.0 * SR))
for s, g in ((0, 1.0), (int(0.28 * SR), 0.7)):
    x = np.sin(2 * np.pi * 50 * t_(0.15)) * np.exp(-t_(0.15) * 30) * g
    hb[s:s + len(x)] += x
write("heartbeat", hb, 0.8)
write("knot", knock_body(900, 0.08, 0.2), 0.4)
write("pickup", np.sin(2 * np.pi * 660 * t_(0.5)) * env(int(0.5 * SR), 0.01, 0.15) * 0.5 + np.sin(2 * np.pi * 990 * t_(0.5)) * env(int(0.5 * SR), 0.05, 0.2) * 0.3, 0.5)
write("page", bp(noise(0.35), 1500, 7000) * env(int(0.35 * SR), 0.08, 0.12), 0.5)
sec = 6
fire = lp_fast(noise(sec), 900) * 0.5
for k in range(60):
    s = rng.integers(0, int((sec - 0.1) * SR))
    fire[s:s + int(0.01 * SR)] += bp(noise(0.01), 2000, 8000) * rng.uniform(0.2, 1)
write("fire_loop", loopify(fire, 0.5), 0.5)
write("gust", lp_fast(noise(2.0), 700) * np.sin(np.pi * t_(2.0) / 2.0) ** 2, 0.7)
write("censer", np.sum([np.pad(knock_body(2600 + rng.uniform(-200, 200), 0.1, 0.1), (s, int(1.2 * SR) - s - int(0.1 * SR))) for s in (0, int(0.12 * SR), int(0.6 * SR), int(0.7 * SR))], axis=0), 0.5)
sting = np.zeros(int(3 * SR))
for f in (55, 58.3, 82.4, 116.5):
    sting += np.sin(2 * np.pi * f * t_(3)) * env(int(3 * SR), 0.02, 1.4)
sting += bp(noise(3), 300, 3000) * env(int(3 * SR), 0.01, 0.3) * 0.5
write("sting", sting, 0.8)
write("scrape_wood", bp(noise(1.2), 300, 2500) * (0.5 + 0.5 * np.sin(2 * np.pi * 9 * t_(1.2))) * env(int(1.2 * SR), 0.2, 0.6), 0.6)
write("scrape_stone", bp(noise(0.9), 150, 1800) * env(int(0.9 * SR), 0.05, 0.4), 0.7)
write("whoosh", bp(noise(0.5), 400, 3000) * np.sin(np.pi * t_(0.5) / 0.5) ** 3, 0.7)
write("chain", np.sum([np.pad(knock_body(3200 + rng.uniform(-400, 400), 0.07, 0.4), (s, int(1.0 * SR) - s - int(0.07 * SR))) for s in rng.integers(0, int(0.9 * SR), 9)], axis=0), 0.6)
write("rope_creak", np.sin(2 * np.pi * (180 + 60 * np.sin(2 * np.pi * 1.3 * t_(1.6))) * t_(1.6)) * bp(noise(1.6), 100, 900) * 0.3 + bp(noise(1.6), 200, 600) * 0.2, 0.5)
write("splash", bp(noise(0.8), 200, 3000) * env(int(0.8 * SR), 0.005, 0.18), 0.7)
write("flash", bp(noise(1.5), 50, 400) * env(int(1.5 * SR), 0.01, 0.5) + np.sin(2 * np.pi * 36 * t_(1.5)) * env(int(1.5 * SR), 0.01, 0.6), 0.9)
print("BUILD_AUDIO DONE")
