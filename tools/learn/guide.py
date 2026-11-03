"""The guide: the short, picture-first lessons the Learn site opens on (docs/learn/guide.json); the
manual itself is the reference next to it.

A lesson is a few steps. Concept lessons (an "anim") are animations the page plays; device lessons
run on the live device of the page (the web simulator, or a Cardenza on USB) through the guide link
(Applications/Synth/GrooveAppGuide.cpp): "start" puts the device where the lesson begins (GL fields:
s scene, p page, t track 1-4, r1 a fresh project, d1 the song demo), and per step

  keys   the keys that blink on the device and stand out on the picture (figures.keys_for tokens)
  done   what completes it, all of: ["press", KEYS] a key pressed, ["state", f, n] / ["seen", f, n]
         field f of the device state is n, ["change", f] it changed since the step began,
         ["min", f, n] it is n or more (fields: Controller::GuideState)
  demo   what a person would press: tap / down / up KEY, wait FRAMES (16 ms), shot (the step's
         picture is taken there, else at the end)
  marks  highlights on the step's picture (figures.target_rect)

The build runs every device lesson's demos on the firmware (CardenzaSimShot) and fails when a step
does not complete: the lessons cannot drift from the device. The pictures come from that run.
"""
import html
import json
import pathlib
import re
import shutil
import subprocess
import sys

import figures

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
    return guide


def lesson_ids(guide):
    return [lesson for part in guide["parts"] for lesson in part["lessons"]]


def device_lessons(guide):
    return [lesson for lesson in lesson_ids(guide) if "anim" not in guide["lessons"][lesson]]


def key_indices(spec):
    """KEYS tokens -> KeyIndex values (row * 14 + column), in order, without repeats."""
    out = []
    for token in spec.split():
        for row, column in figures.keys_for(token):
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
            keys = figures.keys_for(words[1])
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
        return any(index in wanted for index in presses)
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
    for name in ("sdcard", "internal"):
        shutil.rmtree(work / name, ignore_errors=True)
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
    count = 0
    for png in sorted(raw.glob("guide-*.png")):
        if Image is None:
            shutil.copyfile(png, img / png.name)
        else:
            with Image.open(png) as shot:
                shot.convert("RGB").quantize(colors=64, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).save(
                    img / png.name, optimize=True)
        count += 1
    return count


# ---------------------------------------------------------------------------------------------
# The page: lessons are rendered once with both languages in them (data-l), like the manual.

PART_WORD = {"zh": "第 {n} 课", "en": "Lesson {n}"}
UI = {
    "show": {"zh": "▶ 演示", "en": "▶ Show me"},
    "skip": {"zh": "跳过这一步", "en": "Skip this step"},
    "next": {"zh": "下一步", "en": "Next"},
    "back": {"zh": "上一步", "en": "Back"},
    "sound": {"zh": "声音", "en": "Sound"},
    "replay": {"zh": "重播", "en": "Play again"},
    "more": {"zh": "字典里的详细说明", "en": "In the reference"},
    "keys": {"zh": "要按的键", "en": "Keys to press"},
    "screen": {"zh": "会看到", "en": "You will see"},
}


def both(texts, render=html.escape):
    return "".join(f'<span data-l="{lang}">{render(texts[lang], lang)}</span>' for lang in ("zh", "en"))


def plain_both(texts):
    return both(texts, lambda text, lang: html.escape(text))


def ui(name):
    return plain_both(UI[name])


def keys_figure(spec, seed):
    groups = [(token, "") for token in spec.split()] if spec.strip() else []
    svg, _items = figures.keymap(groups, spec, seed)
    return svg


def render(guide, say, dictionary_ids, leds):
    """-> (rail html, home html, lessons html, lesson index json). say(text, lang) renders inline markup."""
    numbers, rail, cards, sections = {}, [], [], []
    order = lesson_ids(guide)
    for part in guide["parts"]:
        items, part_cards = [], []
        for number, lesson_id in enumerate(part["lessons"], start=1):
            lesson = guide["lessons"][lesson_id]
            numbers[lesson_id] = (part, number)
            items.append(f'<li><a href="#/{lesson_id}" data-lesson="{lesson_id}"><span class="n">{number}</span>'
                         f'<span class="t">{plain_both(lesson["title"])}</span><span class="ok" aria-hidden="true"></span></a></li>')
            if "anim" in lesson:
                thumb = f'<div class="c-art" data-anim-thumb="{lesson["anim"]}"></div>'
            else:
                thumb = f'<div class="c-art"><img src="img/{shot_name(lesson_id, len(lesson["steps"]))}.png" alt="" loading="lazy"></div>'
            part_cards.append(f'<a class="card" href="#/{lesson_id}" data-lesson="{lesson_id}">{thumb}<div class="c-text">'
                              f'<span class="c-num">{number}</span><b>{plain_both(lesson["title"])}</b>'
                              f'<small>{plain_both(lesson["lead"])}</small></div><span class="ok" aria-hidden="true"></span></a>')
        rail.append(f'<div class="g-part"><h3>{plain_both(part["title"])}</h3><ol>{"".join(items)}</ol></div>')
        cards.append(f'<section class="cards-part"><h2>{plain_both(part["title"])}</h2><div class="cards">{"".join(part_cards)}</div></section>')

    for position, lesson_id in enumerate(order):
        lesson = guide["lessons"][lesson_id]
        part, number = numbers[lesson_id]
        more = lesson.get("more")
        if more and more not in dictionary_ids:
            sys.exit(f"guide: {lesson_id} links to unknown manual section #{more}")
        steps = []
        for index, step in enumerate(lesson["steps"], start=1):
            text = both(step["say"], say)
            if "anim" in lesson:
                steps.append(f'<li class="step"><button type="button" class="s-head"><span class="s-num">{index}</span>'
                             f'<span class="s-say">{text}</span><span class="s-tick" aria-hidden="true"></span></button></li>')
                continue
            keys = key_indices(step.get("keys", ""))
            demo = [list(action) for action in parse_demo(step["demo"])]
            # Key presses are checked by KeyIndex on the page.
            done = [[c[0], key_indices(c[1])] if c[0] == "press" else c for c in step["done"]]
            shot = shot_name(lesson_id, index)
            marks = [("mark", target, "") for target in step.get("marks", [])]
            picture, _items = figures.screen(f"img/{shot}.png", marks, f"guide:{shot}", plain(step["say"]["en"]), 1, True)
            steps.append(
                f'<li class="step" data-keys="{",".join(map(str, keys))}" data-done=\'{html.escape(json.dumps(done))}\' '
                f'data-demo=\'{html.escape(json.dumps(demo))}\'>'
                f'<button type="button" class="s-head"><span class="s-num">{index}</span><span class="s-say">{text}</span>'
                f'<span class="s-tick" aria-hidden="true"></span></button>'
                f'<div class="s-body">'
                f'<figure class="s-keys"><figcaption>{ui("keys")}</figcaption>{keys_figure(step.get("keys", ""), "guide:" + shot)}</figure>'
                f'<div class="s-row"><figure class="s-shot"><figcaption>{ui("screen")}</figcaption><div class="bezel">{picture}</div></figure>'
                f'<div class="s-bar"><button type="button" class="s-show">{ui("show")}</button>'
                f'<button type="button" class="s-skip">{ui("skip")}</button></div></div></div></li>')
        kind = "anim" if "anim" in lesson else "device"
        attrs = f'data-anim="{lesson["anim"]}"' if kind == "anim" else f'data-start="{html.escape(lesson["start"])}"'
        stage = ""
        if kind == "anim":
            dots = "".join(f'<button type="button" class="a-dot" data-step="{i}" aria-label="{i + 1}"></button>'
                           for i in range(len(lesson["steps"])))
            stage = (f'<div class="anim-wrap"><div class="anim-stage" data-anim="{lesson["anim"]}"></div>'
                     f'<button type="button" class="a-sound" aria-pressed="false">♪ {ui("sound")}</button>'
                     f'<button type="button" class="a-replay" hidden>↻ {ui("replay")}</button></div>'
                     f'<div class="anim-bar"><button type="button" class="a-back" aria-label="Back">←</button>'
                     f'<div class="anim-say"><div class="anim-dots">{dots}</div><p class="anim-text"></p></div>'
                     f'<button type="button" class="a-next">{ui("next")} →</button></div>')
        prev_id = order[position - 1] if position > 0 else None
        next_id = order[position + 1] if position + 1 < len(order) else None
        pager = '<nav class="l-pager">'
        if prev_id:
            pager += f'<a class="prev" href="#/{prev_id}">← {plain_both(guide["lessons"][prev_id]["title"])}</a>'
        pager += f'<a class="next" href="#/{next_id}">{plain_both(guide["lessons"][next_id]["title"])} →</a>' if next_id else \
            '<a class="next" href="#/">' + plain_both({"zh": "回到目录", "en": "All lessons"}) + ' →</a>'
        pager += "</nav>"
        more_link = (f'<p class="l-more"><a href="reference.html#{more}">{ui("more")} →</a></p>' if more else "")
        sections.append(
            f'<section class="lesson" id="lesson-{lesson_id}" data-lesson="{lesson_id}" data-kind="{kind}" {attrs} hidden>'
            f'<header class="l-head"><div class="l-kicker">{plain_both(part["title"])} · {number} / {len(part["lessons"])}</div>'
            f'<h1>{plain_both(lesson["title"])}</h1><p class="l-lead">{plain_both(lesson["lead"])}</p></header>'
            f'{stage}<ol class="l-steps">{"".join(steps)}</ol>'
            f'<div class="l-done" hidden><b>{plain_both({"zh": "完成！", "en": "Done!"})}</b>'
            f'<span>{plain_both({"zh": "这一课学会了。", "en": "You have got this one."})}</span></div>'
            f'{pager}{more_link}</section>')
    index = {"order": order, "dictionary": sorted(dictionary_ids)}
    return "".join(rail), "".join(cards), "".join(sections), index


def plain(text):
    text = re.sub(r"\{\{(?:key|hold|scene|page):([^}]+)\}\}", r"\1", text)
    text = re.sub(r"\{\{track:(\d)\}\}", r"T\1", text)
    return re.sub(r"\s+", " ", text).strip()
