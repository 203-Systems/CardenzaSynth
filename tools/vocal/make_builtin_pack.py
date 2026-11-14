#!/usr/bin/env python3
"""Builds the Vocal engine's built-in voice pack, "Synthetic Vowels".

    python3 tools/vocal/make_builtin_pack.py            writes VocalBuiltinPack.h
    python3 tools/vocal/make_builtin_pack.py out.vocal  also writes the pack as a file

The pack is original: every wave is computed here from formant frequencies, nothing is sampled. Its
layout is the voice pack format the firmware reads (Library/Synth/Core/Engines/Vocal/VocalPack.h);
check() below repeats the firmware's own checks, so a pack this script writes is one it loads.
Standard library only.
"""
import math
import struct
import sys
import zlib
from pathlib import Path

HEADER, WAVE, PHONEME, ZONE, ENTRY, SEGMENT = 128, 104, 48, 8, 24, 8
FORMAT = 2
ROOTS = (60, 67, 74, 81)

# Formants (Hz) of each phoneme. K and S are no tones: they sound at a pitch of their own and their
# pitch is thrown about by the modulation unit at its fastest, which is how this chip makes noise.
FORMANTS = {
    'A': (800, 1150, 2900), 'I': (350, 2300, 3000), 'U': (325, 700, 2500), 'E': (450, 1800, 2700),
    'O': (450, 800, 2800), 'M': (220, 600, 1200), 'K': (900, 2500, 5000), 'S': (2500, 4000, 6500),
}
NOISY = {'K': 67, 'S': 79}  # phoneme: the note it sounds at
SYLLABLES = [(16, 'MI'), (17, 'KU'), (18, 'MA'), (19, 'KA'), (20, 'SA')]


def text(value, size):
    assert 0 < len(value) < size and all(32 <= ord(c) <= 126 for c in value)
    return value.encode().ljust(size, b'\0')


def wave_samples(peaks, note):
    """One period, 64 six-bit steps, stored as (step - 32) * 4."""
    f0 = 440 * 2 ** ((note - 69) / 12)
    amplitudes = [sum(math.exp(-.5 * ((h * f0 - p) / (p * .12 + 90)) ** 2) for p in peaks) / math.sqrt(h) + .02 / h
                  for h in range(1, 25)]
    signal = [sum(a * math.sin(2 * math.pi * h * i / 64) for h, a in enumerate(amplitudes, 1)) for i in range(64)]
    peak = max(abs(x) for x in signal)
    return [(max(0, min(63, round(x * 27 / peak) + 32)) - 32) * 4 for x in signal]


def noise_table():
    """32 modulation steps (0..7) from a fixed sequence: never 4, which would bring the pitch back."""
    state, table = 0x2F6E2B1, []
    while len(table) < 32:
        state = (state * 1103515245 + 12345) & 0x7FFFFFFF
        code = (state >> 16) & 7
        if code != 4:
            table.append(code)
    return table


def build():
    waves, phonemes, zones = bytearray(), bytearray(), bytearray()
    names = list(FORMANTS)
    for name in names:
        noisy = name in NOISY
        first_zone = len(zones) // ZONE
        for root in ROOTS:
            samples = wave_samples(FORMANTS[name], root)
            depth, rate, table = (63, 4095, noise_table()) if noisy else (0, 0, [0] * 32)
            zones += struct.pack('<HBBBBH', len(waves) // WAVE, root, 0, 127, 0, 0)
            waves += struct.pack('<64b', *samples) + struct.pack('<BBHHH', 32, depth, rate, 0, 0) + bytes(table)
        # first zone, zones, pitch mode (2: a note of its own), its note, gain, 0, 0, 0, 1, attack ms, 0, 32 x 0
        phonemes += struct.pack('<HHBBBBHBBHH', first_zone, len(ROOTS), 2 if noisy else 0, NOISY.get(name, 0), 255, 0, 0, 0, 1, 4, 0)
        phonemes += bytes(32)
    entries, segments = bytearray(), bytearray()

    def entry(ident, name, parts):
        nonlocal entries, segments
        entries += struct.pack('<HHHH', ident, len(segments) // SEGMENT, len(parts), 0) + text(name, 16)
        for phoneme, duration, gain in parts:
            segments += struct.pack('<HHBBH', names.index(phoneme), duration, gain, 0, 3)

    for ident, name in enumerate(names):
        entry(ident, name, [(name, 0, 255)])
    for ident, name in SYLLABLES:
        # A short consonant, then the vowel for as long as the note is held.
        entry(ident, name, [(name[0], 65 if name[0] == 'S' else 40, 180), (name[1], 0, 255)])

    body = waves + phonemes + zones + entries + segments
    head = bytearray(HEADER)
    struct.pack_into('<8sHHII6H', head, 0, b'NPVOCAL\0', FORMAT, HEADER, HEADER + len(body), 0, len(waves) // WAVE,
                     len(phonemes) // PHONEME, len(zones) // ZONE, len(entries) // ENTRY, len(segments) // SEGMENT, 0)
    head[32:64] = text('Synthetic Vowels', 32)
    head[64:96] = text('Cardenza', 32)
    head[96:128] = text('cardenza.synthetic', 32)
    pack = head + body
    struct.pack_into('<I', pack, 16, zlib.crc32(pack[20:]))
    return bytes(pack)


def check(b):
    """Pack::Validate, in the same order."""
    u16 = lambda at: struct.unpack_from('<H', b, at)[0]
    u32 = lambda at: struct.unpack_from('<I', b, at)[0]
    named = lambda at, size: (0 < b[at:at + size].find(b'\0') < size and all(32 <= c <= 126 for c in b[at:at + size].split(b'\0')[0])
                              and not any(b[at + b[at:at + size].find(b'\0'):at + size]))
    assert HEADER <= len(b) <= 128 * 1024 and b[:8] == b'NPVOCAL\0' and u16(8) == FORMAT and u16(10) == HEADER
    assert u32(12) == len(b) and u32(16) == zlib.crc32(b[20:])
    assert all(named(at, 32) for at in (32, 64, 96))
    waves, phones, zones, entries, segments = (u16(at) for at in (20, 22, 24, 26, 28))
    assert 0 < waves <= 256 and 0 < phones <= 256 and 0 < zones <= 1024 and 0 < entries <= 256 and 0 < segments <= 2048
    po = HEADER + waves * WAVE
    zo = po + phones * PHONEME
    eo = zo + zones * ZONE
    so = eo + entries * ENTRY
    assert so + segments * SEGMENT == len(b)
    for i in range(waves):
        w = HEADER + i * WAVE
        assert not any(v & 3 for v in b[w:w + 64]) and b[w + 64] <= 32 and b[w + 65] <= 63 and u16(w + 66) <= 4095
        assert u16(w + 68) <= 10000 and u16(w + 70) == 0 and max(b[w + 72:w + 104]) <= 7
    for i in range(zones):
        z = zo + i * ZONE
        assert u16(z) < waves and b[z + 2] <= 127 and b[z + 3] <= b[z + 4] <= 127 and b[z + 5] == 0 and u16(z + 6) == 0
    for i in range(phones):
        p = po + i * PHONEME
        assert u16(p + 2) > 0 and u16(p) + u16(p + 2) <= zones and b[p + 4] <= 2 and (b[p + 4] != 2 or b[p + 5] <= 127)
        assert b[p + 11] == 1 and u16(p + 12) <= 1000
    previous, default = -1, False
    for i in range(entries):
        e = eo + i * ENTRY
        ident, start, count = u16(e), u16(e + 2), u16(e + 4)
        assert ident > previous and u16(e + 6) == 0 and named(e + 8, 16) and 0 < count <= 16 and start + count <= segments
        previous, default = ident, default or ident == u16(30)
        for j in range(count):
            s = so + (start + j) * SEGMENT
            assert u16(s) < phones and u16(s + 2) <= 10000 and b[s + 5] == 0 and u16(s + 6) <= 1000
            assert u16(s + 2) != 0 or j + 1 == count
    assert default


def main():
    pack = build()
    check(pack)
    root = Path(__file__).resolve().parents[2]
    header = root / 'Library/Synth/Core/Engines/Vocal/VocalBuiltinPack.h'
    lines = ['// The built-in voice, "Synthetic Vowels": A I U E O M K S and the syllables MI KU MA KA SA, every wave',
             '// computed from formant frequencies (nothing sampled). Generated by tools/vocal/make_builtin_pack.py;',
             '// do not edit by hand.',
             '#pragma once', '', '#include <array>', '#include <cstdint>', '', 'namespace synth::vocal {', '',
             f'inline constexpr std::array<std::uint8_t, {len(pack)}> kBuiltinPack{{{{']
    lines += ['    ' + ', '.join(str(v) for v in pack[i:i + 20]) + ',' for i in range(0, len(pack), 20)]
    lines += ['}};', '', '}  // namespace synth::vocal', '']
    header.write_text('\n'.join(lines))
    print(f'{header.relative_to(root)}: {len(pack)} bytes')
    if len(sys.argv) > 1:
        Path(sys.argv[1]).write_bytes(pack)
        print(f'{sys.argv[1]}: {len(pack)} bytes')


if __name__ == '__main__':
    main()
