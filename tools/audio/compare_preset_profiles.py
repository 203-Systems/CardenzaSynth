"""Compares two preset profiles (PRINT_PRESET_PROFILE=1 SynthRegression > file.csv) of two builds,
an AMY update say: how much each preset's loudness, spectrum and envelope moved.

    python tools/audio/compare_preset_profiles.py before.csv after.csv [--all]

Loudness is the loudest 0.3 s of the note at the device's level (PresetLevels.inc applied). Timbre
is the RMS difference of the third-octave spectrum (each against its own total, so level does not
count) over the bands above -40 dB in either build. Envelope is the RMS difference of the 50 ms
frames above -60 dBFS once the level difference is taken out: a note that decays or swells
differently. A preset is listed when one of them passes its limit (or with --all).
"""
import argparse
import csv
import math
import statistics

BAND_COUNT = 0
low = 40.0
while low < 16000.0:
    BAND_COUNT += 1
    low *= 2.0 ** (1.0 / 3.0)
BAND_CENTERS = [40.0 * 2.0 ** ((i + 0.5) / 3.0) for i in range(BAND_COUNT)]

LIMITS = {"loudness": 1.5, "timbre": 3.0, "envelope": 3.0}


def load(path):
    profiles = {}
    with open(path, encoding="utf-8") as handle:
        for row in csv.reader(handle):
            if len(row) < 3 + BAND_COUNT:
                continue
            values = [float(v) for v in row[1:]]
            profiles[row[0]] = {
                "loudness": values[0],
                "peak": values[1],
                "bands": values[2:2 + BAND_COUNT],
                "envelope": values[2 + BAND_COUNT:],
            }
    return profiles


def rms(values):
    return math.sqrt(sum(v * v for v in values) / len(values)) if values else 0.0


def compare(old, new):
    loudness = new["loudness"] - old["loudness"]
    band_diffs = [(n - o, c) for o, n, c in zip(old["bands"], new["bands"], BAND_CENTERS) if max(o, n) > -40.0]
    timbre = rms([d for d, _ in band_diffs])
    worst = max(band_diffs, key=lambda item: abs(item[0])) if band_diffs else (0.0, 0.0)
    frames = [(o, n) for o, n in zip(old["envelope"], new["envelope"]) if max(o, n) > -60.0]
    shift = statistics.median([n - o for o, n in frames]) if frames else 0.0
    envelope = rms([n - o - shift for o, n in frames])
    return {"loudness": loudness, "timbre": timbre, "envelope": envelope, "worst_band": worst,
            "old_loudness": old["loudness"], "new_loudness": new["loudness"], "new_peak": new["peak"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("before")
    parser.add_argument("after")
    parser.add_argument("--all", action="store_true", help="list every preset")
    args = parser.parse_args()
    before, after = load(args.before), load(args.after)
    missing = sorted(set(before) ^ set(after))
    results = {name: compare(before[name], after[name]) for name in before if name in after}
    print(f"{len(results)} presets compared" + (f", missing in one: {', '.join(missing)}" if missing else ""))
    for key, limit in LIMITS.items():
        values = [abs(r[key]) for r in results.values()]
        over = sum(1 for v in values if v > limit)
        print(f"  {key:8s}: median {statistics.median(values):.2f} dB, max {max(values):.2f} dB, over {limit} dB: {over}")
    print()
    print(f"{'preset':18s} {'loud':>6s} {'timbre':>6s} {'env':>6s}  {'before':>6s} {'after':>6s} {'peak':>6s}  worst band")
    for name in sorted(results):
        r = results[name]
        flagged = any(abs(r[key]) > limit for key, limit in LIMITS.items())
        if not (flagged or args.all):
            continue
        diff, center = r["worst_band"]
        print(f"{name:18s} {r['loudness']:+6.1f} {r['timbre']:6.1f} {r['envelope']:6.1f}  "
              f"{r['old_loudness']:6.1f} {r['new_loudness']:6.1f} {r['new_peak']:6.1f}  {diff:+.1f} dB at {center:.0f} Hz")


if __name__ == "__main__":
    main()
