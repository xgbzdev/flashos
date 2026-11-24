#!/usr/bin/env python3
"""Summarise a WAV recording of FlashOS sound (CI): loud parts and pitch."""
import sys, wave
import numpy as np
w = wave.open(sys.argv[1])
sr, ch = w.getframerate(), w.getnchannels()
a = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").reshape(-1, ch)[:, 0].astype(float)
print(f"recording: {len(a)/sr:.1f} s at {sr} Hz")
env = np.sqrt(np.convolve(a ** 2, np.ones(sr // 100) / (sr // 100), mode="same"))
loud = env > 300
runs, start = [], 0
for i in range(1, len(loud)):
    if loud[i] != loud[i - 1]:
        runs.append((loud[i - 1], start, i)); start = i
runs.append((loud[-1], start, len(loud)))
for on, s0, s1 in runs:
    if not on or s1 - s0 < sr // 20: continue
    seg = a[s0:s1]
    n = min(len(seg), 8192)
    sp = np.abs(np.fft.rfft(seg[:n] * np.hanning(n)))
    print(f"sound {s0/sr:6.2f}-{s1/sr:6.2f} s  rms {np.sqrt((seg**2).mean()):6.0f}  main pitch {np.argmax(sp)*sr/n:5.0f} Hz")
