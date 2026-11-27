#!/usr/bin/env python3
"""Regenerate art and the short UI/combat sounds. Never rewrites the score.

Music has a separate, opt-in renderer: tools/render_score.py.
"""
from pathlib import Path
import numpy as np
import soundfile as sf

ROOT = Path(__file__).resolve().parents[1] / 'assets' / 'audio'
SR = 22050


def _save_ogg(samples, name):
    samples = np.clip(samples, -0.89, 0.89).astype(np.float32)
    sf.write(ROOT / (name + '.ogg'), samples, SR, format='OGG', subtype='VORBIS')

def _env(n, attack=0.008, release=0.06):
    e = np.ones(n, dtype=np.float32)
    a = min(int(attack * SR), n)
    r = min(int(release * SR), n)
    if a > 0: e[:a] = np.linspace(0.0, 1.0, a)
    if r > 0: e[-r:] = np.linspace(1.0, 0.0, r)
    return e

def square(freq, n, vol=0.5, detune=0.0):
    t = np.arange(n) / SR
    s = np.sign(np.sin(2 * np.pi * freq * (1 + detune) * t) + 0.08 * np.sin(4 * np.pi * freq * t))
    return (s * vol * _env(n)).astype(np.float32)

def tri(freq, n, vol=0.6):
    t = np.arange(n) / SR
    s = 2 * np.abs(2 * (t * freq % 1.0)) - 1
    return (s * vol * _env(n)).astype(np.float32)

def noise(n, vol=0.4, decay=0.2):
    s = np.random.uniform(-1, 1, n).astype(np.float32)
    e = np.exp(-np.arange(n) / (decay * SR))
    return s * e * vol

def sfx_blip(name, freq, dur, vol=0.5, kind="square"):
    n = int(dur * SR)
    if kind == "square": s = square(freq, n, vol=vol)
    elif kind == "noise": s = noise(n, vol=vol, decay=dur / 2)
    else: s = tri(freq, n, vol=vol)
    _save_ogg(s, name)

def sfx_sweep(name, f0, f1, dur, vol=0.5, kind="square"):
    n = int(dur * SR)
    t = np.arange(n) / SR
    f = np.linspace(f0, f1, n)
    phase = 2 * np.pi * np.cumsum(f) / SR
    s = np.sign(np.sin(phase))
    s = s * vol * np.exp(-t * (2.0 / dur))
    _save_ogg(s.astype(np.float32), name)

def sfx_arp(name, freqs, dur, vol=0.5):
    n_each = int(dur / len(freqs) * SR)
    segs = [square(f, n_each, vol=vol) for f in freqs]
    _save_ogg(np.concatenate(segs).astype(np.float32), name)

def build_sfx():
    ROOT.mkdir(parents=True, exist_ok=True)
    sfx_blip("cursor", 880, 0.05, 0.35)
    sfx_arp("confirm", [660, 990], 0.16, 0.4)
    sfx_blip("cancel", 196, 0.12, 0.4, "tri")
    sfx_sweep("sword", 1400, 180, 0.22, 0.5)
    sfx_blip("hit", 110, 0.10, 0.6, "tri")
    sfx_sweep("spell", 380, 1900, 0.30, 0.4)
    sfx_arp("heal", [523, 659, 784], 0.5, 0.35)
    sfx_arp("coin", [988, 1319], 0.18, 0.4)
    sfx_arp("potion", [660, 550, 440], 0.3, 0.35)
    sfx_blip("door", 82, 0.18, 0.6, "tri")
    sfx_sweep("encounter", 1200, 100, 0.5, 0.55)
    sfx_arp("levelup", [523, 659, 784, 1047], 0.45, 0.4)
    sfx_sweep("flee", 1800, 300, 0.35, 0.35)
    print("Wrote 13 sound effects; music unchanged.")


if __name__ == '__main__':
    import build_art
    build_art.main()
    build_sfx()
