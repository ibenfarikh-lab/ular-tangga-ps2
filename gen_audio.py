# Hasilkan aset audio game: WAV terpisah per kejadian -> ADPCM (.adp via
# adpenc PS2SDK) -> audio_data.h (termasuk audsrv.irx tertanam).
# Musik memakai komposisi contoh yang disetujui user (arah A, 2026-10-30).
import math, os, random, subprocess, wave
from array import array

SR = 22050
DIR = "/home/hatch/workspace/ps2-matrix/ular-judul-game"
ADIR = DIR + "/audio"
PS2SDK = os.environ.get("PS2SDK", "/home/hatch/workspace/ps2dev/ps2sdk-install")
os.makedirs(ADIR, exist_ok=True)

def newbuf(detik):
    return array('f', [0.0]) * int(SR * detik)

def add_tone(buf, t0, freq, dur, gain, decay=2.2, harmonics=(1.0, 0.45, 0.18)):
    i0 = int(t0 * SR); n = int(dur * SR)
    for i in range(n):
        j = i0 + i
        if j >= len(buf): break
        t = i / SR
        env = (1.0 - math.exp(-t * 90.0)) * math.exp(-t * decay)
        v = 0.0
        for hi, hg in enumerate(harmonics):
            v += hg * math.sin(2 * math.pi * freq * (hi + 1) * t)
        buf[j] += gain * env * v

def add_gong(buf, t0, base=110.0, gain=0.25, dur=4.0):
    i0 = int(t0 * SR); n = int(dur * SR)
    for i in range(n):
        j = i0 + i
        if j >= len(buf): break
        t = i / SR
        env = (1.0 - math.exp(-t * 40.0)) * math.exp(-t * 1.1)
        v = (math.sin(2*math.pi*base*t) + 0.5*math.sin(2*math.pi*base*2.02*t)
             + 0.25*math.sin(2*math.pi*base*2.94*t) + 0.12*math.sin(2*math.pi*base*4.16*t))
        buf[j] += gain * env * v

def add_click(buf, t0, freq, dur=0.07, gain=0.30):
    i0 = int(t0 * SR); n = int(dur * SR)
    rnd = random.Random(int(t0 * 1000) + 7)
    for i in range(n):
        j = i0 + i
        if j >= len(buf): break
        t = i / SR
        env = math.exp(-t * 55.0)
        v = math.sin(2 * math.pi * freq * t) + 0.6 * (rnd.random() * 2 - 1) * math.exp(-t * 160.0)
        buf[j] += gain * env * v

def add_noise(buf, t0, dur, gain, tone_lp=0.25, seed=3, swell=0.0):
    i0 = int(t0 * SR); n = int(dur * SR)
    rnd = random.Random(seed)
    lp = 0.0
    for i in range(n):
        j = i0 + i
        if j >= len(buf): break
        t = i / SR
        w = rnd.random() * 2 - 1
        lp += tone_lp * (w - lp)
        env = 1.0
        if swell: env = math.sin(math.pi * min(t / dur, 1.0)) ** swell
        buf[j] += gain * env * lp * 3.0

def save_wav(buf, name, peak_target=None):
    peak = max(max(buf), -min(buf), 1e-6)
    k = (peak_target / peak) if peak_target else (0.88 / peak if peak > 0.88 else 1.0)
    out = array('h', [int(max(-1.0, min(1.0, v * k)) * 32767) for v in buf])
    path = f"{ADIR}/{name}.wav"
    with wave.open(path, 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(out.tobytes())
    return path

# ---- musik latar (loop 32 detik, sama dengan contoh yang di-ACC) ----
mus = newbuf(32.0)
A3, C4, D4, E4, G4, A4, C5 = 220.0, 261.63, 293.66, 329.63, 392.0, 440.0, 523.25
E3, A2 = 164.81, 110.0
bars = [
    [A3, E4, G4, E4], [A4, G4, E4, D4], [C4, D4, E4, G4], [A4, None, E4, None],
    [A3, C4, D4, E4], [G4, E4, D4, C4], [D4, E4, G4, A4], [A4, None, C5, None],
]
for b, bar in enumerate(bars):
    tbar = b * 4.0
    if b in (0, 4): add_gong(mus, tbar, A2, 0.20, 4.0)
    add_tone(mus, tbar + 2.0, E3 if b % 2 else A3, 2.0, 0.07, decay=1.4)
    for beat, note in enumerate(bar):
        if note: add_tone(mus, tbar + beat, note, 2.4, 0.16, decay=1.9)
for k in range(40):
    t0 = k * 0.8 + 0.13
    for p in range(3):
        add_tone(mus, t0 + p * 0.07, 5200.0, 0.05, 0.012, decay=30.0, harmonics=(1.0,))
for k in range(20):
    t0 = k * 1.6 + 0.71
    for p in range(2):
        add_tone(mus, t0 + p * 0.09, 4700.0, 0.05, 0.009, decay=30.0, harmonics=(1.0,))
rnd = random.Random(11); lp = 0.0
for j in range(len(mus)):
    t = j / SR
    w = rnd.random() * 2 - 1
    lp += 0.02 * (w - lp)
    mus[j] += 0.028 * lp * (0.6 + 0.4 * math.sin(2 * math.pi * t / 8.0))
save_wav(mus, "musik", peak_target=0.72)

# ---- efek terpisah ----
b = newbuf(0.3); add_click(b, 0.02, 700, 0.07, 0.30); save_wav(b, "tok")
b = newbuf(0.3); add_click(b, 0.02, 880, 0.08, 0.32); save_wav(b, "pilih")
b = newbuf(0.3); add_click(b, 0.02, 300, 0.08, 0.30); save_wav(b, "batal")
b = newbuf(3.1)  # kocokan saja (3 detik), tanpa tuk penutup
rnd = random.Random(5); tt = 0.0
while tt < 2.9:
    tt += 0.10 + rnd.random() * 0.14
    g = 0.10 + 0.13 * math.sin(math.pi * tt / 3.0)
    add_noise(b, tt, 0.07, g, tone_lp=0.5, seed=int(tt * 977))
save_wav(b, "kocok")
b = newbuf(0.35); add_click(b, 0.02, 170, 0.11, 0.5); save_wav(b, "tuk")
b = newbuf(0.25); add_click(b, 0.02, 640, 0.06, 0.28); save_wav(b, "langkah")
b = newbuf(1.6)
add_noise(b, 0.0, 0.5, 0.10, tone_lp=0.08, seed=9, swell=1.0)
for i, f in enumerate((329.63, 392.0, 523.25)):
    add_tone(b, 0.45 + i * 0.16, f, 1.0, 0.18, decay=2.4)
save_wav(b, "tangga")
b = newbuf(1.6)
add_noise(b, 0.0, 0.55, 0.16, tone_lp=0.55, seed=21, swell=1.0)
i0 = int(0.4 * SR)
for i in range(int(0.8 * SR)):
    j = i0 + i
    if j >= len(b): break
    t2 = i / SR
    fr = 760.0 * (200.0 / 760.0) ** (t2 / 0.8)
    b[j] += 0.16 * math.exp(-t2 * 2.0) * math.sin(2 * math.pi * fr * t2)
save_wav(b, "ular")
b = newbuf(1.3)
for i, f in enumerate((880.0, 1108.7, 1318.5)):
    add_tone(b, i * 0.13, f, 1.1, 0.15, decay=2.6, harmonics=(1.0, 0.3, 0.08))
save_wav(b, "bonus")
b = newbuf(0.7); add_tone(b, 0.02, 150, 0.5, 0.30, decay=6.0, harmonics=(1.0, 0.2)); save_wav(b, "gagal")
b = newbuf(0.7); add_click(b, 0.02, 520, 0.07, 0.3); add_click(b, 0.30, 520, 0.07, 0.3); save_wav(b, "seri")
b = newbuf(2.6)
for i, f in enumerate((220.0, 261.63, 329.63, 392.0, 440.0, 523.25)):
    add_tone(b, i * 0.12, f, 1.6, 0.17, decay=1.8)
add_gong(b, 0.72, 110.0, 0.28, 2.4)
save_wav(b, "fanfare")

# ---- konversi ke .adp ----
NAMES = ["musik", "tok", "pilih", "batal", "kocok", "tuk", "langkah",
         "tangga", "ular", "bonus", "gagal", "seri", "fanfare"]
adpenc = PS2SDK + "/bin/adpenc"
total = 0
for n in NAMES:
    args = [adpenc] + (["-L"] if n == "musik" else []) + [f"{ADIR}/{n}.wav", f"{ADIR}/{n}.adp"]
    subprocess.run(args, check=True, capture_output=True)
    sz = os.path.getsize(f"{ADIR}/{n}.adp")
    total += sz
    print(n, "adp:", sz)
print("total adp:", total)

# ---- audio_data.h ----
def emit_bytes(f, varname, path):
    data = open(path, "rb").read()
    f.write(f"static const unsigned char {varname}[{len(data)}] = {{\n")
    for i in range(0, len(data), 16):
        f.write(",".join(f"0x{v:02x}" for v in data[i:i + 16]) + ",\n")
    f.write("};\n")
    return len(data)

with open(DIR + "/audio_data.h", "w") as f:
    f.write("#ifndef AUDIO_DATA_H\n#define AUDIO_DATA_H\n")
    n = emit_bytes(f, "audsrv_irx", PS2SDK + "/iop/irx/audsrv.irx")
    print("audsrv.irx:", n)
    for name in NAMES:
        emit_bytes(f, "adp_" + name, f"{ADIR}/{name}.adp")
    f.write("#endif\n")
print("audio_data.h selesai")
