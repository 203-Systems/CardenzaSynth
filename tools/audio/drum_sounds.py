#!/usr/bin/env python3
"""The drum machine's built-in sounds and kits, generated from AMY's drum samples.

AMY (Library/third_party/amy) carries the Gamma9001 drum banks as 22050 Hz mono WAVs in
sounds/gamma9001 (TR-808, TR-909, Linn 9000, Univox MR-12, Tokyo Synthetics, percussion, an
acoustic set, the 80s Power Kit and some effects) and bakes an 11-sample TR-808 set into its ROM
(pcm_tiny.h). The firmware plays the ROM set as it is and keeps a choice of the other banks in
flash (DrumSoundData.inc, played in place through pcm_load_external): what fits beside the
firmware in its 4 MB partition. Left out: the Power Kit (930 KB), the effects bank (1.2 MB) and
the non-drum percussion (strings, sitar, didgeridoo); their WAVs work from the SD card's Samples.

Every sound gets a gain that brings it to the loudness of its kind (kicks, toms and hand drums,
snares and claps, hats and cymbals, the rest), measured as AMY does for its kits: the loudest
100 ms. The ROM set keeps gain 1, so the 808 kit sounds as it always did.

THE ORDER OF SOUNDS IS PART OF THE PROJECT FORMAT: a pad stores its sound's index (padN.sound).
Only append to SOUNDS; never reorder, remove or reuse an entry.

Usage: drum_sounds.py <amy dir> <output .inc>   (run by cmake/CardenzaSynthSourceLists.cmake)
"""
import json
import math
import os
import re
import sys
import wave
from array import array

RATE = 22050

# (source, name, label): source is ('rom', AMY tiny preset) or a WAV below sounds/gamma9001.
# Names are what the screen shows (up to 14 characters), labels the pad's two letters.
SOUNDS = [
    # TR-808: the ROM set first (the default kit), then what the full bank adds.
    (('rom', 1), '808 KICK', 'BD'),
    (('rom', 2), '808 SNARE', 'SD'),
    (('rom', 6), '808 HAT', 'CH'),
    (('rom', 7), '808 OPEN HAT', 'OH'),
    (('rom', 9), '808 CLAP', 'CP'),
    (('rom', 8), '808 TOM', 'TM'),
    (('rom', 10), '808 COWBELL', 'CB'),
    (('rom', 0), '808 MARACA', 'MA'),
    (('rom', 3), '808 SNARE 2', 'SD'),
    (('rom', 4), '808 SNARE 3', 'SD'),
    (('rom', 5), '808 SNARE 4', 'SD'),
    ('tr808/tr-808-bass-drum-1.wav', '808 KICK LONG', 'BD'),
    ('tr808/tr-808-bass-drum-2.wav', '808 KICK 2', 'BD'),
    ('tr808/tr-808-bass-drum-3.wav', '808 KICK 3', 'BD'),
    ('tr808/tr-808-rimshot.wav', '808 RIM', 'RM'),
    ('tr808/tr-808-clap.wav', '808 CLAP 2', 'CP'),
    ('tr808/tr-808-hihat-closed.wav', '808 HAT 2', 'CH'),
    ('tr808/tr-808-hihat-open.wav', '808 OPEN HAT 2', 'OH'),
    ('tr808/tr-808-cymbal.wav', '808 CYMBAL', 'CY'),
    ('tr808/tr-808-tom-hi.wav', '808 TOM HI', 'TM'),
    ('tr808/tr-808-conga-lo.wav', '808 CONGA LO', 'CG'),
    ('tr808/tr-808-conga-mid.wav', '808 CONGA MID', 'CG'),
    ('tr808/tr-808-conga-hi.wav', '808 CONGA HI', 'CG'),
    ('tr808/tr-808-cowbell.wav', '808 COWBELL 2', 'CB'),
    ('tr808/tr-808-clave.wav', '808 CLAVE', 'CL'),
    ('tr808/tr-808-shaker.wav', '808 SHAKER', 'SH'),
    # TR-909
    ('tr909/909bd.wav', '909 KICK', 'BD'),
    ('tr909/909bd-lo.wav', '909 KICK LO', 'BD'),
    ('tr909/909sd.wav', '909 SNARE', 'SD'),
    ('tr909/909sd-short.wav', '909 SNARE SHRT', 'SD'),
    ('tr909/909rim.wav', '909 RIM', 'RM'),
    ('tr909/909clap.wav', '909 CLAP', 'CP'),
    ('tr909/909hh.wav', '909 HAT', 'CH'),
    ('tr909/909hh-short.wav', '909 HAT SHORT', 'CH'),
    ('tr909/909hh-long.wav', '909 HAT LONG', 'CH'),
    ('tr909/909oh.wav', '909 OPEN HAT', 'OH'),
    ('tr909/909oh-short.wav', '909 OPEN SHORT', 'OH'),
    ('tr909/909oh-long.wav', '909 OPEN LONG', 'OH'),
    ('tr909/909crash.wav', '909 CRASH', 'CY'),
    ('tr909/909ride.wav', '909 RIDE', 'RD'),
    ('tr909/909tom-lo.wav', '909 TOM LO', 'TM'),
    ('tr909/909tom-mid.wav', '909 TOM MID', 'TM'),
    ('tr909/909tom-hi.wav', '909 TOM HI', 'TM'),
    # Linn 9000
    ('linn9000/bassdrum-1.wav', 'LINN KICK', 'BD'),
    ('linn9000/snaredrum-1.wav', 'LINN SNARE', 'SD'),
    ('linn9000/rimshoot-1.wav', 'LINN RIM', 'RM'),
    ('linn9000/hihat-closed-3.wav', 'LINN HAT', 'CH'),
    ('linn9000/hihat-open-2.wav', 'LINN OPEN HAT', 'OH'),
    ('linn9000/cymbal-2.wav', 'LINN CYMBAL', 'CY'),
    ('linn9000/tomtom-2.wav', 'LINN TOM', 'TM'),
    ('linn9000/bongo-3.wav', 'LINN BONGO', 'BG'),
    ('linn9000/cowbell-1.wav', 'LINN COWBELL', 'CB'),
    ('linn9000/taburine-1.wav', 'LINN TAMB', 'TA'),
    # Univox MR-12
    ('mr12/mr-12-kick.wav', 'MR12 KICK', 'BD'),
    ('mr12/mr-12-snr.wav', 'MR12 SNARE', 'SD'),
    ('mr12/mr-12-hhc.wav', 'MR12 HAT', 'CH'),
    ('mr12/mr-12-hho.wav', 'MR12 OPEN HAT', 'OH'),
    # Tokyo Synthetics
    ('synthetics/tokyo-jet-bd.wav', 'SYN JET KICK', 'BD'),
    ('synthetics/tokyo-deep-bd.wav', 'SYN DEEP KICK', 'BD'),
    ('synthetics/tokyo-burst-bd.wav', 'SYN BURST KICK', 'BD'),
    ('synthetics/bd-03.wav', 'SYN KICK 3', 'BD'),
    ('synthetics/bd-04.wav', 'SYN KICK 4', 'BD'),
    ('synthetics/bd-07.wav', 'SYN KICK 7', 'BD'),
    ('synthetics/static-07.wav', 'SYN STATIC', 'SD'),
    ('synthetics/hizz-06.wav', 'SYN HIZZ', 'SD'),
    ('synthetics/wooden-01.wav', 'SYN WOOD 1', 'WD'),
    ('synthetics/wooden-02.wav', 'SYN WOOD 2', 'WD'),
    ('synthetics/wooden-03.wav', 'SYN WOOD 3', 'WD'),
    ('synthetics/click-05.wav', 'SYN CLICK 5', 'CK'),
    ('synthetics/click-06.wav', 'SYN CLICK 6', 'CK'),
    ('synthetics/click-07.wav', 'SYN CLICK 7', 'CK'),
    ('synthetics/click-08.wav', 'SYN CLICK 8', 'CK'),
    ('synthetics/click-09.wav', 'SYN CLICK 9', 'CK'),
    ('synthetics/pew-03.wav', 'SYN PEW', 'PW'),
    ('synthetics/metallic-09.wav', 'SYN METAL 9', 'MT'),
    ('synthetics/metallic-16.wav', 'SYN METAL 16', 'MT'),
    ('synthetics/boink-02.wav', 'SYN BOINK 2', 'BK'),
    ('synthetics/boink-04.wav', 'SYN BOINK 4', 'BK'),
    ('synthetics/blop-01.wav', 'SYN BLOP 1', 'BL'),
    ('synthetics/blop-02.wav', 'SYN BLOP 2', 'BL'),
    ('synthetics/tokyo-space.wav', 'SYN SPACE', 'FX'),
    # Percussion
    ('percussion/noice-kick.wav', 'PERC KICK', 'BD'),
    ('percussion/old-snare.wav', 'PERC SNARE', 'SD'),
    ('percussion/tamp-002.wav', 'PERC TAMP', 'HD'),
    ('percussion/kanu-001.wav', 'PERC KANU', 'KN'),
    ('percussion/conga-001.wav', 'CONGA 1', 'CG'),
    ('percussion/conga-002.wav', 'CONGA 2', 'CG'),
    ('percussion/conga-003.wav', 'CONGA 3', 'CG'),
    ('percussion/conga-004.wav', 'CONGA 4', 'CG'),
    ('percussion/conga-005.wav', 'CONGA 5', 'CG'),
    ('percussion/conga-006.wav', 'CONGA 6', 'CG'),
    ('percussion/african-conga-001.wav', 'AFRO CONGA 1', 'CG'),
    ('percussion/african-conga-002.wav', 'AFRO CONGA 2', 'CG'),
    ('percussion/african-conga-003.wav', 'AFRO CONGA 3', 'CG'),
    ('percussion/african-conga-004.wav', 'AFRO CONGA 4', 'CG'),
    ('percussion/bongo-001.wav', 'BONGO 1', 'BG'),
    ('percussion/bongo-002.wav', 'BONGO 2', 'BG'),
    ('percussion/bongo-003.wav', 'BONGO 3', 'BG'),
    ('percussion/bongo-004.wav', 'BONGO 4', 'BG'),
    ('percussion/bongo-005.wav', 'BONGO 5', 'BG'),
    ('percussion/bongo-006.wav', 'BONGO 6', 'BG'),
    ('percussion/bongo-007.wav', 'BONGO 7', 'BG'),
    ('percussion/tabla-001.wav', 'TABLA 1', 'TB'),
    ('percussion/tabla-002.wav', 'TABLA 2', 'TB'),
    ('percussion/tabla-003.wav', 'TABLA 3', 'TB'),
    ('percussion/timbales-001.wav', 'TIMBALE 1', 'TI'),
    ('percussion/timbales-002.wav', 'TIMBALE 2', 'TI'),
    ('percussion/timbales-003.wav', 'TIMBALE 3', 'TI'),
    ('percussion/timbales-004.wav', 'TIMBALE 4', 'TI'),
    ('percussion/timpani-001.wav', 'TIMPANI', 'TP'),
    ('percussion/tamb-001.wav', 'TAMB 1', 'TA'),
    ('percussion/tamb-003.wav', 'TAMB 2', 'TA'),
    ('percussion/tamb-004.wav', 'TAMB 3', 'TA'),
    ('percussion/simple-shaker.wav', 'SHAKER 1', 'SH'),
    ('percussion/simple-shaker-002.wav', 'SHAKER 2', 'SH'),
    ('percussion/triangle-001.wav', 'TRIANGLE 1', 'TR'),
    ('percussion/triangle-002.wav', 'TRIANGLE 2', 'TR'),
    ('percussion/wistle-001.wav', 'WHISTLE', 'WH'),
    ('acoustic/tambourine-short.wav', 'TAMBOURINE', 'TA'),
    ('acoustic/shaker-short.wav', 'SHAKER 3', 'SH'),
    ('acoustic/shaker-tiny.wav', 'SHAKER TINY', 'SH'),
]

# The kits, after AMY's GM kits (amy/headers.py, GAMMA_KIT_*), on the eight pads: kick, snare,
# closed hat, open hat, clap, tom, cowbell, maraca (GM 36, 38, 42, 46, 39, 43, 56, 70). Each pad is
# (sound name, semitones from the sound's own pitch). The 808 kit is the ROM set the firmware had.
KITS = [
    ('drums.808', '808 KIT', [('808 KICK', 0), ('808 SNARE', 0), ('808 HAT', 0), ('808 OPEN HAT', 0),
                              ('808 CLAP', 0), ('808 TOM', 0), ('808 COWBELL', 0), ('808 MARACA', 0)]),
    ('drums.909', '909 KIT', [('909 KICK', 0), ('909 SNARE', 0), ('909 HAT', 0), ('909 OPEN HAT', 0),
                              ('909 CLAP', 0), ('909 TOM LO', 0), ('LINN COWBELL', 0), ('SHAKER 2', 0)]),
    ('drums.linn', 'LINN 9000', [('LINN KICK', 0), ('LINN SNARE', 0), ('LINN HAT', 0), ('LINN OPEN HAT', 0),
                                 ('808 CLAP 2', 0), ('LINN TOM', -2), ('LINN COWBELL', 0), ('SHAKER 2', 0)]),
    ('drums.mr12', 'MR-12', [('MR12 KICK', 0), ('MR12 SNARE', 0), ('MR12 HAT', 0), ('MR12 OPEN HAT', 0),
                             ('MR12 SNARE', 3), ('MR12 KICK', 6), ('LINN COWBELL', 0), ('SHAKER 2', 0)]),
    ('drums.synthetics', 'SYNTHETICS', [('SYN JET KICK', 0), ('SYN STATIC', 0), ('SYN CLICK 5', 0), ('SYN PEW', 0),
                                        ('SYN WOOD 3', 0), ('SYN BLOP 2', -2), ('SYN WOOD 1', 0), ('SHAKER 2', 0)]),
    ('drums.percussion', 'PERCUSSION', [('PERC KICK', 3), ('PERC SNARE', 0), ('SHAKER 1', 0), ('SHAKER 2', 0),
                                        ('PERC TAMP', 0), ('TIMPANI', -2), ('LINN COWBELL', 0), ('TAMBOURINE', 0)]),
]

# The loudest 100 ms each kind is brought to, in dBFS (the ROM 808 set sits about there).
TARGETS = {'BD': -8.4, 'TM': -10.0, 'CG': -10.0, 'BG': -10.0, 'TB': -10.0, 'TI': -10.0, 'TP': -10.0, 'KN': -10.0, 'HD': -12.0,
           'SD': -15.5, 'CP': -15.5, 'RM': -15.5, 'CH': -17.0, 'OH': -17.0, 'CY': -17.0, 'RD': -17.0, 'SH': -17.0, 'TA': -17.0,
           'TR': -17.0, 'MA': -17.0}
DEFAULT_TARGET = -14.0


def loudness(samples):
    n = 2205
    best = 0.0
    squares = [(s / 32768.0) ** 2 for s in samples]
    total = sum(squares[:n])
    best = total
    for i in range(n, len(squares)):
        total += squares[i] - squares[i - n]
        best = max(best, total)
    return 20.0 * math.log10(math.sqrt(best / n) + 1e-9)


def read_wav(path):
    with wave.open(path) as w:
        assert w.getnchannels() == 1 and w.getsampwidth() == 2 and w.getframerate() == RATE, path
        data = array('h')
        data.frombytes(w.readframes(w.getnframes()))
    if sys.byteorder == 'big':
        data.byteswap()
    return data


def read_rom(amy):
    """AMY's tiny ROM set: (offset, length, root) per preset, and the samples."""
    table = open(os.path.join(amy, 'src', 'pcm_tiny.h')).read()
    entries = [(int(a), int(b), int(e)) for a, b, c, d, e in re.findall(r'\{(\d+), (\d+), (\d+), (\d+), (\d+)\}, /\*', table)]
    text = open(os.path.join(amy, 'src', 'pcm_samples_tiny.h')).read()
    body = text[text.index('{') + 1:text.rindex('}')]
    samples = array('h', (int(v) for v in re.findall(r'-?\d+', body)))
    return entries, samples


def c_string(text):
    assert '"' not in text and '\\' not in text
    return '"' + text + '"'


def main():
    amy, out = sys.argv[1], sys.argv[2]
    manifest = {m['file']: m for m in json.load(open(os.path.join(amy, 'sounds', 'gamma9001', 'manifest.json')))}
    rom, rom_samples = read_rom(amy)
    names = [name for _, name, _ in SOUNDS]
    assert len(set(names)) == len(names), 'sound names must be unique'
    assert all(len(name) <= 14 for name in names), [n for n in names if len(n) > 14]
    assert all(len(label) == 2 for _, _, label in SOUNDS)

    frames = array('h')
    rows = []
    for source, name, label in SOUNDS:
        if isinstance(source, tuple):
            offset, length, root = rom[source[1]]
            rows.append((name, label, source[1], 0, length, root, 1.0))
            continue
        data = read_wav(os.path.join(amy, 'sounds', 'gamma9001', source))
        root = int(manifest[source]['root'])
        target = TARGETS.get(label, DEFAULT_TARGET)
        gain = min(3.0, max(0.5, 10.0 ** ((target - loudness(data)) / 20.0)))
        # A short click measures quiet over 100 ms: never past its own full scale.
        peak = max(1, max(abs(v) for v in data)) / 32768.0
        gain = min(gain, max(1.0, 1.0 / peak))
        # Each sound starts on a 16-byte boundary: a block of the flash cache holds no two sounds' ends.
        while len(frames) % 8:
            frames.append(0)
        rows.append((name, label, 0xFFFF, len(frames), len(data), root, round(gain, 3)))
        frames.extend(data)

    index = {name: i for i, name in enumerate(names)}
    lines = ['// Generated by tools/audio/drum_sounds.py from AMY\'s drum samples (Library/third_party/amy): do not edit.',
             '// %d sounds, %d of them in flash: %d frames, %d KB.' % (len(rows), sum(r[2] == 0xFFFF for r in rows), len(frames), len(frames) * 2 // 1024),
             '',
             'alignas(16) const std::int16_t kDrumSoundFrames[%d] = {' % max(1, len(frames))]
    for start in range(0, len(frames), 24):
        lines.append('    ' + ', '.join(str(v) for v in frames[start:start + 24]) + ',')
    lines.append('};')
    lines.append('')
    lines.append('const DrumSound kDrumSounds[%d] = {' % len(rows))
    for name, label, rom_preset, offset, length, root, gain in rows:
        lines.append('    {%s, %s, %d, %d, %d, %d, %.3ff},' % (c_string(name), c_string(label), rom_preset, offset, length, root, gain))
    lines.append('};')
    lines.append('')
    lines.append('const DrumKit kDrumKits[%d] = {' % len(KITS))
    for kit_id, kit_name, pads in KITS:
        assert len(pads) == 8
        pad_text = ', '.join('{%d, %d}' % (index[sound], tune) for sound, tune in pads)
        lines.append('    {%s, %s, {{%s}}},' % (c_string(kit_id), c_string(kit_name), pad_text))
    lines.append('};')
    text = '\n'.join(lines) + '\n'
    if os.path.exists(out) and open(out).read() == text:
        return
    os.makedirs(os.path.dirname(out), exist_ok=True)
    temporary = out + '.%d.tmp' % os.getpid()
    with open(temporary, 'w') as f:
        f.write(text)
    os.replace(temporary, out)
    print('drum sounds: %d sounds, %d KB in flash -> %s' % (len(rows), len(frames) * 2 // 1024, out))


if __name__ == '__main__':
    main()
