"""Carries Library/Synth/Audio/Amy/PresetLevels.inc across a change of the sound engine (an AMY
update): each preset keeps the loudness it had, its level moved by exactly as much as its raw
loudness moved. Regenerating the table from scratch instead (PRINT_PRESET_LEVELS=1) measures every
preset again and moves presets that did not change by a dB or two.

    python tools/audio/carry_preset_levels.py before_raw.csv after_raw.csv PresetLevels.inc

The CSVs are raw profiles of the two builds, every preset from a fresh process (noise presets
depend on what played before them):

    for i in $(seq 0 255); do PRINT_PRESET_PROFILE=1 PRESET_RAW=1 PRESET_FROM=$i PRESET_COUNT=1 SynthRegression; done

The table is rewritten in place; presets silent in either build keep their level.
"""
import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import compare_preset_profiles as profiles  # noqa: E402

SILENT = -70.0


def preset_id(patch):
    if patch < 128:
        return "juno.%s%d%d" % ("a" if patch < 64 else "b", (patch % 64) // 8 + 1, patch % 8 + 1)
    index = patch - 128
    return "dx7.rom%d%s.%02d" % (index // 64 + 1, "a" if index % 64 < 32 else "b", index % 32 + 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("before")
    parser.add_argument("after")
    parser.add_argument("table")
    args = parser.parse_args()
    before, after = profiles.load(args.before), profiles.load(args.after)
    text = open(args.table, encoding="utf-8").read()
    header, body = text.split("\n", 1)
    levels = [int(v) for v in re.findall(r"-?\d+", body)]
    if len(levels) != 256:
        sys.exit(f"{args.table}: {len(levels)} levels, expected 256")
    moved = []
    for patch in range(256):
        name = preset_id(patch)
        if name not in before or name not in after:
            sys.exit(f"{name} missing from a profile")
        old, new = before[name]["loudness"], after[name]["loudness"]
        if old < SILENT or new < SILENT:
            continue
        level = max(-30, min(30, round(levels[patch] - (new - old))))
        if level != levels[patch]:
            moved.append((name, levels[patch], level, new - old))
        levels[patch] = level
    lines = [header]
    for row in range(16):
        lines.append(", ".join(str(v) for v in levels[row * 16:(row + 1) * 16]) + ",")
    open(args.table, "w", encoding="utf-8", newline="\n").write("\n".join(lines) + "\n")
    print(f"{len(moved)} levels moved")
    for name, old_level, new_level, change in moved:
        print(f"  {name:16s} {old_level:+3d} -> {new_level:+3d} dB (raw {change:+.1f} dB)")


if __name__ == "__main__":
    main()
