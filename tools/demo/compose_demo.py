"""Compose an original 64-bar demo and export a score for the native renderer.

No sample downloads: pitched voices and the 808 kit come from CardenzaSynth.
The optional mix stage uses NumPy for external stereo production effects.
"""
import argparse
import csv
import json
import math
from pathlib import Path
import struct
import wave

import numpy as np

BPM = 128
SR = 44100
PPQ = 96
BAR = 384
BEAT_SECONDS = 60 / BPM
BAR_SECONDS = 4 * BEAT_SECONDS
N = SR * 120
ROOTS = [40, 36, 43, 38]
CHORDS = [[64, 67, 71], [64, 67, 72], [62, 67, 71], [62, 66, 69]]
HOOK = [
    [(0, 76, 2), (3, 79, 1), (6, 83, 1), (8, 81, 2), (11, 79, 1), (14, 78, 1.7)],
    [(0, 76, 2), (3, 79, 1), (6, 83, 1), (8, 84, 2), (11, 83, 1), (14, 79, 1.7)],
    [(0, 74, 2), (3, 79, 1), (6, 83, 1), (8, 86, 2), (11, 83, 1), (14, 81, 1.7)],
    [(0, 81, 2), (3, 78, 1), (6, 76, 1), (8, 74, 2), (11, 78, 1), (14, 76, .8), (15, 74, .8)],
]
PATCHES = [
    (4, "drums.808", "NEON 808"),
    (2, "juno.a55", "NEON ELECTRO BASS"),
    (2, "juno.a51", "NEON CHORD STABS"),
    (1, "dx7.rom2b.01", "NEON FM HOOK"),
]
PARAMS = [
    {"master.level": 66, "kick.level": 127, "snare.level": 62, "clap.level": 110,
     "closed_hat.level": 75, "open_hat.level": 83, "tom.level": 90,
     "cowbell.level": 45, "maraca.level": 55},
    {"dco.range": 1, "dco.pwm": 28, "dco.sub": 100, "dco.noise": 0, "dco.saw": 1,
     "vcf.cutoff": 73, "vcf.resonance": 32, "vcf.env": 84, "vcf.lfo": 0,
     "vca.level": 62, "env.attack": 0, "env.decay": 46, "env.sustain": 20,
     "env.release": 18, "lfo.rate": 0, "lfo.delay": 0},
    {"dco.range": 1, "dco.pwm": 80, "dco.sub": 0, "dco.noise": 0, "dco.saw": 1,
     "vcf.cutoff": 92, "vcf.resonance": 16, "vcf.env": 51, "vcf.lfo": 0,
     "vca.level": 44, "env.attack": 0, "env.decay": 59, "env.sustain": 27,
     "env.release": 38, "lfo.rate": 0, "lfo.delay": 0},
    {"global.lfo_pitch": 0, "global.lfo_amp": 0, "global.feedback": 4, "global.transpose": 0},
]


def make_bar(chord=0, mode="drop", variation=False, fill=False):
    tracks = [[], [], [], []]

    def hit(track, step, pitch, velocity, length=.8):
        tracks[track].append((float(step), pitch, velocity, min(length, 16 - step)))

    if mode in ("drop", "groove", "intro", "outro"):
        for step in (0, 4, 8, 12):
            hit(0, step, 36, 125 if mode == "drop" else 116)
        for step in (2, 6, 10, 14):
            hit(0, step, 46 if step in (6, 14) and mode == "drop" else 42, 90 if step % 8 == 6 else 80)
        if mode != "intro":
            for step in (4, 12):
                hit(0, step, 39, 115)
                hit(0, step + 1 / 6, 38, 63)
        if mode == "drop":
            for step in (1, 5, 9, 13):
                hit(0, step, 42, 42 + (step % 3) * 6)
            for step in (3, 7, 11, 15):
                hit(0, step + 1 / 6, 70, 48 + step % 3 * 5)
        if fill:
            for step, pitch, velocity in [(13, 43, 90), (14, 38, 90), (15, 43, 100), (15.5, 38, 100)]:
                hit(0, step, pitch, velocity, .4)

    root = ROOTS[chord]
    if mode in ("drop", "groove", "outro"):
        last = [10, 11, 11, 9][chord]
        for step, offset, vel, length in zip(
                (0, 3, 6, 7, 10, 11, 14, 15), (0, 0, 12, 7, 0, 12, 7, last),
                (121, 111, 103, 114, 120, 99, 113, 86), (2.1, 1.3, .7, .7, .7, .7, .7, .7)):
            hit(1, step, root + offset, vel, length)
    elif mode == "intro":
        for step in (2, 6, 10, 14):
            hit(1, step, root, 86, .8)

    if mode in ("drop", "groove", "intro", "outro"):
        for i, step in enumerate((2, 6, 10, 14)):
            for note in CHORDS[chord]:
                hit(2, step, note, (91, 103, 94, 108)[i] if mode == "drop" else 73, 1.3)
    elif mode in ("break", "build"):
        for note in CHORDS[chord]:
            hit(2, 0, note, 82, 14)
        if mode == "build":
            for step in (4, 8, 12):
                hit(0, step, 38, 72 + step * 2, .6)
                hit(0, step + .5, 39, 60 + step * 2, .4)

    if mode == "drop":
        for i, (step, note, length) in enumerate(HOOK[chord]):
            if variation and i >= 3:
                note = note + 12 if i == 3 else note
            hit(3, step, note, (106, 96, 113, 110, 98, 113, 94)[i], length)
    elif mode in ("break", "build"):
        for step, note, length in [(0, CHORDS[chord][0] + 12, 5), (6, CHORDS[chord][1] + 12, 3), (12, CHORDS[chord][2] + 12, 3.5)]:
            hit(3, step, note, 74 if mode == "break" else 90, length)
    return tracks


def section(bar):
    if bar < 8: return "INTRO"
    if bar < 16: return "GROOVE"
    if bar < 32: return "DROP 1"
    if bar < 40: return "BREAKDOWN"
    if bar < 48: return "BUILD"
    if bar < 60: return "FINAL DROP"
    return "OUTRO"


def compose(folder):
    folder.mkdir(parents=True, exist_ok=True)
    lines = [f"TEMPO {BPM}"]
    for track, ((kind, preset, name), params) in enumerate(zip(PATCHES, PARAMS)):
        lines.append(f'PATCH {track} {kind} "{preset}" "{name}"')
        lines.extend(f"PARAM {track} {name} {value}" for name, value in params.items())
    patterns = [make_bar(i) for i in range(4)] + [make_bar(0, variation=True, fill=True),
                make_bar(0, "groove"), make_bar(0, "break"), make_bar(0, "build")]
    for pattern, tracks in enumerate(patterns):
        for track, notes in enumerate(tracks):
            for step, pitch, velocity, length in notes:
                index = int(step)
                micro = round((step - index) * 6)
                length = min(round(length * 24), 384 - index * 24 - micro * 4)
                lines.append(f"STEP {track} {pattern} {index} {pitch} {velocity} {max(1, length)} {micro}")

    notes = []
    automation = []
    arrangement = []
    for bar in range(64):
        chord = bar % 4
        mode = {"INTRO": "intro", "GROOVE": "groove", "DROP 1": "drop", "BREAKDOWN": "break",
                "BUILD": "build", "FINAL DROP": "drop", "OUTRO": "outro"}[section(bar)]
        if bar >= 60: chord = (0, 1, 3, 0)[bar - 60]
        tracks = make_bar(chord, mode, variation=bar >= 48 or (bar % 8 >= 4), fill=bar % 8 == 7 and mode in ("drop", "groove"))
        if bar < 4:
            tracks[1] = []
        if bar in (6, 7, 14, 15):
            tracks[3] = [(6., HOOK[chord][0][1], 70, 1.5), (14., HOOK[chord][2][1], 77, 1.4)]
        if mode == "build":
            build = bar - 40
            tracks[0] = []
            stride = 4 if build < 2 else 2 if build < 4 else 1 if build < 7 else .5
            for step in np.arange(0, 16 if build < 7 else 12, stride):
                tracks[0].append((float(step), 38, min(125, 62 + build * 7 + round(step)), .35))
            if build >= 4:
                for step in (0, 4, 8, 12):
                    if build == 7 and step >= 12: continue
                    tracks[0].append((float(step), 36, 118, .7))
            if build == 7:
                tracks = [[note for note in tr if note[0] < 12] for tr in tracks]
        if mode == "outro":
            if bar >= 62: tracks[2] = []
            if bar == 63:
                tracks[0] = [(0., 36, 118, 1), (4., 39, 88, 1)]
                tracks[1] = [(0., 40, 115, 8)]
                tracks[2] = [(0., n, 82, 8) for n in CHORDS[0]]
        for track, events in enumerate(tracks):
            for step, pitch, velocity, length in events:
                pulse = bar * BAR + round(step * 24)
                duration = max(2, min(round(length * 24), (bar + 1) * BAR - pulse))
                notes.append([track, pulse, pitch, velocity, duration])
        cutoff = 40 + bar * 3 if bar < 8 else 70 if bar < 16 else 92 if bar < 32 else 48 if bar < 40 else 50 + (bar - 40) * 9 if bar < 48 else 102 if bar < 60 else 76 - (bar - 60) * 8
        automation.append([2, bar * BAR, "vcf.cutoff", min(120, cutoff)])
        automation.append([1, bar * BAR, "vcf.cutoff", 78 if mode == "drop" else 63])
        arrangement.append([bar + 1, round(bar * BAR_SECONDS, 4), section(bar), ["Em", "C", "G", "D"][chord]])

    # A re-trigger must not inherit the previous note's future note-off.
    for track in range(4):
        for pitch in range(128):
            same = sorted([n for n in notes if n[0] == track and n[2] == pitch], key=lambda n: n[1])
            for left, right in zip(same, same[1:]):
                left[4] = min(left[4], max(1, right[1] - left[1] - 1))
    lines.extend("NOTE " + " ".join(map(str, n)) for n in sorted(notes, key=lambda n: (n[1], n[0], n[2])))
    lines.extend("AUTO " + " ".join(map(str, a)) for a in automation)
    (folder / "score.txt").write_text("\n".join(lines) + "\n")
    with (folder / "arrangement.csv").open("w", newline="") as f:
        writer = csv.writer(f); writer.writerow(["bar", "seconds", "section", "chord"]); writer.writerows(arrangement)
    metadata = {"title": "Neon Circuit", "bpm": BPM, "key": "E minor", "bars": 64, "duration_seconds": 120,
                "tracks": [p[2] for p in PATCHES], "notes": len(notes), "sections": arrangement,
                "native_patterns": ["Em drop", "C drop", "G drop", "D drop", "Em fill", "Em groove", "Em breakdown", "Em build"]}
    (folder / "composition.json").write_text(json.dumps(metadata, indent=2))
    write_midi(folder / "Neon_Circuit.mid", notes)
    print(f"Composed {len(notes)} note events; 64 bars, 128 BPM, 120 seconds", flush=True)


def vlq(n):
    result = [n & 127]; n >>= 7
    while n: result.insert(0, (n & 127) | 128); n >>= 7
    return bytes(result)


def write_midi(path, notes):
    chunks = []
    tempo = b"\x00\xff\x51\x03" + int(60_000_000 / BPM).to_bytes(3, "big")
    tempo += b"\x00\xff\x58\x04\x04\x02\x18\x08" + b"\x00\xff\x59\x02\x01\x01" + vlq(BAR * 64) + b"\xff\x2f\x00"
    chunks.append(b"MTrk" + struct.pack(">I", len(tempo)) + tempo)
    for track in range(4):
        channel = 9 if track == 0 else track - 1
        name = PATCHES[track][2].encode()
        data = b"\x00\xff\x03" + vlq(len(name)) + name
        events = []
        for t, pulse, pitch, vel, length in notes:
            if t != track: continue
            events.extend([(pulse, 1, bytes([0x90 | channel, pitch, vel])), (pulse + length, 0, bytes([0x80 | channel, pitch, 0]))])
        previous = 0
        for pulse, _, event in sorted(events):
            data += vlq(pulse - previous) + event; previous = pulse
        data += vlq(BAR * 64 - previous) + b"\xff\x2f\x00"
        chunks.append(b"MTrk" + struct.pack(">I", len(data)) + data)
    path.write_bytes(b"MThd" + struct.pack(">IHHH", 6, 1, len(chunks), PPQ) + b"".join(chunks))


def read_wav(path):
    with wave.open(str(path), "rb") as f:
        assert f.getframerate() == SR and f.getnchannels() == 1 and f.getsampwidth() == 2
        result = np.frombuffer(f.readframes(f.getnframes()), dtype="<i2").astype(np.float32) / 32768
    assert len(result) == N
    return result


def spectral_shape(x, low=20, high=18000):
    freq = np.fft.rfftfreq(len(x), 1 / SR)
    gain = 1 / np.sqrt(1 + (low / np.maximum(freq, .1)) ** 4)
    gain /= np.sqrt(1 + (freq / high) ** 8)
    return np.fft.irfft(np.fft.rfft(x) * gain, n=len(x)).astype(np.float32)


def normalized(x, target):
    peak = float(np.max(np.abs(x)))
    assert peak > .0001, "Silent native stem"
    return x * target / peak


def echo(x, beats=0.75, amount=.15, repeats=4):
    stereo = np.column_stack((x, x)).astype(np.float32)
    for tap in range(1, repeats + 1):
        shift = round(beats * BEAT_SECONDS * tap * SR)
        stereo[shift:, tap % 2] += x[:-shift] * amount * .56 ** (tap - 1)
        stereo[shift:, (tap + 1) % 2] += x[:-shift] * amount * .2 * .56 ** (tap - 1)
    return stereo


def write_wav(path, data):
    with wave.open(str(path), "wb") as f:
        f.setnchannels(data.shape[1] if data.ndim == 2 else 1); f.setsampwidth(2); f.setframerate(SR)
        f.writeframes(np.round(np.clip(data, -1, 1) * 32767).astype("<i2").tobytes())


def mix(folder):
    raw = [read_wav(folder / f"stem-{i}.wav") for i in range(4)]
    stem_stats = [{"track": i + 1, "peak": float(np.max(np.abs(x))), "rms": float(np.sqrt(np.mean(x * x))),
                   "clipped_samples": int(np.count_nonzero(np.abs(x) >= .9997))} for i, x in enumerate(raw)]
    # All musical voices are native. Offline processing supplies stereo space,
    # ducking and transitions which are not part of the current device project.
    drums = normalized(spectral_shape(raw[0], 28, 16500), .74)
    bass = normalized(spectral_shape(raw[1], 30, 6000), .52)
    chords = normalized(spectral_shape(raw[2], 220, 11500), .35)
    lead = normalized(spectral_shape(raw[3], 360, 9200), .38)
    bass = np.tanh(bass * 2.1) / 2.1
    t = np.arange(N, dtype=np.float64) / SR
    beat_phase = np.mod(t, BEAT_SECONDS)
    active = np.ones(N, dtype=np.float32)
    active[(t >= 60) & (t < 75)] = .12
    active[(t >= 75) & (t < 82.5)] = .35
    duck = 1 - active * .80 * np.exp(-beat_phase / .095)
    bass *= duck
    chords *= .48 + .52 * duck
    lead *= .65 + .35 * duck
    chord_stereo = echo(chords, .75, .24, 5)
    # A short decorrelated tap widens the stabs without moving the bass off centre.
    width = round(.011 * SR)
    chord_stereo[width:, 1] += chords[:-width] * .18
    lead_stereo = echo(lead, .75, .29, 5)
    stereo = np.column_stack((drums + bass, drums + bass)) + chord_stereo + lead_stereo

    rng = np.random.default_rng(203)
    # Deterministic filtered-noise risers and impact tails, added only in the mix.
    for start, duration, strength in [(26.25, 3.75, .095), (78.75, 11.25, .13)]:
        count = round(duration * SR)
        noise = rng.standard_normal(count).astype(np.float32)
        freq = np.fft.rfftfreq(count, 1 / SR)
        noise = np.fft.irfft(np.fft.rfft(noise) * (freq / (freq + 1500)) / np.sqrt(1 + (freq / 13000) ** 8), n=count).astype(np.float32)
        ramp = np.linspace(0, 1, count, dtype=np.float32)
        noise *= ramp ** 1.8 * strength * (0.4 + .6 * np.sin(2 * np.pi * np.linspace(0, duration * 4.2667, count)) ** 2)
        left = round(start * SR)
        stereo[left:left + count, 0] += noise
        stereo[left:left + count, 1] += np.roll(noise, 173)
    for impact in (30, 60, 90):
        count = 2 * SR
        noise = rng.standard_normal(count).astype(np.float32)
        decay = np.exp(-np.arange(count) / (SR * .38)).astype(np.float32)
        noise = spectral_shape(noise, 3200, 14500) * decay * .11
        i = round(impact * SR)
        stereo[i:i + count, 0] += noise
        stereo[i:i + count, 1] += np.roll(noise, 109)

    # Smooth entrance, a breath before the last drop, and a resolved ending.
    stereo[:round(.008 * SR)] *= np.linspace(0, 1, round(.008 * SR))[:, None]
    breath_start, breath_end = round(89.8 * SR), 90 * SR
    stereo[breath_start:breath_end] *= np.linspace(1, 0, breath_end - breath_start, dtype=np.float32)[:, None] ** 2
    fade_start = round(118.65 * SR)
    stereo[fade_start:] *= np.linspace(1, 0, N - fade_start, dtype=np.float32)[:, None] ** 1.4
    # Static drive into a soft ceiling; gain is chosen from sustained drop RMS.
    rms = np.sqrt(np.mean(stereo[30 * SR:60 * SR] ** 2))
    gain = min(2.4, .22 / max(rms, .001))
    stereo = .94 * np.tanh(stereo * gain / .94)
    peak = float(np.max(np.abs(stereo)))
    stereo *= .89 / max(peak, .001)
    write_wav(folder / "Neon_Circuit_demo.wav", stereo)
    raw_mix = np.sum(raw, axis=0)
    raw_mix *= .9 / max(float(np.max(np.abs(raw_mix))), .001)
    write_wav(folder / "Neon_Circuit_native.wav", raw_mix)
    stats = {"sample_rate": SR, "channels": 2, "frames": N, "duration_seconds": N / SR,
             "peak_dbfs": float(20 * np.log10(np.max(np.abs(stereo)))),
             "rms_dbfs": float(20 * np.log10(np.sqrt(np.mean(stereo ** 2)))),
             "dc": np.mean(stereo, axis=0).tolist(), "finite": bool(np.isfinite(stereo).all()), "native_stems": stem_stats}
    (folder / "audio-checks.json").write_text(json.dumps(stats, indent=2))
    print(json.dumps(stats, indent=2), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("compose", "mix"))
    parser.add_argument("folder", type=Path)
    args = parser.parse_args()
    (compose if args.mode == "compose" else mix)(args.folder)
