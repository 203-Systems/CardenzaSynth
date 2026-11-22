#!/usr/bin/env python3
"""Builds a TouchOSC (Mk2, 1.5+) template for CardenzaSynth's USB MIDI: tools/touchosc/CardenzaSynth.tosc.

One iPad page, send-only. A track selector (T1-T4) picks the MIDI channel (1-4) every control sends on;
every control sends its MIDI from a Lua script (sendMIDI), so the channel follows the selector and
program change, pitch bend and Start / Stop need no fixed message. What each CC does on the device:
docs/midi-implementation-chart.txt.

A .tosc file is zlib-compressed XML (<lexml version='4'>). Run:
  python3 tools/touchosc/make_cardenza_touchosc.py [output.tosc]
"""

import sys
import uuid
import zlib
from pathlib import Path
from xml.sax.saxutils import escape

# The canvas: iPad Air / iPad 10th generation landscape points. TouchOSC scales a layout to the screen.
WIDTH, HEIGHT = 1180, 820

# Colors (r, g, b), close to the device's palette.
INK = (0.91, 0.91, 0.88)
DIM = (0.52, 0.52, 0.49)
PANEL = (0.10, 0.10, 0.09)
TRACKS = [(1.0, 0.36, 0.69), (0.28, 0.60, 1.0), (0.69, 0.93, 0.22), (1.0, 0.64, 0.16)]
MIX = (0.21, 0.84, 0.90)    # TAB cyan
SOUND = (0.63, 0.44, 1.0)   # CTRL violet
PLAY = (0.28, 1.0, 0.47)    # play green
DANGER = (1.0, 0.20, 0.27)  # rec red
FN = (1.0, 0.82, 0.19)      # FN yellow

# Every script asks the selector for the channel (0-3).
CHANNEL_LUA = """local function channel()
  local track = root:findByName('track', true)
  if track == nil then return 0 end
  return math.max(0, math.min(3, math.floor(track.values.x + 0.5)))
end
"""


def prop(kind, key, value):
    if kind == "c":
        r, g, b, a = value
        inner = f"<r>{r}</r><g>{g}</g><b>{b}</b><a>{a}</a>"
    elif kind == "r":
        x, y, w, h = value
        inner = f"<x>{x}</x><y>{y}</y><w>{w}</w><h>{h}</h>"
    elif kind == "s":
        inner = f"<![CDATA[{value}]]>"
    else:
        inner = str(value)
    return f"<property type='{kind}'><key><![CDATA[{key}]]></key><value>{inner}</value></property>"


def value(key, default, locked_current=0):
    return (f"<value><key><![CDATA[{key}]]></key><locked>0</locked><lockedDefaultCurrent>{locked_current}</lockedDefaultCurrent>"
            f"<default><![CDATA[{default}]]></default><defaultPull>0</defaultPull></value>")


def node(kind, props, values, children=""):
    body = "".join(prop(*p) for p in sorted(props, key=lambda p: p[1]))
    kids = f"<children>{children}</children>" if children else ""
    return f"<node ID='{uuid.uuid1()}' type='{kind}'><properties>{body}</properties><values>{''.join(values)}</values>{kids}</node>"


def common(name, frame, color, interactive=1, outline=1, background=1):
    return [("b", "background", background), ("c", "color", (*color, 1)), ("f", "cornerRadius", 1), ("r", "frame", frame),
            ("b", "grabFocus", 1 if interactive else 0), ("b", "interactive", interactive), ("b", "locked", 0), ("s", "name", name),
            ("b", "outline", outline), ("i", "outlineStyle", 1), ("i", "pointerPriority", 0), ("i", "shape", 1), ("b", "visible", 1)]


def text(name, frame, label, color=INK, size=16, align=1, dynamic=False):
    props = common(name, frame, (0, 0, 0), interactive=0, outline=0, background=0) + [
        ("i", "font", 0), ("i", "orientation", 0), ("i", "textAlignH", align), ("i", "textAlignV", 1), ("b", "textClip", 1),
        ("c", "textColor", (*color, 1)), ("i", "textSize", size), ("b", "textWrap", 1)]
    return node("TEXT", props, [value("text", escape(label), 0 if dynamic else 1), value("touch", "false")])


def fader(name, frame, color, script, default=0.0):
    props = common(name, frame, color) + [
        ("b", "bar", 1), ("i", "barDisplay", 0), ("b", "cursor", 1), ("i", "cursorDisplay", 0), ("b", "grid", 0),
        ("c", "gridColor", (0, 0, 0, 0.25)), ("i", "gridSteps", 10), ("i", "orientation", 0), ("i", "response", 0),
        ("i", "responseFactor", 100), ("s", "script", script)]
    return node("FADER", props, [value("x", default), value("touch", "false")])


def button(name, frame, color, script, toggle=False):
    props = common(name, frame, color) + [
        ("i", "buttonType", 1 if toggle else 0), ("i", "orientation", 0), ("b", "press", 1), ("b", "release", 1),
        ("s", "script", script), ("b", "valuePosition", 0)]
    return node("BUTTON", props, [value("x", 0), value("touch", "false")])


def radio(name, frame, color, steps, orientation, script, default=0):
    props = common(name, frame, color) + [
        ("i", "orientation", orientation), ("i", "radioType", 0), ("i", "steps", steps), ("s", "script", script)]
    return node("RADIO", props, [value("x", default), value("touch", "false")])


def group(name, frame, children, color=(0, 0, 0)):
    props = common(name, frame, color, interactive=0, outline=0) + [("i", "orientation", 0)]
    return node("GROUP", props, [value("touch", "false")], children)


# --- Scripts -----------------------------------------------------------------------------------------

def cc_fader_script(cc):
    # 0-1 to 0-127, sent while it moves; the value label above it shows what went out.
    return CHANNEL_LUA + f"""
function onValueChanged(key)
  if key ~= 'x' then return end
  local v = math.floor(self.values.x * 127 + 0.5)
  sendMIDI({{ 0xB0 + channel(), {cc}, v }})
  local label = root:findByName('v_' .. self.name, true)
  if label ~= nil then label.values.text = tostring(v) end
end
"""


PITCH_SCRIPT = CHANNEL_LUA + """
-- 14 bits, 8192 the centre (the device bends +/-2 semitones); back to the centre when let go.
function onValueChanged(key)
  if key == 'x' then
    local v = math.floor(self.values.x * 16383 + 0.5)
    sendMIDI({ 0xE0 + channel(), v % 128, math.floor(v / 128) })
  elseif key == 'touch' and not self.values.touch then
    self.values.x = 0.5
  end
end
"""

# Chorus is a choice on the device (OFF / I / II / I+II): these CC values land on each of the four.
CHORUS_SCRIPT = CHANNEL_LUA + """
local values = { 0, 42, 85, 127 }
function onValueChanged(key)
  if key ~= 'x' then return end
  local index = math.max(0, math.min(3, math.floor(self.values.x + 0.5)))
  sendMIDI({ 0xB0 + channel(), 93, values[index + 1] })
end
"""

SUSTAIN_SCRIPT = CHANNEL_LUA + """
function onValueChanged(key)
  if key ~= 'x' then return end
  sendMIDI({ 0xB0 + channel(), 64, self.values.x > 0.5 and 127 or 0 })
end
"""


def momentary(lua_body):
    return CHANNEL_LUA + f"""
function onValueChanged(key)
  if key ~= 'x' or self.values.x < 0.5 then return end
{lua_body}
end
"""


# Start / Continue / Stop: one-byte system real time.
START_SCRIPT = momentary("  sendMIDI({ 250 })")
CONTINUE_SCRIPT = momentary("  sendMIDI({ 251 })")
STOP_SCRIPT = momentary("  sendMIDI({ 252 })")
RESET_SCRIPT = momentary("  sendMIDI({ 0xB0 + channel(), 121, 0 })")
# Panic: every track's notes off (CC 123), controllers reset (CC 121).
PANIC_SCRIPT = momentary("""  for ch = 0, 3 do
    sendMIDI({ 0xB0 + ch, 123, 0 })
    sendMIDI({ 0xB0 + ch, 121, 0 })
  end""")

# Bank and preset: the numbers live in their labels (bank from 0, preset from 1 as the device's list
# counts). A change sends bank select (CC 0) and the program change (preset - 1): the device loads
# preset bank * 128 + program of the track's engine.
SEND_PROGRAM_LUA = """local function sendProgram()
  local bank = tonumber(root:findByName('bankValue', true).values.text) or 0
  local preset = tonumber(root:findByName('presetValue', true).values.text) or 1
  sendMIDI({ 0xB0 + channel(), 0, bank })
  sendMIDI({ 0xC0 + channel(), preset - 1 })
end
"""


def step_script(label, delta, low, high):
    return CHANNEL_LUA + SEND_PROGRAM_LUA + f"""
function onValueChanged(key)
  if key ~= 'x' or self.values.x < 0.5 then return end
  local label = root:findByName('{label}', true)
  local n = (tonumber(label.values.text) or {low}) + ({delta})
  if n < {low} then n = {high} elseif n > {high} then n = {low} end
  label.values.text = tostring(n)
  sendProgram()
end
"""


SEND_SCRIPT = momentary(SEND_PROGRAM_LUA.replace("local function sendProgram()", "  local function sendProgram()") + "  sendProgram()")

TRACK_SCRIPT = """local names = { 'T1', 'T2', 'T3', 'T4' }
function onValueChanged(key)
  if key ~= 'x' then return end
  local index = math.max(0, math.min(3, math.floor(self.values.x + 0.5)))
  local label = root:findByName('channelLabel', true)
  if label ~= nil then label.values.text = names[index + 1] .. ' ON MIDI CHANNEL ' .. (index + 1) end
end
"""


# --- Layout ------------------------------------------------------------------------------------------

def build():
    kids = []
    kids.append(text("title", (16, 14, 220, 32), "CARDENZA SYNTH", INK, 22, align=0))
    kids.append(text("channelLabel", (16, 50, 220, 24), "T1 ON MIDI CHANNEL 1", DIM, 13, align=0, dynamic=True))

    # Track selector: four segments, a label on each in its track's color.
    tx, tw = 250, 440
    kids.append(radio("track", (tx, 12, tw, 64), (0.35, 0.35, 0.33), 4, 1, TRACK_SCRIPT))
    for i, color in enumerate(TRACKS):
        kids.append(text(f"trackName{i + 1}", (tx + i * tw // 4, 12, tw // 4, 64), f"T{i + 1}", color, 22))

    # Transport and housekeeping.
    for i, (name, label, color, script) in enumerate([
            ("start", "START", PLAY, START_SCRIPT), ("continue", "CONT", PLAY, CONTINUE_SCRIPT), ("stop", "STOP", DANGER, STOP_SCRIPT)]):
        x = 708 + i * 100
        kids.append(button(name, (x, 12, 90, 64), color, script))
        kids.append(text(name + "Label", (x, 12, 90, 64), label, (0, 0, 0), 16))
    kids.append(button("reset", (1010, 12, 154, 30), FN, RESET_SCRIPT))
    kids.append(text("resetLabel", (1010, 12, 154, 30), "RESET CTRL (121)", (0, 0, 0), 12))
    kids.append(button("panic", (1010, 46, 154, 30), DANGER, PANIC_SCRIPT))
    kids.append(text("panicLabel", (1010, 46, 154, 30), "PANIC ALL", (0, 0, 0), 12))

    # Bank / preset / sustain row.
    y, h = 92, 56
    kids.append(text("bankTitle", (16, y, 60, h), "BANK", DIM, 14, align=0))
    kids.append(button("bankDown", (76, y, 56, h), INK, step_script("bankValue", -1, 0, 127)))
    kids.append(text("bankDownLabel", (76, y, 56, h), "-", (0, 0, 0), 24))
    kids.append(text("bankValue", (136, y, 70, h), "0", INK, 24, dynamic=True))
    kids.append(button("bankUp", (210, y, 56, h), INK, step_script("bankValue", 1, 0, 127)))
    kids.append(text("bankUpLabel", (210, y, 56, h), "+", (0, 0, 0), 24))

    kids.append(text("presetTitle", (300, y, 80, h), "PRESET", DIM, 14, align=0))
    kids.append(button("presetDown", (380, y, 56, h), INK, step_script("presetValue", -1, 1, 128)))
    kids.append(text("presetDownLabel", (380, y, 56, h), "-", (0, 0, 0), 24))
    kids.append(text("presetValue", (440, y, 80, h), "1", INK, 24, dynamic=True))
    kids.append(button("presetUp", (524, y, 56, h), INK, step_script("presetValue", 1, 1, 128)))
    kids.append(text("presetUpLabel", (524, y, 56, h), "+", (0, 0, 0), 24))
    kids.append(button("send", (590, y, 90, h), FN, SEND_SCRIPT))
    kids.append(text("sendLabel", (590, y, 90, h), "LOAD", (0, 0, 0), 14))

    kids.append(button("sustain", (708, y, 456, h), PLAY, SUSTAIN_SCRIPT, toggle=True))
    kids.append(text("sustainLabel", (708, y, 456, h), "SUSTAIN (CC 64, TOGGLE)", (0, 0, 0), 16))

    # Fader sections: a title, a value label over each fader, a name under it.
    sections = [
        ("MIX + FX", MIX, [("LEVEL", 7, 0.8), ("EXPR", 11, 1.0), ("REVERB", 91, 0.0), ("DELAY", 94, 0.0), ("DRIVE", 95, 0.0),
                           ("CHORUS", 93, None)]),
        ("SOUND", SOUND, [("CUTOFF", 74, 0.7), ("RESO", 71, 0.2), ("ATTACK", 73, 0.0), ("DECAY", 75, 0.5), ("RELEASE", 72, 0.3),
                          ("VIB RATE", 76, 0.3), ("VIB DEPTH", 77, 0.0)]),
        ("PLAY", PLAY, [("PITCH", "bend", 0.5), ("MOD", 1, 0.0)]),
    ]
    top, title_h, value_h, name_h = 166, 26, 24, 28
    fader_top = top + title_h + value_h
    fader_h = HEIGHT - 14 - name_h - fader_top
    col_w, gap, section_gap = 66, 8, 22
    x = 16
    for title, color, controls in sections:
        width = len(controls) * (col_w + gap) - gap
        kids.append(text(f"section{title}", (x, top, width, title_h), title, color, 14, align=0))
        for label, cc, default in controls:
            name = label.replace(" ", "").lower()
            if default is None:  # the chorus: four modes, not a range
                kids.append(radio("chorus", (x, fader_top, col_w, fader_h), color, 4, 0, CHORUS_SCRIPT))
                for i, mode in enumerate(["OFF", "I", "II", "I+II"]):
                    seg = fader_h // 4
                    kids.append(text(f"chorus{i}", (x, fader_top + fader_h - (i + 1) * seg, col_w, seg), mode, (0.9, 0.9, 0.9), 13))
                kids.append(text("v_chorus", (x, top + title_h, col_w, value_h), "CC 93", DIM, 12))
            elif cc == "bend":
                kids.append(fader("pitch", (x, fader_top, col_w, fader_h), color, PITCH_SCRIPT, 0.5))
                kids.append(text("v_pitch", (x, top + title_h, col_w, value_h), "BEND", DIM, 12))
            else:
                script = cc_fader_script(cc)
                kids.append(fader(name, (x, fader_top, col_w, fader_h), color, script, default))
                kids.append(text(f"v_{name}", (x, top + title_h, col_w, value_h), f"CC {cc}", DIM, 12, dynamic=True))
            kids.append(text(f"n_{name}", (x, fader_top + fader_h + 2, col_w, name_h), label, INK, 12))
            x += col_w + gap
        x += section_gap - gap

    root = group("CardenzaSynth", (0, 0, WIDTH, HEIGHT), "".join(kids), color=(0.04, 0.04, 0.04))
    return "<?xml version='1.0' encoding='UTF-8'?><lexml version='4'>" + root + "</lexml>"


def main():
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).with_name("CardenzaSynth.tosc")
    xml = build()
    out.write_bytes(zlib.compress(xml.encode("utf-8")))
    print(f"wrote {out} ({len(xml)} bytes of XML)")


if __name__ == "__main__":
    main()
