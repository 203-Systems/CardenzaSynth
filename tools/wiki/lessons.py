"""The guide: the short, picture-first lessons the wiki opens on (docs/learn/guide.json, drawn by
wiki/src/guide); the manual is the reference next to it.

A part with "hidden": true stays off the site for now (its lessons are still loaded and checked).
A lesson is a few steps. Concept lessons (an "anim") are animations the page plays; device lessons
run on a Cardenza on USB that follows the page through the guide link
(Applications/Synth/GrooveAppGuide.cpp): "start" puts the device where the lesson begins (GL fields:
s scene, p page, t track 1-4, r1 a fresh project, d<n> the song of the song lessons as lessons 1..n made it, f1 the keyboard of a
new project instead of the user's, for lessons that change it or name the notes of keys), and per step

  keys   the keys that blink on the device until the step is learned (the others dimmed), framed on the picture (keys_for tokens, tools/wiki/keys.py)
  done   what completes it, all of: ["press", KEYS] a key pressed (["press", KEYS, n]: n different
         ones of them), ["state", f, n] / ["seen", f, n]
         field f of the device state is n, ["change", f] it changed since the step began,
         ["min", f, n] it is n or more (fields: Controller::GuideState)
  demo   what a person would press: tap / down / up KEY, wait FRAMES (16 ms), shot (the step's
         picture is taken there, else at the end)
  marks  highlights on the step's picture (screen targets, wiki/src/components/cardenza/keys.ts)
  lights the key picture glows with the device's key lights at that moment (as in the manual's pictures)
  keymarks  numbered frames on the key picture, [[KEYS, {zh, en}], ...], told under it; they take the
         place of the plain frame around "keys"

tools/wiki/generate.py runs every device lesson's demos on the firmware (CardenzaSimShot) and fails when
a step does not complete: the lessons cannot drift from the device. The pictures come from that run.
"""
import json
import pathlib
import re
import shutil
import subprocess
import sys

from keys import keys_for

ROOT = pathlib.Path(__file__).resolve().parents[2]
GUIDE = ROOT / "docs" / "learn" / "guide.json"


def load():
    guide = json.loads(GUIDE.read_text(encoding="utf-8"))
    ids = [lesson for part in guide["parts"] for lesson in part["lessons"]]
    missing = sorted(set(ids) ^ set(guide["lessons"]))
    if missing:
        sys.exit(f"guide: lessons listed and defined differ: {missing}")
    for lesson_id, lesson in guide["lessons"].items():
        for lang in ("zh", "en"):
            for field in ("title", "lead"):
                if not lesson.get(field, {}).get(lang):
                    sys.exit(f"guide: {lesson_id} has no {field} in {lang}")
            for number, step in enumerate(lesson["steps"], start=1):
                if not step.get("say", {}).get(lang):
                    sys.exit(f"guide: {lesson_id} step {number} says nothing in {lang}")
        if "anim" not in lesson:
            for number, step in enumerate(lesson["steps"], start=1):
                if not step.get("done") or not step.get("demo"):
                    sys.exit(f"guide: {lesson_id} step {number} needs done and demo")
                key_indices(step.get("keys", ""))
                parse_demo(step["demo"])
                for spec, label in step.get("keymarks", []):
                    key_indices(spec)
                    if not label.get("zh") or not label.get("en"):
                        sys.exit(f"guide: {lesson_id} step {number} has a key mark without words")
    return guide


def lesson_ids(guide):
    return [lesson for part in guide["parts"] for lesson in part["lessons"]]


def device_lessons(guide):
    return [lesson for lesson in lesson_ids(guide) if "anim" not in guide["lessons"][lesson]]


def key_indices(spec):
    """KEYS tokens -> KeyIndex values (row * 14 + column), in order, without repeats."""
    out = []
    for token in spec.split():
        for row, column in keys_for(token):
            index = row * 14 + column
            if index not in out:
                out.append(index)
    return out


def parse_demo(spec):
    """'down ESC; wait 30; tap G4; up ESC' -> [('down', 0), ('wait', 30), ('tap', 12), ('up', 0)]."""
    actions = []
    for part in spec.split(";"):
        words = part.split()
        if not words:
            continue
        verb = words[0].lower()
        if verb == "shot" and len(words) == 1:
            actions.append(("shot", 0))
        elif verb == "wait" and len(words) == 2 and words[1].isdigit():
            actions.append(("wait", int(words[1])))
        elif verb in ("tap", "down", "up") and len(words) == 2:
            keys = keys_for(words[1])
            if len(keys) != 1:
                sys.exit(f"guide: demo key {words[1]!r} is not one key")
            row, column = keys[0]
            actions.append((verb, row * 14 + column))
        else:
            sys.exit(f"guide: bad demo action {part.strip()!r}")
    return actions


def shot_name(lesson_id, number):
    return f"guide-{lesson_id}-{number}"


# ---------------------------------------------------------------------------------------------
# The check: every device lesson's demos on the firmware, then the conditions against what it said.

def script(guide):
    lines = ["wait 140", "app GE", "wait 4"]
    for lesson_id in device_lessons(guide):
        lesson = guide["lessons"][lesson_id]
        lines += [f"app G#{lesson_id}", f"app GL{lesson['start']}", "wait 30"]
        for number, step in enumerate(lesson["steps"], start=1):
            lines.append(f"app G#{lesson_id}.{number}")
            shot = False
            for verb, value in parse_demo(step["demo"]):
                if verb == "wait":
                    lines.append(f"wait {value}")
                elif verb == "shot":
                    lines += ["wait 6", f"screenshot {shot_name(lesson_id, number)}"]
                    shot = True
                else:
                    lines += [f"{verb} {value // 14},{value % 14}", "wait 3"]
            lines.append("wait 24")
            if not shot:
                lines.append(f"screenshot {shot_name(lesson_id, number)}")
            lines.append("wait 6")
    lines.append(f"app G#end")
    return "\n".join(lines) + "\n"


def parse_state(text):
    fields = {}
    body, _, name = text.partition("n")
    for letter, value in re.findall(r"([a-z])(\d+)", body):
        fields[letter] = int(value)
    fields["n"] = name
    return fields


def satisfied(condition, base, states, presses):
    kind = condition[0]
    if kind == "press":
        wanted = set(key_indices(condition[1]))
        needed = condition[2] if len(condition) > 2 else 1
        return len(wanted & set(presses)) >= needed
    field = condition[1]
    every = [base] + states
    if kind in ("state", "seen"):
        return any(state.get(field) == condition[2] for state in every)
    if kind == "min":
        return any(state.get(field, -1) >= condition[2] for state in every)
    if kind == "change":
        return any(state.get(field) != base.get(field) for state in states)
    sys.exit(f"guide: unknown condition {condition!r}")


def check(guide, log_text):
    """The app log of the run -> the steps that did not complete, as messages."""
    failures = []
    state = {}
    marker, base, states, presses = None, {}, [], []

    def close():
        if marker is None or "." not in marker:
            return
        lesson_id, number = marker.split(".")
        step = guide["lessons"][lesson_id]["steps"][int(number) - 1]
        missing = [c for c in step["done"] if not satisfied(c, base, states, presses)]
        if missing:
            last = states[-1] if states else base
            failures.append(f"{lesson_id} step {number}: {missing} not met (state {last})")

    for line in log_text.splitlines():
        _frame, direction, message = line.split(" ", 2)
        if direction == ">" and message.startswith("G#"):
            close()
            marker, base, states, presses = message[2:], dict(state), [], []
        elif direction == "<" and message.startswith("GS"):
            state = parse_state(message[2:])
            states.append(state)
        elif direction == "<" and re.fullmatch(r"GK[0-9a-f]{2}1", message):
            presses.append(int(message[2:4], 16))
        elif direction == "<" and message.startswith("GE0"):
            failures.append(f"the guide did not start: {message}")
    if marker != "end":
        failures.append(f"the run stopped at {marker}")
    return failures


def run(guide, sim_build, work, img):
    """Runs the lessons on the firmware; their pictures go to img/. Exits when a step fails."""
    shot_tool = sim_build / "Devices" / "CardenzaSim" / "CardenzaSimShot"
    raw = work / "guide-shots"
    shutil.rmtree(raw, ignore_errors=True)
    shutil.rmtree(work / "sdcard", ignore_errors=True)
    (work / "guide.script").write_text(script(guide))
    subprocess.run([str(shot_tool), "--script", str(work / "guide.script"), "--out", str(raw), "--scale", "2"], check=True,
                   cwd=work, stdout=subprocess.DEVNULL)
    failures = check(guide, (raw / "app.log").read_text())
    if failures:
        sys.exit("guide: lessons that do not work on the firmware:\n  " + "\n  ".join(failures))
    img.mkdir(parents=True, exist_ok=True)
    try:
        from PIL import Image
    except ImportError:
        Image = None
    leds = {}
    for png in sorted(raw.glob("guide-*.png")):
        if Image is None:
            shutil.copyfile(png, img / png.name)
        else:
            with Image.open(png) as shot:
                shot.convert("RGB").quantize(colors=64, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).save(
                    img / png.name, optimize=True)
        # The key lights of that moment (CardenzaSimShot writes them next to the picture).
        led_file = png.with_suffix(".leds.txt")
        if led_file.exists():
            leds[png.stem] = [list(map(int, line.split())) for line in led_file.read_text().splitlines()[:4]]
    (work / "guide-leds.json").write_text(json.dumps(leds))
    return leds
