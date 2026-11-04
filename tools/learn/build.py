#!/usr/bin/env python3
"""Builds the Learn manual: a bilingual (中文 / English), searchable web page with screenshots of the
real UI, key-light diagrams, an interactive device map and generated reference tables.

  python3 tools/learn/build.py                     build into build/web/site/learn
  python3 tools/learn/build.py --out DIR           somewhere else
  python3 tools/learn/build.py --skip-shots        reuse the screenshots of the last run

Sources (docs/learn/):
  manual.json        version, title, chapter order
  zh/*.md, en/*.md   chapters, one file per chapter and language, same section ids in both
  synonyms.json      search synonyms: groups of words that mean the same (both languages)

The host build (build/linux-groove, or LEARN_HOST_BUILD) provides CardenzaSimShot (screenshots of
the firmware UI and its key lights) and CardenzaLearnDump (pages, parameters and shortcuts as JSON).

Markdown subset: # / ## / ### headings with {#id}, paragraphs, - and 1. lists, pipe tables,
> notes (> [!tip] / > [!warn]), **bold**, *em*, `code`, [text](url), and these extensions:
  [[#id]] or [[#id|text]]   link to a section (its title in the reader's language)
  {{key:ESC}}               a key cap in its role colour; {{key:FN+6}} a chord; {{hold:ESC}} held
  {{icon:seq}}              an icon (tools/learn/figures.py ICONS)
  {{led:half}} {{led:half|半亮}}   a key light: off / half / full / blink
  {{scene:SEQ}} {{track:2}} {{page:ROLL}}   chips as the screen draws them
  {{shot:name|caption}}     screenshot with the key lights of that moment
  {{ref:engine:JUNO}}       generated parameter reference of an engine
  {{ref:fn}} {{ref:fn-quick}} {{ref:lists}} {{ref:pages}}
  {{map:device}} {{map:scenes}}
  <!-- keywords: a, b -->    extra search words for the section it sits in

Picture blocks (the opening line has no closing braces; a line "}}" ends the block):
  {{shot:name|caption       a screenshot with numbered highlights and a legend under it:
  mark cell:2,3 = label     one per line (tools/learn/figures.py): mark TARGET = label (screen),
  key ESC = label           key KEYS = label (the key-light diagram)
  }}
  {{strip                   numbered steps: KEYS > PICTURE | caption, one per line; PICTURE is a
  TAB > seq-keyboard | ..   screenshot with @TARGET highlights, or -
  }}
  {{keymap:caption          the keyboard with groups of keys highlighted and numbered:
  ESC TAB FN CTRL = label   KEYS = label, one group per line
  }}
"""
import argparse
import datetime
import html
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys

import figures
import guide as guide_lessons

ROOT = pathlib.Path(__file__).resolve().parents[2]
LEARN = ROOT / "docs" / "learn"
HERE = pathlib.Path(__file__).resolve().parent
LANGS = ("zh", "en")
ADD, SUB = "0,13", "1,13"

# ---------------------------------------------------------------------------------------------
# Screenshots: one script walks the firmware UI; every shot waits until animations have settled.

def grid_key(index):
    return f"{index // 4},{9 + index % 4}"


class Script:
    def __init__(self):
        self.lines = ["wait 140"]

    def act(self, *commands):
        self.lines.extend(commands)
        return self

    def wait(self, frames=45):
        self.lines.append(f"wait {frames}")
        return self

    def shot(self, name, frames=45):
        self.wait(frames)
        self.lines.append(f"screenshot {name}")
        return self

    def scene(self, number):
        # The on-screen scene number: 0 SETUP, 1 INSTR, 2 SEQ, 3 TAPE (ESC map column = number).
        return self.act("down esc", f"tap {grid_key(number)}", "up esc").wait(30)

    def track(self, number):
        return self.act("down tab", f"tap {grid_key(number - 1)}", "up tab").wait(30)

    def page(self, index):
        return self.act("down ctrl", f"tap {grid_key(index)}", "up ctrl").wait(30)


def shot_script():
    s = Script()
    # The boot screen: INSTR VOICE on T1 (the drums). Then SEQ, T1 drums and a first beat (kick,
    # snare, hat).
    s.shot("instr-boot")
    s.scene(2).shot("seq-empty")
    s.track(1)
    s.act("down 3,1").shot("seq-drum-held", 20)
    s.act(*[f"tap {grid_key(i)}" for i in (0, 4, 8, 12)], "up 3,1")
    s.act("down 3,2", f"tap {grid_key(4)}", f"tap {grid_key(12)}", "up 3,2")
    s.act("down 3,3", *[f"tap {grid_key(i)}" for i in range(0, 16, 2)], "up 3,3")
    s.shot("seq-drums")
    # Hold a hit (CH on step 1) and ADD: its velocity on screen; a changed hit stays.
    s.act(f"down {grid_key(0)}").wait(20).act("tap del").shot("seq-drum-edit", 10).act(f"up {grid_key(0)}").wait(120)
    s.act("tap space").shot("seq-drums-playing", 40)
    s.act("tap space").wait(40)
    # TAB: next track; the left keys show which one for a moment.
    s.act("tap tab").shot("tab-hint", 22)
    s.shot("seq-keyboard", 60)
    # Notes: hold a pad, tap steps.
    # On a melodic track C3 is pad 2,1 (row 3 is the white keys' gap row).
    s.act("down 2,1").shot("seq-hold-pad", 20)
    s.act(f"tap {grid_key(0)}").shot("seq-hold-tap", 10)
    s.act(f"tap {grid_key(4)}", f"tap {grid_key(8)}", "up 2,1")
    s.act("down 2,3", f"tap {grid_key(2)}", f"tap {grid_key(10)}", "up 2,3")
    s.act("down 2,5", f"tap {grid_key(12)}", f"tap {grid_key(14)}", "up 2,5")
    s.shot("seq-notes")
    s.act(f"tap {grid_key(4)}")
    s.shot("seq-selected")
    s.act("tap enter", "tap enter")
    s.shot("seq-lane")
    s.act(f"tap {grid_key(4)}").wait(20)
    s.act("tap space").shot("seq-playing", 30)
    s.act("tap space").wait(40)
    # STEP REC: OK switches the view; pads write into the cursor step.
    s.act("tap enter").shot("seq-step-view", 30).act(f"tap {grid_key(1)}", "tap 2,2", "tap 2,3")
    s.shot("seq-step", 140)
    s.act("tap enter").wait(200)
    # Capture: a few phrases, then hold OK for the RECORD panel; OK puts a bar into a new pattern.
    for pad in ("2,1", "2,3", "2,5", "2,3", "2,1"):
        s.act(f"down {pad}").wait(8).act(f"up {pad}").wait(10)
    s.wait(200)
    for pad in ("2,5", "2,6", "2,8"):
        s.act(f"down {pad}").wait(12).act(f"up {pad}").wait(6)
    s.act("down enter").wait(40).act("up enter").wait(10).act(f"tap {grid_key(5)}")
    s.shot("record", 30)
    s.act("tap enter").shot("record-placed", 40)
    s.page(1).shot("seq-pattern")
    s.page(2).shot("seq-track")
    s.page(0)
    s.act("down ctrl", f"tap {grid_key(1)}", "up ctrl").wait(30)
    s.act(f"tap {grid_key(0)}").wait(20)
    s.page(0)
    # Hold maps.
    s.act("down esc").shot("map-esc", 30).act("up esc")
    s.act("down tab").shot("map-tab", 30).act("up tab")
    s.act("down ctrl").shot("map-ctrl", 30).act("up ctrl").wait(30)
    s.act("down fn").shot("fn-held", 30)
    s.act("tap 0,6").shot("fn-quick", 6)
    s.act("tap 1,6").wait(60).act("tap 0,7").shot("fn-volume", 6)
    s.act("tap 1,7").wait(60).act("tap 0,8").shot("fn-octave", 6)
    s.act("tap 1,8").act("up fn").wait(60)
    # A tap on FN keeps the menu open.
    s.act("tap fn").shot("fn-menu-open", 30).act("tap fn").wait(40)
    # INSTR on T2 (Juno): VOICE, CHORD, ARP, DCO, VCF, ENV, LFO, FX; and its FN menu.
    s.scene(1).page(0).shot("instr-voice")
    s.act("down fn").shot("fn-menu-instr", 30).act("up fn").wait(40)
    s.act(f"tap {grid_key(0)}").shot("engine-picker").act("tap esc").wait(30)
    s.act(f"tap {grid_key(1)}").shot("preset-browser")
    # The pads do not type in a browser: FN + F opens the search field.
    s.act("down fn", "tap f", "up fn").wait(10).act("tap b", "tap a", "tap s", "tap s").shot("preset-search", 30)
    s.act("tap enter").shot("preset-filtered", 30).act("tap esc").wait(10).act("tap esc").wait(30)
    # CHORD and ARP: the track's own MIDI effects.
    s.page(1).shot("instr-chord")
    s.act(f"tap {grid_key(0)}").shot("chord-on")
    s.act("down 2,1").shot("chord-held", 20).act("up 2,1").wait(10)
    s.act(*["tap 1,13"] * 12).wait(20)
    s.page(2).shot("instr-arp")
    s.act(f"tap {grid_key(0)}").wait(10)
    s.act("down 2,1", "down 2,3", "down 2,5").shot("arp-running", 50).act("up 2,1", "up 2,3", "up 2,5")
    s.act(f"tap {grid_key(0)}", *["tap 1,13"] * 8).wait(20)
    for index, name in enumerate(["dco", "vcf", "env", "lfo", "fx"], start=3):
        s.page(index).shot(f"instr-juno-{name}")
    # T3 DX7: VOICE, CHORD, ARP, ALGO, LEVELS, OP1-OP6, LFO, FX.
    s.track(3)
    for index, name in [(0, "voice"), (3, "algo"), (4, "levels"), (5, "op1"), (11, "lfo")]:
        s.page(index).shot(f"instr-dx7-{name}")
    # T1 drums (VOICE, KIT, FX), T4 sampler (VOICE, CHORD, ARP, SAMPLE, TONE, FX).
    s.track(1).page(1).shot("instr-drums-kit")
    s.track(4).page(3).shot("instr-sampler-sample")
    s.act(f"tap {grid_key(0)}").shot("sampler-files", 30).act("tap esc").wait(30)
    s.act(f"tap {grid_key(1)}").shot("sampler-rec", 30).act("tap esc").wait(30)
    s.page(4).shot("instr-sampler-tone")
    s.track(2).page(0)
    # SETUP (scene 0, ESC + GRID column 1): KEYS with the keyboard layouts, SYSTEM with the LED ramp.
    s.scene(0)
    s.page(2).shot("setup-keys")
    s.act(f"tap {grid_key(0)}").shot("keys-scale").act(f"tap {grid_key(0)}").shot("keys-chroma")
    s.act(f"tap {grid_key(0)}").wait(20)
    s.page(3).shot("setup-system")
    s.act(f"tap {grid_key(1)}").shot("setup-keyled")
    s.act(f"tap {grid_key(0)}").act("tap esc").wait(40)
    # TAPE: a few song rows first (SESSION capture).
    s.scene(3).page(0).shot("tape-session")
    s.act("tap space").wait(20).act(f"tap {grid_key(5)}").shot("tape-session-queued", 8)
    s.act("tap space").wait(40).act(f"tap {grid_key(1)}").wait(20)
    s.act("down fn", "tap enter", "up fn").wait(10)
    s.act("down fn", "tap enter", "up fn").wait(10)
    s.page(1).shot("tape-song")
    s.page(2).shot("tape-mix")
    s.page(3).shot("tape-sends")
    s.page(4).shot("tape-fx")
    # SETUP PROJECT (SAVE, LOAD, NEW) and FILES; SAVE AS is in the FN menu.
    s.scene(0).page(0).shot("setup-project")
    s.act("down fn", f"tap {grid_key(2)}", "up fn").shot("text-entry", 30)
    s.act("tap esc").wait(30)
    s.act(f"tap {grid_key(2)}").shot("confirm")
    s.act("tap esc").wait(30)
    s.act("down fn").shot("fn-menu-setup", 30).act("up fn").wait(40)
    s.act("tap esc").wait(30)
    # Recording armed while playing.
    s.scene(2).page(0).act("down fn", "tap space", "up fn", "tap space").shot("seq-recording", 30)
    s.act("tap space", "down fn", "tap space", "up fn").wait(40)
    # FN + S: a new project is saved at once as SONG 01, no name field.
    s.act("down fn", "tap s", "up fn").shot("save-done", 30)
    s.wait(60)
    # FILES after the save: the list shows the project.
    s.scene(0).page(1).shot("setup-files")
    s.act("tap esc").wait(30)
    # A long pattern: SEQ menu LEN + twice (16 -> 48 steps), then FN + ADD to its second page.
    s.scene(2).page(0)
    s.act("down fn", f"tap {grid_key(11)}", f"tap {grid_key(11)}", "tap del", "up fn").shot("seq-long", 30)
    return "\n".join(s.lines) + "\n"


def run_shots(sim_build, out_dir, work):
    shot_tool = sim_build / "Devices" / "CardenzaSim" / "CardenzaSimShot"
    if not shot_tool.exists():
        sys.exit(f"learn: {shot_tool} missing (build the host target CardenzaSimShot)")
    script = work / "learn.script"
    script.write_text(shot_script())
    raw = work / "shots"
    shutil.rmtree(raw, ignore_errors=True)
    # A fresh card and internal flash every run: settings, projects and the autosaved working state
    # from the last run would change the screens.
    shutil.rmtree(work / "sdcard", ignore_errors=True)
    shutil.rmtree(work / "internal", ignore_errors=True)
    subprocess.run([str(shot_tool), "--script", str(script), "--out", str(raw), "--scale", "2"], check=True,
                   cwd=work, stdout=subprocess.DEVNULL)
    leds = {}
    img = out_dir / "img"
    img.mkdir(parents=True, exist_ok=True)
    try:
        from PIL import Image  # optional: the simulator writes uncompressed PNGs
    except ImportError:
        Image = None
    for png in sorted(raw.glob("*.png")):
        if Image is None:
            shutil.copyfile(png, img / png.name)
        else:
            # A screen has a handful of colours: a palette PNG is a few KB instead of 390.
            with Image.open(png) as shot:
                shot.convert("RGB").quantize(colors=64, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).save(
                    img / png.name, optimize=True)
        led_file = png.with_suffix(".leds.txt")
        if led_file.exists():
            rows = [list(map(int, line.split())) for line in led_file.read_text().splitlines()[:4]]
            leds[png.stem] = rows
    (work / "leds.json").write_text(json.dumps(leds))
    return leds


# Where the live device goes when a screenshot's "try it" opens it: scene, track and page, picked
# through the hold maps like a person would (tools/learn/learn.js runs them).
def shot_setup(name):
    # Scenes by their on-screen number (0 SETUP, 1 INSTR, 2 SEQ, 3 TAPE); tracks 1-4; pages from 0.
    first_track = ("seq-drum-held", "seq-drums", "seq-drum-edit", "seq-drums-playing")
    steps = []
    setup_pages = {"setup-project": 0, "text-entry": 0, "confirm": 0, "fn-menu-setup": 0, "setup-files": 1, "setup-keys": 2, "keys-scale": 2,
                   "keys-chroma": 2, "setup-system": 3, "setup-keyled": 3}
    instr_pages = {"instr-chord": (2, 1), "chord-on": (2, 1), "chord-held": (2, 1), "instr-arp": (2, 2), "arp-running": (2, 2),
                   "instr-juno-dco": (2, 3), "instr-juno-vcf": (2, 4), "instr-juno-env": (2, 5), "instr-juno-lfo": (2, 6),
                   "instr-juno-fx": (2, 7), "instr-dx7-voice": (3, 0), "instr-dx7-algo": (3, 3), "instr-dx7-levels": (3, 4),
                   "instr-dx7-op1": (3, 5), "instr-dx7-lfo": (3, 11), "instr-drums-kit": (1, 1), "instr-sampler-sample": (4, 3),
                   "instr-sampler-tone": (4, 4), "sampler-files": (4, 3), "sampler-rec": (4, 3)}
    if name in setup_pages:
        steps = [["scene", 0], ["track", 2], ["page", setup_pages[name]]]
    elif name in instr_pages or name.startswith(("instr-", "engine-", "preset-", "fn-menu-instr")):
        track, page = instr_pages.get(name, (2, 0))
        steps = [["scene", 1], ["track", track], ["page", page]]
    elif name.startswith(("seq-", "record", "fn-", "map-", "tab-", "save-")):
        steps = [["scene", 2], ["track", 1 if name in first_track else 2]]
        steps.append(["page", 1 if name == "seq-pattern" else 2 if name == "seq-track" else 0])
    elif name.startswith("tape-"):
        pages = {"tape-session": 0, "tape-session-queued": 0, "tape-song": 1, "tape-mix": 2, "tape-sends": 3, "tape-fx": 4}
        steps = [["scene", 3], ["page", pages.get(name, 0)]]
    return steps


TRY = {"zh": ("▶ 在这里跟着操作", "跳到这个画面", "收起"), "en": ("▶ Try it live here", "Go to this screen", "Close")}

# ---------------------------------------------------------------------------------------------
# Key lights (Devices/LedLight.h), so diagrams look like the keys: the perceived level through the
# gamma table to CardenzaHAL's linear key duty, times the HAL's gamma-2 global PWM.
KEY_LEVEL_FOR_PERCEIVED = (0, 1, 1, 1, 1, 1, 1, 2, 3, 4, 5, 7, 8, 10, 13, 15)


def led_light(level, master=15):
    return (KEY_LEVEL_FOR_PERCEIVED[min(level, 15)] / 15) * (min(master, 15) / 15) ** 2


LEGEND = [
    ["ESC", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "↑"],
    ["TAB", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]", "↓"],
    ["FN", "", "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", "↵"],
    ["CTRL", "", "", "Z", "X", "C", "V", "B", "N", "M", ",", ".", "/", "▶"],
]


def keyboard_svg(levels, label="", marks=(), seed="kbd", first=1):
    """4 x 14 keys, each glowing with its LED's light (one LED colour, as on the board), with highlights."""
    return figures.keyboard(levels, led_light, marks, seed, label, first)


# ---------------------------------------------------------------------------------------------
# Generated reference from CardenzaLearnDump.

def run_dump(sim_build):
    tool = sim_build / "tests" / "CardenzaLearnDump"
    if not tool.exists():
        sys.exit(f"learn: {tool} missing (build the host target CardenzaLearnDump)")
    out = subprocess.run([str(tool)], check=True, capture_output=True, text=True).stdout
    return json.loads(out)


T = {
    "zh": {"page": "页面", "slot": "GRID 位置", "param": "参数", "range": "范围", "default": "默认", "options": "选项",
           "action": "动作", "row": "行", "col": "列", "engine": "音源", "track": "默认轨道", "preset": "默认音色",
           "key": "键", "does": "作用", "toggle": "开 / 关", "on": "目录", "search": "搜索手册",
           "actions": {"engine": "选择音源", "preset": "音色浏览", "save": "保存音色", "save-as": "另存音色",
                       "sample-file": "采样文件", "sample-record": "录音", "sample-preview": "试听"}},
    "en": {"page": "Page", "slot": "GRID key", "param": "Parameter", "range": "Range", "default": "Default", "options": "Options",
           "action": "Action", "row": "row", "col": "col", "engine": "Engine", "track": "Default track", "preset": "Default sound",
           "key": "Key", "does": "Does", "toggle": "on / off", "on": "Contents", "search": "Search the manual",
           "actions": {"engine": "Choose engine", "preset": "Browse sounds", "save": "Save sound", "save-as": "Save sound as",
                       "sample-file": "Sample file", "sample-record": "Record", "sample-preview": "Preview"}},
}


def grid_name(index, lang):
    legend = LEGEND[index // 4][9 + index % 4]
    t = T[lang]
    return f'<span class="kc">{html.escape(legend)}</span> <small>{t["row"]} {index // 4 + 1} · {t["col"]} {index % 4 + 1}</small>'


def param_cells(param, lang):
    t = T[lang]
    if not param:
        return "—", "—"
    if param.get("options"):
        rng = " / ".join(html.escape(o) for o in param["options"])
    else:
        rng = f'{html.escape(param.get("minText", str(param["min"])))} … {html.escape(param.get("maxText", str(param["max"])))}'
    default = html.escape(param.get("defaultText", param.get("valueText", str(param.get("default", "")))))
    return rng, default


def engine_reference(dump, engine, lang):
    t = T[lang]
    entry = next((e for e in dump["engines"] if e["engine"] == engine), None)
    if entry is None:
        return f"<p>(no engine {html.escape(engine)})</p>"
    parts = [f'<p class="ref-meta">{t["engine"]}: <b>{engine}</b> · {t["track"]}: T{entry["track"]} · '
             f'{t["preset"]}: {html.escape(entry["preset"])}</p>']
    for page in entry["pages"]:
        parts.append(f'<h4 class="ref-page">{html.escape(page["tab"])} <small>{html.escape(page["name"])}</small></h4>')
        parts.append(f'<div class="table-wrap"><table class="ref"><thead><tr><th>{t["slot"]}</th><th>{t["param"]}</th>'
                     f'<th>{t["range"]}</th><th>{t["default"]}</th></tr></thead><tbody>')
        for slot in page["slots"]:
            if "action" in slot:
                rng, default = t["actions"].get(slot["action"], slot["action"]), "—"
            else:
                rng, default = param_cells(slot.get("param"), lang)
            parts.append(f'<tr><td>{grid_name(slot["index"], lang)}</td><td><b>{html.escape(slot["label"])}</b>'
                         f'{" <code>" + html.escape(slot["id"]) + "</code>" if slot.get("id") else ""}</td><td>{rng}</td><td>{default}</td></tr>')
        parts.append("</tbody></table></div>")
    return "".join(parts)


def fn_reference(dump, lang):
    t = T[lang]
    parts = []
    for page in dump["settingsPages"]:
        parts.append(f'<h4 class="ref-page">SETUP {html.escape(page["tab"])} <small>{html.escape(page["name"])}</small></h4>')
        parts.append(f'<div class="table-wrap"><table class="ref"><thead><tr><th>{t["slot"]}</th><th>{t["param"]}</th>'
                     f'<th>{t["range"]}</th><th>{t["default"]}</th></tr></thead><tbody>')
        for slot in page["slots"]:
            param = slot.get("param") or {}
            if param.get("kind") == "range":
                rng = f'{param["min"]} … {param["max"]}'
            elif param.get("kind") == "toggle":
                rng = t["toggle"]
            else:
                rng = f'{param.get("max", 0) - param.get("min", 0) + 1} {t["options"]}'
            parts.append(f'<tr><td>{grid_name(slot["index"], lang)}</td><td><b>{html.escape(slot["label"])}</b> <code>{html.escape(slot.get("id", ""))}</code></td>'
                         f'<td>{rng}</td><td>{html.escape(param.get("valueText", ""))}</td></tr>')
        parts.append("</tbody></table></div>")
    return "".join(parts)


def fn_quick_reference(dump, lang):
    t = T[lang]
    rows = "".join(f'<tr><td><span class="kc">FN</span>+<span class="kc">{q["up"]}</span> / <span class="kc">FN</span>+<span class="kc">{q["down"]}</span></td>'
                   f'<td>{html.escape(q["label"])} + / −</td></tr>' for q in dump["fnQuick"])
    return f'<div class="table-wrap"><table class="ref"><thead><tr><th>{t["key"]}</th><th>{t["does"]}</th></tr></thead><tbody>{rows}</tbody></table></div>'


def lists_reference(dump, lang):
    names = {"zh": {"scales": "音阶", "chords": "和弦", "voicings": "转位", "layouts": "键盘布局", "locks": "音阶锁定",
                    "arpModes": "ARP 模式", "arpRates": "ARP 速度"},
             "en": {"scales": "Scales", "chords": "Chords", "voicings": "Voicings", "layouts": "Keyboard layouts",
                    "locks": "Scale lock", "arpModes": "Arp modes", "arpRates": "Arp rates"}}[lang]
    rows = "".join(f'<tr><th>{names[key]}</th><td>{" · ".join(html.escape(v) for v in values)}</td></tr>'
                   for key, values in dump["lists"].items())
    return f'<div class="table-wrap"><table class="ref lists"><tbody>{rows}</tbody></table></div>'


def pages_reference(dump, lang):
    rows = []
    rows.append(("INSTR", " · ".join(f'{e["engine"]}: ' + " ".join(p["tab"] for p in e["pages"]) for e in dump["engines"])))
    for scene, pages in dump["scenes"].items():
        rows.append((scene, " · ".join(p["tab"] for p in pages)))
    body = "".join(f"<tr><th>{a}</th><td>{b}</td></tr>" for a, b in rows)
    return f'<div class="table-wrap"><table class="ref lists"><tbody>{body}</tbody></table></div>'


# ---------------------------------------------------------------------------------------------
# Maps: the device (every key and screen area links to its section) and the scenes and pages.

DEVICE_LINKS = {
    # (row, col) -> section id
    "left": ["nav-scenes", "nav-tracks", "fn-menu", "nav-pages"],
    "right": ["keys-right", "keys-right", "keys-right", "seq-transport"],
}
MAP_TEXT = {
    "zh": {"esc": "ESC：切换场景 / 返回", "tab": "TAB：切换轨道", "fn": "FN：当前画面的菜单", "ctrl": "CTRL：切换页面",
           "add": "ADD：加", "sub": "SUB：减", "enter": "ENTER：确认", "play": "PLAY：播放 / 停止",
           "pads": "演奏区：弹当前轨", "grid": "GRID：屏幕 4×4", "quick": "FN 快捷：BPM / 音量 / 八度",
           "screen": "屏幕", "header": "状态行", "footer": "页签", "content": "内容区（对应 GRID）", "strips": "两侧色块 = 两侧按键",
           "home": "HOME 侧键：不用（全部停止是 FN + ESC）"},
    "en": {"esc": "ESC: scenes / back", "tab": "TAB: tracks", "fn": "FN: the menu of this screen", "ctrl": "CTRL: pages",
           "add": "ADD: increase", "sub": "SUB: decrease", "enter": "ENTER: confirm", "play": "PLAY: play / stop",
           "pads": "Pads: play the current track", "grid": "GRID: the screen's 4×4", "quick": "FN quick: BPM / volume / octave",
           "screen": "Screen", "header": "Status line", "footer": "Page tabs", "content": "Content (mirrors the GRID)",
           "strips": "Edge strips = edge keys", "home": "HOME side key: unused (stop all is FN + ESC)"},
}


def device_map(lang):
    m = MAP_TEXT[lang]
    w, h, gap = 34, 30, 4
    ox, oy = 10, 196
    width = ox * 2 + 14 * (w + gap) + 12
    parts = [f'<svg class="device-map" viewBox="0 0 {width} {oy + 4 * (h + gap) + 14}" role="img" aria-label="Cardenza">']
    # Screen with its areas.
    sx, sy, sw, sh = width / 2 - 170, 10, 340, 172
    parts.append(f'<rect class="body" x="4" y="4" width="{width - 8}" height="{oy + 4 * (h + gap) + 6}" rx="16"/>')

    def area(x, y, w2, h2, href, label, cls=""):
        parts.append(f'<a href="#{href}" class="map-link {cls}"><title>{html.escape(label)}</title>'
                     f'<rect x="{x}" y="{y}" width="{w2}" height="{h2}" rx="3"/></a>')

    parts.append(f'<rect class="screen" x="{sx}" y="{sy}" width="{sw}" height="{sh}" rx="6"/>')
    area(sx + 14, sy + 6, sw - 28, 16, "screen-layout", m["header"], "zone")
    area(sx + 14, sy + 26, sw - 28, sh - 52, "keys-grid", m["content"], "zone")
    area(sx + 14, sy + sh - 22, sw - 28, 16, "nav-pages", m["footer"], "zone")
    for row, color in enumerate(["esc", "tab", "fn", "ctrl"]):
        area(sx + 2, sy + 6 + row * (sh - 12) / 4, 9, (sh - 12) / 4 - 3, "lights-edges", m["strips"], f"strip {color}")
        area(sx + sw - 11, sy + 6 + row * (sh - 12) / 4, 9, (sh - 12) / 4 - 3, "lights-edges", m["strips"], "strip right")
    parts.append(f'<text class="map-label" x="{sx + sw / 2}" y="{sy + sh / 2 + 4}">{html.escape(m["screen"])}</text>')
    area(width - 26, 60, 12, 40, "keys-home", m["home"], "home")
    for row in range(4):
        for col in range(14):
            x = ox + col * (w + gap) + (4 if col >= 1 else 0) + (4 if col >= 9 else 0) + (4 if col >= 13 else 0)
            y = oy + row * (h + gap)
            legend = LEGEND[row][col]
            if col == 0:
                href, label, cls = DEVICE_LINKS["left"][row], m[["esc", "tab", "fn", "ctrl"][row]], ["esc", "tab", "fn", "ctrl"][row]
            elif col == 13:
                href, label, cls = DEVICE_LINKS["right"][row], m[["add", "sub", "enter", "play"][row]], "right"
            elif col >= 9:
                href, label, cls = "keys-grid", m["grid"], "grid"
            elif (row, col) in ((0, 6), (0, 7), (0, 8), (1, 6), (1, 7), (1, 8)):
                href, label, cls = "fn-quick", m["quick"], "pad quick"
            else:
                href, label, cls = "play-keyboard", m["pads"], "pad"
            parts.append(f'<a href="#{href}" class="map-key {cls}"><title>{html.escape(label)}</title>'
                         f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="5"/>'
                         f'<text x="{x + w / 2}" y="{y + h / 2 + 4}">{html.escape(legend)}</text></a>')
    parts.append("</svg>")
    legend_items = "".join(f'<li><span class="sw {c}"></span>{html.escape(m[k])}</li>' for c, k in
                           [("esc", "esc"), ("tab", "tab"), ("fn", "fn"), ("ctrl", "ctrl"), ("pad", "pads"), ("grid", "grid"),
                            ("quick", "quick"), ("right", "play")])
    return f'<figure class="device">{"".join(parts)}<ul class="map-legend">{legend_items}</ul></figure>'


SCENE_MAP = [
    # (title, section, [(label, section, shot)])
    ("0 SETUP", "setup", [("PROJECT", "setup-project", "setup-project"), ("FILES", "setup-files", "setup-files"),
                          ("KEYS", "setup-keys", "setup-keys"), ("SYSTEM", "setup-system", "setup-system")]),
    ("1 INSTR", "instr", [("VOICE", "instr-voice", "instr-voice"), ("CHORD", "play-chords", "instr-chord"), ("ARP", "play-arp", "instr-arp"),
                          ("DRUMS · KIT", "engine-drums", "instr-drums-kit"), ("JUNO · DCO VCF ENV LFO", "engine-juno", "instr-juno-dco"),
                          ("DX7 · ALGO LEVELS OP1–6 LFO", "engine-dx7", "instr-dx7-algo"),
                          ("SAMPLER · SAMPLE TONE", "engine-sampler", "instr-sampler-sample"), ("FX", "instr-fx", "instr-juno-fx")]),
    ("2 SEQ", "seq", [("ROLL", "seq-roll", "seq-notes"), ("STEP REC", "seq-step-rec", "seq-step"), ("RECORD", "record-panel", "record"),
                      ("PATTERN", "seq-patterns", "seq-pattern"), ("TRACK", "seq-track", "seq-track")]),
    ("3 TAPE", "tape", [("SESSION", "tape-session", "tape-session"), ("SONG", "tape-song", "tape-song"), ("MIX", "tape-mix", "tape-mix"),
                        ("SENDS", "tape-sends", "tape-sends"), ("FX", "tape-fx", "tape-fx")]),
    ("HOLD", "nav-maps", [("ESC", "nav-scenes", "map-esc"), ("TAB", "nav-tracks", "map-tab"), ("CTRL", "nav-pages", "map-ctrl"),
                          ("FN", "fn-menu", "fn-held")]),
]


def scene_map(lang):
    titles = {"zh": {"HOLD": "按住左侧键"}, "en": {"HOLD": "Hold a left key"}}[lang]
    cols = []
    for title, section, items in SCENE_MAP:
        cards = "".join(f'<a class="scene-card" href="#{sec}"><img loading="lazy" src="img/{shot}.png" width="480" height="270" alt="{html.escape(label)}">'
                        f'<span>{html.escape(label)}</span></a>' for label, sec, shot in items)
        cols.append(f'<section class="scene-col"><a class="scene-title" href="#{section}">{html.escape(titles.get(title, title))}</a>{cards}</section>')
    return f'<div class="scene-map">{"".join(cols)}</div>'


# ---------------------------------------------------------------------------------------------
# Markdown subset -> HTML, collecting sections for the table of contents and the search index.

class Doc:
    def __init__(self, lang, dump, leds, titles):
        self.lang, self.dump, self.leds, self.titles = lang, dump, leds, titles
        self.sections = []  # {id, level, title, chapter, text[], keywords[]}

    def current(self):
        return self.sections[-1] if self.sections else None


def slug(text):
    return re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")


SCENES = {"INSTR": ("1", "instr"), "SEQ": ("2", "seq"), "TAPE": ("3", "tape"), "SETUP": ("0", "setup"), "FN": ("FN", "fn")}
TRACKS = {"1": "DRUMS", "2": "JUNO", "3": "DX7", "4": "SAMPLER"}
LED_STATES = ("off", "half", "full", "blink")
CHAPTER_ICONS = {"start": "book", "keys": "keyboard", "navigate": "map", "play": "note", "fn": "fn", "instr": "instr", "setup": "list",
                 "sampler": "sample", "seq": "seq", "tape": "tape", "effects": "fx", "lights": "light", "projects": "folder",
                 "web": "web", "faq": "faq", "reference": "list", "changelog": "tag"}
CALLOUT_ICONS = {"tip": "tip", "warn": "warn", "note": "eye", "try": "play", "keys": "keyboard"}


def chip(kind, arg, doc):
    arg = arg.strip()
    if kind == "key":
        return figures.key_sequence(arg, doc.lang)
    if kind == "hold":
        return figures.chord("hold " + arg, doc.lang)
    if kind == "icon":
        return figures.icon(arg)
    if kind == "led":
        state, _, label = arg.partition("|")
        if state not in LED_STATES:
            sys.exit(f"learn: [{doc.lang}] unknown light {state}")
        return f'<span class="ledc"><span class="led {state}" aria-hidden="true"></span>{html.escape(label)}</span>'
    if kind == "scene":
        name = arg.upper()
        if name not in SCENES:
            sys.exit(f"learn: [{doc.lang}] unknown scene {arg}")
        number, _glyph = SCENES[name]
        if number == "FN":
            return f'<span class="chip scene fn"><b>FN</b> MENU</span>'
        return f'<span class="chip scene"><b>{number}</b> {html.escape(name)}</span>'
    if kind == "track":
        if arg not in TRACKS:
            sys.exit(f"learn: [{doc.lang}] unknown track {arg}")
        return f'<span class="chip trk t{arg}"><b>T{arg}</b> {TRACKS[arg]}</span>'
    if kind == "page":
        return f'<span class="chip page">{html.escape(arg)}</span>'
    raise SystemExit(f"learn: unknown chip {kind}")


def inline(text, doc):
    out = []
    pos = 0
    pattern = re.compile(r"`([^`]+)`|\[\[#([\w-]+)(?:\|([^\]]+))?\]\]|\{\{(key|hold|icon|led|scene|track|page):([^}]+)\}\}|\*\*(.+?)\*\*|\*([^*\s][^*]*?)\*|\[([^\]]+)\]\(([^)]+)\)")
    for m in pattern.finditer(text):
        out.append(html.escape(text[pos:m.start()]))
        if m.group(1) is not None:
            out.append(f"<code>{html.escape(m.group(1))}</code>")
        elif m.group(2) is not None:
            target = m.group(2)
            if target not in doc.titles:
                sys.exit(f"learn: [{doc.lang}] link to unknown section #{target}")
            label = m.group(3) or doc.titles[target]
            out.append(f'<a class="xref" href="#{target}">{html.escape(label)}</a>')
        elif m.group(4) is not None:
            out.append(chip(m.group(4), m.group(5), doc))
        elif m.group(6) is not None:
            out.append(f"<strong>{inline(m.group(6), doc)}</strong>")
        elif m.group(7) is not None:
            out.append(f"<em>{inline(m.group(7), doc)}</em>")
        else:
            href = m.group(9)
            external = href.startswith("http")
            out.append(f'<a href="{html.escape(href)}"{" target=_blank rel=noopener" if external else ""}>{inline(m.group(8), doc)}</a>')
        pos = m.end()
    out.append(html.escape(text[pos:]))
    return "".join(out)


def plain(text):
    text = re.sub(r"\{\{icon:[^}]*\}\}", "", text)
    text = re.sub(r"\{\{led:\w+\|?([^}]*)\}\}", r"\1", text)
    text = re.sub(r"\{\{track:(\d)\}\}", lambda m: f"T{m.group(1)} {TRACKS.get(m.group(1), '')}", text)
    text = re.sub(r"\{\{(?:key|hold|scene|page):([^}]+)\}\}", r"\1", text)
    text = re.sub(r"\[\[#[\w-]+\|([^\]]+)\]\]", r"\1", text)
    text = re.sub(r"\[\[#([\w-]+)\]\]", r"\1", text)
    text = re.sub(r"\[([^\]]+)\]\([^)]+\)", r"\1", text)
    text = re.sub(r"[*`>#|]", " ", text)
    return re.sub(r"\s+", " ", text).strip()


def note_text(doc, text):
    if doc.current() is not None and text:
        doc.current()["text"].append(plain(text))


def screen_picture(shot, marks, seed, alt, first=1, light=False):
    """The screenshot in its bezel, with highlights when it has marks. -> (html, legend items)."""
    svg, items = figures.screen(f"img/{shot}.png", marks, seed, alt, first, light)
    return f'<div class="bezel">{svg}</div>', items


def need_shot(shot, doc):
    if shot not in doc.leds:
        sys.exit(f"learn: [{doc.lang}] no screenshot named {shot}")


def shot_figure(arg, body, doc):
    shot, _, caption = arg.partition("|")
    shot, caption = shot.strip(), caption.strip()
    need_shot(shot, doc)
    marks = figures.parse_marks(body)
    for _, _, text in marks:
        note_text(doc, text)
    note_text(doc, caption)
    levels = doc.leds.get(shot)
    seed = f"{doc.lang}:{shot}:{len(doc.sections)}:{len(body)}"
    alt = plain(caption) or shot
    picture, items = screen_picture(shot, marks, seed, alt)
    keys, key_items = keyboard_svg(levels, alt, marks, seed + "k", len(items) + 1) if levels else ("", [])
    label = {"zh": "同一时刻的按键灯", "en": "Key lights at that moment"}[doc.lang]
    keys_block = f'<div class="keys-wrap"><span class="keys-label">{figures.icon("light")}{label}</span>{keys}</div>' if keys else ""
    legend = figures.legend_html(items + key_items, lambda text: inline(text, doc))
    try_label, goto_label, close_label = TRY[doc.lang]
    setup = html.escape(json.dumps(shot_setup(shot)))
    cap = inline(caption, doc) if caption else ""
    # The live device takes the place of the picture in the figure last opened (tools/learn/learn.js).
    return (f'<figure class="shot" data-shot="{shot}" data-setup="{setup}">'
            f'<div class="shot-static">{picture}{keys_block}</div>'
            f'<div class="shot-live" hidden></div>'
            f'{legend}'
            f'{f"<figcaption>{cap}</figcaption>" if cap else ""}'
            f'<div class="shot-bar"><button type="button" class="try">{try_label}</button>'
            f'<button type="button" class="goto" hidden>{goto_label}</button><button type="button" class="untry" hidden>{close_label}</button></div>'
            f'</figure>')


def strip_figure(body, doc):
    """Numbered steps: the keys to press, the screen it leads to (highlighted), what happens."""
    panels = []
    for raw in body:
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        m = re.match(r"^(.*?)\s+>\s+(.*?)\s*\|\s*(.*)$", line)
        if not m:
            sys.exit(f"learn: [{doc.lang}] strip step needs KEYS > PICTURE | caption: {line!r}")
        panels.append(m.groups())
    if not panels:
        sys.exit(f"learn: [{doc.lang}] empty strip")
    cols = 2 if len(panels) in (2, 4) else 1 if len(panels) == 1 else 3
    out = [f'<div class="strip" data-cols="{cols}" style="--cols:{cols}">']
    for number, (keys, picture, caption) in enumerate(panels, start=1):
        note_text(doc, caption)
        do = "" if keys.strip() == "-" else f'<span class="do">{figures.key_sequence(keys, doc.lang)}</span>'
        picture = picture.strip()
        pic = ""
        if picture != "-":
            name, *targets = picture.split()
            need_shot(name, doc)
            marks = [("mark", t.lstrip("@"), "") for t in targets if t.startswith("@")]
            pic, _items = screen_picture(name, marks, f"{doc.lang}:{name}:{len(doc.sections)}:{number}", plain(caption), light=True)
        out.append(f'<div class="panel"><div class="panel-head"><span class="num">{number:02d}</span>{do}</div>'
                   f'{pic}<p class="say">{inline(caption, doc)}</p></div>')
    out.append("</div>")
    return "".join(out)


def keymap_figure(arg, body, doc):
    groups = []
    for raw in body:
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        spec, _, label = line.partition("=")
        groups.append((spec.strip(), label.strip()))
        note_text(doc, label)
    caption = arg.strip()
    note_text(doc, caption)
    svg, items = figures.keymap(groups, plain(caption), f"{doc.lang}:keymap:{caption}")
    legend = figures.legend_html(items, lambda text: inline(text, doc))
    return f'<figure class="keymap">{svg}{legend}{f"<figcaption>{inline(caption, doc)}</figcaption>" if caption else ""}</figure>'


def directive(name, arg, doc, body=()):
    if name == "shot":
        return shot_figure(arg, body, doc)
    if name == "strip":
        return strip_figure(body, doc)
    if name == "keymap":
        return keymap_figure(arg, body, doc)
    if name == "ref":
        kind, _, rest = arg.partition(":")
        if kind == "engine":
            return engine_reference(doc.dump, rest, doc.lang)
        if kind in ("fn", "settings"):
            return fn_reference(doc.dump, doc.lang)
        if kind == "fn-quick":
            return fn_quick_reference(doc.dump, doc.lang)
        if kind == "lists":
            return lists_reference(doc.dump, doc.lang)
        if kind == "pages":
            return pages_reference(doc.dump, doc.lang)
    if name == "map":
        return device_map(doc.lang) if arg == "device" else scene_map(doc.lang)
    raise SystemExit(f"learn: unknown directive {{{{{name}:{arg}}}}}")


def split_cells(row):
    """A table row's cells: | separates them, except inside {{...}} and [[...]]."""
    cells, depth, current = [], 0, ""
    for index, ch in enumerate(row):
        pair = row[index:index + 2]
        if pair in ("{{", "[["):
            depth += 1
        elif pair in ("}}", "]]") and depth:
            depth -= 1
        if ch == "|" and depth == 0:
            cells.append(current)
            current = ""
        else:
            current += ch
    cells.append(current)
    return cells


def convert(markdown, doc, chapter_id):
    lines = markdown.splitlines()
    out = []
    i = 0
    paragraph = []

    def flush():
        if paragraph:
            text = " ".join(paragraph)
            out.append(f"<p>{inline(text, doc)}</p>")
            if doc.current() is not None:
                doc.current()["text"].append(plain(text))
            paragraph.clear()

    while i < len(lines):
        line = lines[i]
        stripped = line.strip()
        if not stripped:
            flush()
            i += 1
            continue
        kw = re.match(r"<!--\s*keywords:(.*)-->", stripped)
        if kw:
            flush()
            if doc.current() is not None:
                doc.current()["keywords"].extend(k.strip() for k in kw.group(1).split(",") if k.strip())
            i += 1
            continue
        heading = re.match(r"^(#{1,3})\s+(.*?)\s*(?:\{#([\w-]+)\})?$", stripped)
        if heading:
            flush()
            level = len(heading.group(1))
            title = heading.group(2)
            sid = heading.group(3) or (chapter_id if level == 1 else slug(title))
            doc.sections.append({"id": sid, "level": level, "title": plain(title), "chapter": chapter_id, "text": [], "keywords": []})
            tag = f"h{level + 1}" if level > 1 else "h1"
            anchor = f'<a class="anchor" href="#{sid}" aria-label="link">#</a>'
            badge = f'<span class="h-ico">{figures.icon(CHAPTER_ICONS.get(chapter_id, "book"))}</span>' if level == 1 else ""
            out.append(f'<{tag} id="{doc.lang}-{sid}" data-id="{sid}">{badge}{inline(title, doc)}{anchor}</{tag}>')
            i += 1
            continue
        if stripped.startswith("```"):
            flush()
            code = []
            i += 1
            while i < len(lines) and not lines[i].strip().startswith("```"):
                code.append(lines[i])
                i += 1
            i += 1
            out.append(f'<pre class="code"><code>{html.escape(chr(10).join(code))}</code></pre>')
            if doc.current() is not None:
                doc.current()["text"].append(plain(" ".join(code)))
            continue
        block = re.match(r"^\{\{(shot|ref|map):([^{}]+)\}\}$", stripped)
        if block:
            flush()
            out.append(directive(block.group(1), block.group(2), doc))
            i += 1
            continue
        opening = re.match(r"^\{\{(shot|strip|keymap)(?::(.*))?$", stripped)
        if opening:
            flush()
            body = []
            i += 1
            while i < len(lines) and lines[i].strip() != "}}":
                body.append(lines[i])
                i += 1
            if i >= len(lines):
                sys.exit(f"learn: [{doc.lang}] {chapter_id}: {{{{{opening.group(1)} is not closed with }}}}")
            i += 1
            out.append(directive(opening.group(1), opening.group(2) or "", doc, body))
            continue
        if stripped.startswith("|"):
            flush()
            rows = []
            while i < len(lines) and lines[i].strip().startswith("|"):
                rows.append([c.strip() for c in split_cells(lines[i].strip().strip("|"))])
                i += 1
            header, body = rows[0], [r for r in rows[2:]]
            head_html = "".join(f"<th>{inline(c, doc)}</th>" for c in header)
            body_html = "".join("<tr>" + "".join(f"<td>{inline(c, doc)}</td>" for c in r) + "</tr>" for r in body)
            out.append(f'<div class="table-wrap"><table><thead><tr>{head_html}</tr></thead><tbody>{body_html}</tbody></table></div>')
            if doc.current() is not None:
                doc.current()["text"].append(plain(" ".join(" ".join(r) for r in rows if not set("".join(r)) <= set("-: "))))
            continue
        if stripped.startswith(">"):
            flush()
            quote = []
            while i < len(lines) and lines[i].strip().startswith(">"):
                quote.append(lines[i].strip()[1:].strip())
                i += 1
            kind = "note"
            if quote and re.match(r"\[!(\w+)\]", quote[0]):
                kind = re.match(r"\[!(\w+)\]", quote[0]).group(1).lower()
                quote[0] = re.sub(r"^\[!\w+\]\s*", "", quote[0])
            text = " ".join(q for q in quote if q)
            glyph = figures.icon(CALLOUT_ICONS.get(kind, "eye"))
            out.append(f'<aside class="callout {kind}"><span class="c-ico">{glyph}</span><p>{inline(text, doc)}</p></aside>')
            if doc.current() is not None:
                doc.current()["text"].append(plain(text))
            continue
        list_match = re.match(r"^(-|\d+\.)\s+(.*)$", stripped)
        if list_match:
            flush()
            ordered = list_match.group(1) != "-"
            items = []
            while i < len(lines):
                m = re.match(r"^(-|\d+\.)\s+(.*)$", lines[i].strip())
                if m and (m.group(1) != "-") == ordered:
                    items.append(m.group(2))
                    i += 1
                elif lines[i].startswith("  ") and lines[i].strip() and items:
                    items[-1] += " " + lines[i].strip()
                    i += 1
                else:
                    break
            tag = "ol" if ordered else "ul"
            out.append(f"<{tag}>" + "".join(f"<li>{inline(item, doc)}</li>" for item in items) + f"</{tag}>")
            if doc.current() is not None:
                doc.current()["text"].append(plain(" ".join(items)))
            continue
        if stripped.startswith("<"):
            flush()
            out.append(line)
            i += 1
            continue
        paragraph.append(stripped)
        i += 1
    flush()
    return "\n".join(out)


def collect_titles(lang, chapters):
    titles = {}
    for chapter_id in chapters:
        text = (LEARN / lang / f"{chapter_id}.md").read_text(encoding="utf-8")
        for m in re.finditer(r"^(#{1,3})\s+(.*?)\s*(?:\{#([\w-]+)\})?$", text, re.M):
            sid = m.group(3) or (chapter_id if len(m.group(1)) == 1 else slug(m.group(2)))
            titles[sid] = plain(m.group(2))
    return titles


# ---------------------------------------------------------------------------------------------

def git_info():
    def run(*args):
        try:
            return subprocess.run(["git", *args], cwd=ROOT, check=True, capture_output=True, text=True).stdout.strip()
        except (OSError, subprocess.CalledProcessError):
            return ""
    commit = run("rev-parse", "--short", "HEAD")
    dirty = bool(run("status", "--porcelain", "--untracked-files=no"))
    return commit + ("+" if dirty and commit else ""), datetime.date.today().isoformat()


def build(out_dir, sim_build, skip_shots):
    manual = json.loads((LEARN / "manual.json").read_text(encoding="utf-8"))
    chapters = manual["chapters"]
    work = ROOT / "build" / "learn"
    work.mkdir(parents=True, exist_ok=True)
    out_dir.mkdir(parents=True, exist_ok=True)
    dump = run_dump(sim_build)
    if skip_shots and (work / "leds.json").exists() and (out_dir / "img").exists():
        leds = json.loads((work / "leds.json").read_text())
    else:
        leds = run_shots(sim_build, out_dir, work)
    commit, date = git_info()
    version = manual["version"]

    contents, tocs, index_sections = {}, {}, []
    all_ids, docs = {}, {}
    for lang in LANGS:
        titles = collect_titles(lang, chapters)
        doc = docs[lang] = Doc(lang, dump, leds, titles)
        articles = []
        for number, chapter_id in enumerate(chapters):
            text = (LEARN / lang / f"{chapter_id}.md").read_text(encoding="utf-8")
            body = convert(text, doc, chapter_id)
            prev_id = chapters[number - 1] if number > 0 else None
            next_id = chapters[number + 1] if number + 1 < len(chapters) else None
            nav = '<nav class="pager">'
            if prev_id:
                nav += f'<a class="prev" href="#{prev_id}">← {html.escape(titles[prev_id])}</a>'
            if next_id:
                nav += f'<a class="next" href="#{next_id}">{html.escape(titles[next_id])} →</a>'
            nav += "</nav>"
            articles.append(f'<article class="chapter" data-chapter="{chapter_id}">{body}{nav}</article>')
        contents[lang] = "\n".join(articles)
        # Table of contents: chapters with their ## sections.
        toc = ['<ol class="toc">']
        open_chapter = False
        for section in doc.sections:
            if section["level"] == 1:
                if open_chapter:
                    toc.append("</ol></li>")
                glyph = figures.icon(CHAPTER_ICONS.get(section["id"], "book"))
                toc.append(f'<li class="toc-chapter"><a href="#{section["id"]}">{glyph}{html.escape(section["title"])}</a><ol>')
                open_chapter = True
            elif section["level"] == 2:
                toc.append(f'<li><a href="#{section["id"]}">{html.escape(section["title"])}</a></li>')
        if open_chapter:
            toc.append("</ol></li>")
        toc.append("</ol>")
        tocs[lang] = "".join(toc)
        all_ids[lang] = [s["id"] for s in doc.sections]
        chapter_titles = {s["id"]: s["title"] for s in doc.sections if s["level"] == 1}
        parent = None
        for section in doc.sections:
            if section["level"] <= 2:
                parent = section["title"]
            index_sections.append({"id": section["id"], "lang": lang, "title": section["title"],
                                   "path": chapter_titles[section["chapter"]] + ("" if section["level"] == 1 else
                                                                               (" › " + parent if section["level"] == 3 and parent != section["title"] else "")),
                                   "level": section["level"], "text": " ".join(section["text"]), "keywords": section["keywords"]})

    map_targets = {href for links in DEVICE_LINKS.values() for href in links}
    map_targets |= {"screen-layout", "keys-grid", "lights-edges", "keys-home", "play-keyboard", "fn-quick", "nav-pages"}
    map_targets |= {section for _, section, items in SCENE_MAP for section in [section] + [sec for _, sec, _ in items]}
    missing_map = sorted(t for t in map_targets if t not in all_ids["zh"])
    if missing_map:
        sys.exit(f"learn: map links to unknown sections: {missing_map}")
    missing_shots = sorted({shot for _, _, items in SCENE_MAP for _, _, shot in items} - set(leds))
    if missing_shots:
        sys.exit(f"learn: map uses unknown screenshots: {missing_shots}")
    if set(all_ids["zh"]) != set(all_ids["en"]):
        missing = sorted(set(all_ids["zh"]) ^ set(all_ids["en"]))
        sys.exit(f"learn: section ids differ between zh and en: {missing}")
    duplicates = [s for s in set(all_ids["zh"]) if all_ids["zh"].count(s) > 1]
    if duplicates:
        sys.exit(f"learn: duplicate section ids: {duplicates}")

    synonyms = json.loads((LEARN / "synonyms.json").read_text(encoding="utf-8"))
    index = {"version": version, "build": commit, "date": date, "sections": index_sections,
             "synonyms": synonyms["groups"], "stopwords": synonyms.get("stopwords", [])}
    (out_dir / "search-index.json").write_text(json.dumps(index, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")

    template = (HERE / "page.html").read_text(encoding="utf-8")
    page = (template.replace("{{ICONS}}", figures.icon_sprite())
            .replace("{{VERSION}}", html.escape(version))
            .replace("{{BUILD}}", html.escape(commit or "dev"))
            .replace("{{DATE}}", date)
            .replace("{{TITLE_ZH}}", html.escape(manual["title"]["zh"]))
            .replace("{{TITLE_EN}}", html.escape(manual["title"]["en"]))
            .replace("{{TOC_ZH}}", tocs["zh"]).replace("{{TOC_EN}}", tocs["en"])
            .replace("{{CONTENT_ZH}}", contents["zh"]).replace("{{CONTENT_EN}}", contents["en"])
            .replace("{{MAP_ZH}}", device_map("zh") + scene_map("zh"))
            .replace("{{MAP_EN}}", device_map("en") + scene_map("en")))
    # The manual is the reference; the site opens on the guide.
    (out_dir / "reference.html").write_text(page, encoding="utf-8")
    lessons = build_guide(out_dir, work, sim_build, skip_shots, docs, all_ids["zh"], leds, version, commit, date)
    for name in ("learn.css", "learn.js", "learn-search.js", "guide.css", "guide.js", "guide-anim.js"):
        shutil.copyfile(HERE / name, out_dir / name)
    # The search backend: a serverless function next to the site (Vercel api/), same core.
    api = out_dir.parent / "api"
    api.mkdir(exist_ok=True)
    shutil.copyfile(HERE / "api-search.js", api / "search.js")
    # Search regression: everyday questions must find their sections (needs node).
    if shutil.which("node"):
        subprocess.run(["node", str(HERE / "search.test.js"), str(out_dir)], check=True)
    print(f"learn: {out_dir} v{version} ({commit or 'dev'}), {len(index_sections) // 2} sections per language, "
          f"{len(leds)} screenshots, {len(synonyms['groups'])} synonym groups, {lessons} guide lessons")


def build_guide(out_dir, work, sim_build, skip_shots, docs, dictionary_ids, leds, version, commit, date):
    """The guide (tools/learn/guide.py) as the site's index.html. Its lessons run on the firmware first."""
    guide = guide_lessons.load()
    img = out_dir / "img"
    shots = sorted(img.glob("guide-*.png")) if img.exists() else []
    if not (skip_shots and shots):
        guide_lessons.run(guide, sim_build, work, img)
    say = lambda text, lang: inline(text, docs[lang])
    rail, cards, lessons, index = guide_lessons.render(guide, say, set(dictionary_ids), leds)
    template = (HERE / "guide.html").read_text(encoding="utf-8")
    page = (template.replace("{{ICONS}}", figures.icon_sprite())
            .replace("{{VERSION}}", html.escape(version))
            .replace("{{BUILD}}", html.escape(commit or "dev"))
            .replace("{{DATE}}", date)
            .replace("{{DICTIONARY_IDS}}", json.dumps(sorted(dictionary_ids)))
            .replace("{{RAIL}}", rail).replace("{{CARDS}}", cards).replace("{{LESSONS}}", lessons)
            .replace("{{GUIDE_INDEX}}", json.dumps(index, ensure_ascii=False)))
    (out_dir / "index.html").write_text(page, encoding="utf-8")
    return len(index["order"])


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", default=str(ROOT / "build" / "web" / "site" / "learn"))
    parser.add_argument("--host-build", default=os.environ.get("LEARN_HOST_BUILD", str(ROOT / "build" / "linux-groove")))
    parser.add_argument("--skip-shots", action="store_true")
    args = parser.parse_args()
    build(pathlib.Path(args.out).resolve(), pathlib.Path(args.host_build).resolve(), args.skip_shots)


if __name__ == "__main__":
    main()
