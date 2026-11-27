#!/usr/bin/env python3
"""Render OpenRTP's authored MIDI score *offline*. The game only needs the Oggs.

Usage: python tools/render_score.py /path/to/GeneralUser.sf2 [cue ...]
See assets/audio/SCORE.txt for the source versions and credits.
"""
import argparse
from pathlib import Path

import mido
import numpy as np
import soundfile as sf
from tinysoundfont import Synth

ASSETS = Path(__file__).resolve().parents[1] / 'assets'
SCORES = {
    'title': 'Opening 2', 'village': 'Town 1', 'field': 'Field 1',
    'wood': 'Ending 2', 'fen': 'Dungeon 1', 'pass': 'Castle 1', 'dunes': 'Vehicle 1',
    'hearth': 'Sorrow', 'chapel': 'Church', 'battle': 'Battle 1', 'boss': 'Boss 3',
    'victory': 'JFanfare 1', 'gameover': 'Game Over 1',
}
RATE = 32000


def render(cue, font):
    midi = mido.MidiFile(ASSETS / 'source/openrtp/Music' / (SCORES[cue] + '.mid'))
    synth = Synth(gain=-9, samplerate=RATE)
    sfid = synth.sfload(str(font))
    for ch in range(16):
        synth.program_select(ch, sfid, 0, 0, is_drums=(ch == 9))
    end = round(midi.length * RATE)
    audio = np.zeros((end + RATE, 2), dtype=np.float32)
    pos, seconds = 0, 0.
    for msg in midi:  # merged tempo map; accumulate time before rounding to a sample
        seconds += msg.time
        stop = round(seconds * RATE)
        if stop > pos:
            synth.generate_simple(stop - pos, memoryview(audio[pos:stop]).cast('B'))
            pos = stop
        if msg.type == 'program_change':
            synth.program_change(msg.channel, msg.program, is_drums=(msg.channel == 9))
        elif msg.type == 'note_on' and msg.velocity:
            synth.noteon(msg.channel, msg.note, msg.velocity)
        elif msg.type in ('note_off', 'note_on'):
            synth.noteoff(msg.channel, msg.note)
        elif msg.type == 'control_change':
            synth.control_change(msg.channel, msg.control, msg.value)
        elif msg.type == 'pitchwheel':
            synth.pitchbend(msg.channel, msg.pitch + 8192)
    synth.notes_off()
    synth.generate_simple(len(audio) - pos, memoryview(audio[pos:]).cast('B'))
    # A little stereo echo, with release tails across the barline instead of
    # appending dead air to a rhythmically exact loop.
    for delay, level in ((0.083, 0.12), (0.159, 0.065)):
        n = round(delay * RATE)
        audio[n:] += audio[:-n, ::-1] * level
    audio[-RATE:] *= np.linspace(1, 0, RATE)[:, None]
    if cue not in ('victory', 'gameover'):
        audio[:RATE] += audio[end:]
        audio = audio[:end]
    # Match perceived levels across cues, preserve the headroom of orchestral hits.
    rms = np.sqrt(np.mean(audio ** 2))
    audio *= min(0.115 / max(rms, 1e-6), 0.88 / max(np.max(np.abs(audio)), 1e-6))
    path = ASSETS / 'audio' / (cue + '.ogg')
    with sf.SoundFile(path, 'w', RATE, 2, format='OGG', subtype='VORBIS') as stream:
        for i in range(0, len(audio), 8192):
            stream.write(audio[i:i + 8192])
    print(f'{cue:9} {len(audio)/RATE:6.1f}s  {path.stat().st_size//1024:5} KiB')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('soundfont', type=Path)
    parser.add_argument('cues', nargs='*', help='default: the whole score')
    args = parser.parse_args()
    for cue in args.cues or SCORES:
        if cue not in SCORES:
            parser.error(f'unknown cue: {cue}')
        render(cue, args.soundfont)
