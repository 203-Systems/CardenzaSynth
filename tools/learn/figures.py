"""Pictures for the Learn manual, drawn in the product's own look (the web simulator and the screen):
key caps as the simulator draws them, highlights on screenshots and keys with numbered markers and a
numbered legend under the picture, icons, and inline chips. Used by tools/learn/build.py.

Marks (one per line inside a {{shot:...}} block):
  mark TARGET = label     highlight an area of the screen; numbered, the label goes in the legend
  key KEYS = label        highlight keys on the key-light diagram under the screenshot
TARGET: header, scene, track, sound, bpm, content, footer, cell:R,C (1-based), cells:R,C-R,C,
strip:ESC|TAB|FN|CTRL|ADD|SUB|OK|PLAY, or X,Y,W,H in screen pixels (240 x 135).
KEYS: ESC TAB FN CTRL ADD SUB OK PLAY, a key's legend (1 Q A Z ; , ...), a keycap print (shift opt
alt del space), G1..G16 or G1-G4 (GRID cells), r3c1 or r0c1-r3c8 (row, column), PADS, GRID, LEFT,
RIGHT, ALL; space separated (each token gets its own frame, one number per line).
"""
import hashlib
import html
import re

# ---------------------------------------------------------------------------------------------
# Icons: 24 x 24 stroke icons in one SVG sprite (<use href="#i-name">).

ICONS = {
    "tap": '<rect x="4" y="13" width="16" height="8" rx="2"/><path d="M12 2v8M8.5 6.5L12 10l3.5-3.5"/>',
    "hold": '<rect x="4" y="13" width="16" height="8" rx="2"/><circle cx="12" cy="6.5" r="4.5"/><path d="M12 4.2v2.5l1.6 1"/>',
    "chord": '<rect x="2" y="12" width="9" height="8" rx="2"/><rect x="13" y="12" width="9" height="8" rx="2"/><path d="M12 3v6M9 6h6"/>',
    "instr": '<rect x="2" y="5" width="20" height="14" rx="2"/><path d="M7 5v8M12 5v8M17 5v8"/>',
    "seq": '<rect x="3" y="3" width="7" height="7" rx="1"/><rect x="14" y="3" width="7" height="7" rx="1" fill="currentColor"/><rect x="3" y="14" width="7" height="7" rx="1" fill="currentColor"/><rect x="14" y="14" width="7" height="7" rx="1"/>',
    "tape": '<rect x="2" y="5" width="20" height="14" rx="2"/><circle cx="8" cy="12" r="2.5"/><circle cx="16" cy="12" r="2.5"/><path d="M10.5 12h3M6 19l2-3h8l2 3"/>',
    "track": '<path d="M12 3l9 5-9 5-9-5 9-5z"/><path d="M3 13l9 5 9-5"/>',
    "page": '<rect x="3" y="8" width="18" height="12" rx="2"/><path d="M3 8l2-3h5l2 3"/>',
    "fn": '<path d="M13 2L4 14h7l-1 8 9-12h-7z"/>',
    "map": '<path d="M9 4L3 6v14l6-2 6 2 6-2V4l-6 2-6-2z"/><path d="M9 4v14M15 6v14"/>',
    "search": '<circle cx="11" cy="11" r="7"/><path d="M21 21l-5-5"/>',
    "play": '<path d="M7 4l13 8-13 8z"/>',
    "stop": '<rect x="5" y="5" width="14" height="14" rx="2"/>',
    "rec": '<circle cx="12" cy="12" r="7" fill="currentColor"/>',
    "save": '<path d="M5 3h11l3 3v15H5z"/><path d="M8 3v5h7V3M8 21v-7h8v7"/>',
    "tip": '<path d="M9 18h6M10 21h4"/><path d="M12 3a6 6 0 0 0-4 10.5c.7.7 1 1.5 1 2.5h6c0-1 .3-1.8 1-2.5A6 6 0 0 0 12 3z"/>',
    "warn": '<path d="M12 3l10 18H2z"/><path d="M12 10v5M12 18v.5"/>',
    "note": '<path d="M9 18V5l11-2v13"/><circle cx="6" cy="18" r="3"/><circle cx="17" cy="16" r="3"/>',
    "capture": '<path d="M3 12a9 9 0 1 0 3-6.7"/><path d="M3 4v5h5"/><path d="M12 7v5l3 2"/>',
    "mix": '<path d="M6 3v18M12 3v18M18 3v18"/><rect x="4" y="13" width="4" height="3" fill="currentColor"/><rect x="10" y="7" width="4" height="3" fill="currentColor"/><rect x="16" y="15" width="4" height="3" fill="currentColor"/>',
    "fx": '<path d="M12 3v4M12 17v4M3 12h4M17 12h4M6 6l2.5 2.5M15.5 15.5L18 18M6 18l2.5-2.5M15.5 8.5L18 6"/>',
    "sample": '<path d="M2 12h3l2-6 3 12 3-9 2 6 2-3h5"/>',
    "folder": '<path d="M3 6h6l2 2h10v11H3z"/>',
    "usb": '<path d="M12 2v14"/><path d="M8.5 5.5L12 2l3.5 3.5"/><circle cx="12" cy="19" r="2.5"/><path d="M12 12l-5-3V7M12 14l5-3V9"/>',
    "web": '<circle cx="12" cy="12" r="9"/><path d="M3 12h18M12 3c3 3 3 15 0 18M12 3c-3 3-3 15 0 18"/>',
    "light": '<circle cx="12" cy="12" r="4"/><path d="M12 2v3M12 19v3M2 12h3M19 12h3M5 5l2 2M17 17l2 2M5 19l2-2M17 7l2-2"/>',
    "arp": '<path d="M3 20h5v-5h5v-5h5V5h3"/>',
    "keyboard": '<rect x="2" y="6" width="20" height="12" rx="2"/><path d="M6 10h.01M10 10h.01M14 10h.01M18 10h.01M7 14h10"/>',
    "sound": '<path d="M4 9h4l5-4v14l-5-4H4z"/><path d="M16 9a4 4 0 0 1 0 6M19 6a8 8 0 0 1 0 12"/>',
    "lock": '<rect x="5" y="11" width="14" height="10" rx="2"/><path d="M8 11V7a4 4 0 0 1 8 0v4"/>',
    "book": '<path d="M4 4h7a3 3 0 0 1 3 3v14a2 2 0 0 0-2-2H4z"/><path d="M20 4h-4a2 2 0 0 0-2 2"/><path d="M20 4v15h-6"/>',
    "faq": '<circle cx="12" cy="12" r="9"/><path d="M9.5 9a2.5 2.5 0 1 1 3.5 2.3c-.6.3-1 .8-1 1.4V14M12 17v.5"/>',
    "list": '<path d="M8 6h13M8 12h13M8 18h13M3 6h.01M3 12h.01M3 18h.01"/>',
    "tag": '<path d="M3 3h8l10 10-8 8L3 11z"/><circle cx="7.5" cy="7.5" r="1.5"/>',
    "home": '<path d="M3 11l9-8 9 8"/><path d="M5 10v10h14V10"/>',
    "step": '<rect x="3" y="7" width="5" height="10" rx="1"/><rect x="10" y="7" width="5" height="10" rx="1" fill="currentColor"/><rect x="17" y="7" width="4" height="10" rx="1"/><path d="M12.5 3v2M12.5 19v2"/>',
    "roll": '<path d="M3 4v16h18"/><rect x="6" y="6" width="5" height="3" fill="currentColor"/><rect x="11" y="11" width="7" height="3" fill="currentColor"/><rect x="8" y="15" width="4" height="3" fill="currentColor"/>',
    "arrow": '<path d="M4 12h15M14 6l6 6-6 6"/>',
    "eye": '<path d="M2 12s4-7 10-7 10 7 10 7-4 7-10 7S2 12 2 12z"/><circle cx="12" cy="12" r="3"/>',
}


def icon_sprite():
    symbols = "".join(f'<symbol id="i-{name}" viewBox="0 0 24 24">{body}</symbol>' for name, body in ICONS.items())
    return f'<svg xmlns="http://www.w3.org/2000/svg" style="display:none">{symbols}</svg>'


def icon(name, cls=""):
    if name not in ICONS:
        raise SystemExit(f"learn: unknown icon {name}")
    return f'<svg class="ico{(" " + cls) if cls else ""}" aria-hidden="true"><use href="#i-{name}"/></svg>'


# ---------------------------------------------------------------------------------------------
# Keys, as the web simulator draws them (Devices/CardenzaWeb/web/cardenza-web.js, which mirrors
# GrooveTypes.h and the device's keycap prints).

LEFT_ROLES = ["ESC", "TAB", "FN", "CTRL"]
RIGHT_ROLES = ["ADD", "SUB", "OK", "PLAY"]
RIGHT_SYMBOLS = ["←", "↓", "↵", "▶"]  # as on the keycaps: ADD is the backspace key
LEGEND = [
    ["`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", ""],
    ["", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]", "\\"],
    ["", "", "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", ""],
    ["", "", "", "Z", "X", "C", "V", "B", "N", "M", ",", ".", "/", " "],
]
CAP_PRINT = {(0, 13): "del", (2, 1): "shift", (3, 1): "opt", (3, 2): "alt", (3, 13): "space"}


def key_role(row, column):
    if column == 0:
        return LEFT_ROLES[row].lower()
    if column == 13:
        return "play" if row == 3 else "right"
    return "grid" if column >= 9 else "pad"


def keys_for(token):
    """Key positions a token names (see the module doc)."""
    t = token.strip().partition("~")[0].strip()
    up = t.upper()
    m = re.fullmatch(r"R(\d)C(\d+)(?:-R(\d)C(\d+))?", up)
    if m:
        r0, c0 = int(m.group(1)), int(m.group(2))
        r1, c1 = (int(m.group(3)), int(m.group(4))) if m.group(3) else (r0, c0)
        if max(r0, r1) < 4 and max(c0, c1) < 14:
            return [(r, c) for r in range(min(r0, r1), max(r0, r1) + 1) for c in range(min(c0, c1), max(c0, c1) + 1)]
    if up == "ALL":
        return [(r, c) for r in range(4) for c in range(14)]
    if up == "PADS":
        return [(r, c) for r in range(4) for c in range(1, 9)]
    if up == "GRID":
        return [(r, c) for r in range(4) for c in range(9, 13)]
    if up == "LEFT":
        return [(r, 0) for r in range(4)]
    if up == "RIGHT":
        return [(r, 13) for r in range(4)]
    m = re.fullmatch(r"G(\d+)(?:-G?(\d+))?", up)
    if m:
        first, last = int(m.group(1)), int(m.group(2) or m.group(1))
        return [((n - 1) // 4, 9 + (n - 1) % 4) for n in range(first, last + 1) if 1 <= n <= 16]
    if up in LEFT_ROLES:
        return [(LEFT_ROLES.index(up), 0)]
    if up in ("ENTER",):
        up = "OK"
    if up in RIGHT_ROLES:
        return [(RIGHT_ROLES.index(up), 13)]
    for (r, c), cap in CAP_PRINT.items():
        if cap.upper() == up:
            return [(r, c)]
    for r, row in enumerate(LEGEND):
        for c, legend in enumerate(row):
            if legend.strip() and legend == up and 0 < c < 13:
                return [(r, c)]
    raise SystemExit(f"learn: unknown key {token!r}")


def cap_text(row, column):
    """(legend, small tag) the simulator prints on a key."""
    if column == 0:
        return LEFT_ROLES[row], LEGEND[row][0]
    if column == 13:
        return RIGHT_SYMBOLS[row], RIGHT_ROLES[row]
    legend = LEGEND[row][column]
    return (legend, "") if legend.strip() else (CAP_PRINT.get((row, column), ""), "")


HOLD_WORD = {"zh": "按住", "en": "HOLD"}


def chip(token, hold=False, lang="en"):
    """One key cap as an inline chip, styled like the simulator's key; a held key says so."""
    name, _, custom = token.strip().partition("~")
    keys = keys_for(name)
    up = name.strip().upper()
    if len(keys) == 1:
        row, column = keys[0]
        role = key_role(row, column)
        legend, tag = cap_text(row, column)
        if column == 0:
            tag = ""
        if custom.strip():
            legend, tag = custom.strip(), ("" if column != 13 else tag)
        elif not legend.strip():
            legend = "PAD"
    else:
        role = "grid" if all(c >= 9 and c <= 12 for _, c in keys) else "pad" if all(1 <= c <= 8 for _, c in keys) else "right" if up == "RIGHT" else "pad"
        legend, tag = (custom.strip() or {"PADS": "PADS", "GRID": "GRID", "LEFT": "LEFT", "RIGHT": "RIGHT"}.get(up, up)), ""
        m = re.fullmatch(r"G(\d+)-G?(\d+)", up)
        if m and not custom.strip():
            legend = f"G{m.group(1)}–G{m.group(2)}"
    if re.fullmatch(r"G\d+", up) and not custom.strip():
        tag = up  # the GRID cell number next to the key's own legend
    inner = (f'<i class="held">{HOLD_WORD[lang]}</i>' if hold else "") + f"<b>{html.escape(legend)}</b>" + (f"<small>{html.escape(tag)}</small>" if tag else "")
    return f'<span class="kc k-{role}{" hold" if hold else ""}">{inner}</span>'


def column_is_right(keys):
    return len(keys) == 1 and keys[0][1] == 13


def chord(spec, lang="en"):
    """"FN+6" -> keys joined by +. "hold ESC + G3" -> a held ESC, then G3."""
    parts = [p.strip() for p in spec.strip().split("+") if p.strip()]
    out = []
    for part in parts:
        hold = part.lower().startswith("hold ")
        out.append(chip(part[5:].strip() if hold else part, hold, lang))
    return '<span class="combo">' + '<span class="plus">+</span>'.join(out) + "</span>"


def key_sequence(spec, lang="en"):
    """"TAB, TAB" or "hold ESC + G3": commas are presses one after another."""
    if spec.strip() in (",", "FN+,"):
        return chord(spec, lang)  # the comma key itself
    steps = [s for s in spec.split(",") if s.strip()]
    return '<span class="then">›</span>'.join(chord(step, lang) for step in steps)


# ---------------------------------------------------------------------------------------------
# Marks: parsing, the numbered legend.

def parse_marks(lines):
    """[(kind, target, label)] with kind 'mark' (screen) or 'key'."""
    out = []
    for raw in lines:
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        m = re.match(r"(mark|key)\s+(.+?)\s*(?:=\s*(.*))?$", line)
        if not m:
            raise SystemExit(f"learn: cannot read mark {line!r} (mark TARGET = label, key KEYS = label)")
        out.append((m.group(1), m.group(2), (m.group(3) or "").strip()))
    return out


def legend_html(items, inline):
    """items: [(number, label)] -> the numbered list under a picture."""
    if not items:
        return ""
    rows = "".join(f'<li><span class="mk">{n}</span><span class="mk-text">{inline(label)}</span></li>' for n, label in items)
    return f'<ol class="marks">{rows}</ol>'


def _uid(seed):
    return "m" + hashlib.sha1(seed.encode()).hexdigest()[:10]


def _badge(x, y, number, size, limit_w, limit_h, low=0):
    text = str(number)
    w = size if len(text) == 1 else size * 1.45
    x = min(max(x, low), limit_w - w)
    y = min(max(y, low), limit_h - size)
    return (f'<g class="badge"><rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{size}"/>'
            f'<text x="{x + w / 2:.1f}" y="{y + size * 0.78:.1f}" font-size="{size * 0.82:.1f}">{text}</text></g>')


# ---------------------------------------------------------------------------------------------
# Screen targets (screen pixels, 240 x 135).

def cell_rect(row, column):
    return (11 + (column - 1) * 55, 15 + (row - 1) * 27, 52, 24)


NAMED = {
    "header": (9, 0, 221, 11), "scene": (9, 0, 28, 11), "track": (40, 0, 18, 11), "sound": (59, 0, 64, 11),
    "bpm": (190, 0, 40, 11), "content": (9, 13, 222, 110), "footer": (9, 123, 221, 12), "tabs": (9, 123, 221, 12),
}
STRIPS = {"ESC": 0, "TAB": 1, "FN": 2, "CTRL": 3, "ADD": 0, "SUB": 1, "OK": 2, "PLAY": 3}


def target_rect(spec):
    spec = spec.strip()
    if spec in NAMED:
        return NAMED[spec]
    m = re.fullmatch(r"cell:(\d),(\d)", spec)
    if m:
        return cell_rect(int(m.group(1)), int(m.group(2)))
    m = re.fullmatch(r"cells:(\d),(\d)-(\d),(\d)", spec)
    if m:
        a = cell_rect(int(m.group(1)), int(m.group(2)))
        b = cell_rect(int(m.group(3)), int(m.group(4)))
        return (a[0], a[1], b[0] + b[2] - a[0], b[1] + b[3] - a[1])
    m = re.fullmatch(r"strip:(\w+)", spec)
    if m and m.group(1).upper() in STRIPS:
        name = m.group(1).upper()
        x = 0 if name in ("ESC", "TAB", "FN", "CTRL") else 232
        return (x, STRIPS[name] * 34, 8, 31)
    m = re.fullmatch(r"(-?\d+),(-?\d+),(\d+),(\d+)", spec)
    if m:
        return tuple(int(v) for v in m.groups())
    raise SystemExit(f"learn: unknown screen target {spec!r}")


def screen(image, marks, seed, alt, first=1, light=False):
    """The screenshot with its marks: the marked areas stay bright, the rest dims, each mark gets the
    device's dashed focus frame and (when it has a label) a numbered block. -> (svg, legend items)."""
    rects, frames, badges, items = [], [], [], []
    number = first
    for kind, target, label in marks:
        if kind != "mark":
            continue
        x, y, w, h = target_rect(target)
        x, y, w, h = x - 1.5, y - 1.5, w + 3, h + 3
        rects.append(f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" rx="1"/>')
        frames.append(f'<rect class="hl" x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}"/>')
        if label:
            badges.append(_badge(x - 3, y - 3, number, 9, 240, 135))
            items.append((number, label))
            number += 1
    scrim = ""
    if rects:
        mask = _uid(seed)
        scrim = (f'<mask id="{mask}"><rect x="0" y="0" width="240" height="135" fill="#fff"/><g fill="#000">{"".join(rects)}</g></mask>'
                 f'<rect class="scrim{" light" if light else ""}" x="0" y="0" width="240" height="135" mask="url(#{mask})"/>')
    svg = (f'<svg class="screen-svg" viewBox="0 0 240 135" role="img" aria-label="{html.escape(alt)}">'
           f'<image href="{image}" x="0" y="0" width="240" height="135" preserveAspectRatio="none"/>'
           f'{scrim}{"".join(frames)}{"".join(badges)}</svg>')
    return svg, items


# ---------------------------------------------------------------------------------------------
# The keyboard: 4 x 14 keys as the simulator draws them, lit by their LEDs, with highlighted keys.

KEY_W, KEY_H, GAP = 40, 36, 4


def key_geometry(row, column):
    x = column * (KEY_W + GAP) + GAP * ((column >= 1) + (column >= 9) + (column >= 13))
    return x, row * (KEY_H + GAP), KEY_W, KEY_H


KB_W = 13 * (KEY_W + GAP) + 3 * GAP + KEY_W
KB_H = 4 * KEY_H + 3 * GAP


def _frame_tokens(spec):
    """Each token of a KEYS spec -> its keys' bounding box."""
    boxes = []
    for token in spec.split():
        keys = keys_for(token)
        xs, ys = [], []
        for (r, c) in keys:
            x, y, w, h = key_geometry(r, c)
            xs += [x, x + w]
            ys += [y, y + h]
        boxes.append((min(xs), min(ys), max(xs) - min(xs), max(ys) - min(ys), keys))
    return boxes


def keyboard(levels, light, marks=(), seed="kbd", label="", first=1, focus_only=False):
    """levels: 4 x 14 LED levels (or None: unlit); light(level) -> 0..1. marks: [(kind, KEYS, label)],
    kind 'key'. focus_only dims the keys no mark names (key maps). -> (svg, legend items)."""
    pad = 8
    marked = set()
    frames, badges, items = [], [], []
    number = first
    for kind, spec, text in marks:
        if kind != "key":
            continue
        boxes = _frame_tokens(spec)
        for index, (x, y, w, h, keys) in enumerate(boxes):
            marked.update(keys)
            frames.append(f'<rect class="hl" x="{x - 2.5}" y="{y - 2.5}" width="{w + 5}" height="{h + 5}" rx="7"/>')
            if index == 0 and text:
                badges.append(_badge(x + w - 8, y - 9, number, 15, KB_W + pad, KB_H + pad, -pad))
        if text:
            items.append((number, text))
            number += 1
    parts = [f'<svg class="keys-svg" viewBox="{-pad} {-pad} {KB_W + 2 * pad} {KB_H + 2 * pad}" role="img" '
             f'aria-label="{html.escape(label)}"><rect class="kb-body" x="{-pad}" y="{-pad}" width="{KB_W + 2 * pad}" '
             f'height="{KB_H + 2 * pad}" rx="12"/>']
    for row in range(4):
        for column in range(14):
            x, y, w, h = key_geometry(row, column)
            role = key_role(row, column)
            legend, tag = cap_text(row, column)
            level = levels[row][column] if levels else 0
            value = light(level) if levels else 0
            glow = round(value ** 0.45, 3) if value else 0
            dim = focus_only and (row, column) not in marked
            cls = f"k {role}{' dim' if dim else ''}{' lit' if glow > 0.55 else ''}"
            small = " small" if 0 < column < 13 and not LEGEND[row][column].strip() else ""
            parts.append(
                f'<g class="{cls}" data-k="{row * 14 + column}" style="--b:{glow}">'
                f'<rect class="glow" x="{x - 3}" y="{y - 3}" width="{w + 6}" height="{h + 6}" rx="9"/>'
                f'<rect class="cap" x="{x}" y="{y}" width="{w}" height="{h}" rx="6"/>'
                f'<rect class="lamp" x="{x}" y="{y}" width="{w}" height="{h}" rx="6"/>'
                f'<text class="lg{small}" x="{x + 5}" y="{y + 13}">{html.escape(legend)}</text>'
                + (f'<text class="tg" x="{x + w - 4}" y="{y + 10}">{html.escape(tag)}</text>' if tag.strip() else "")
                + "</g>")
    parts += frames + badges
    parts.append("</svg>")
    return "".join(parts), items


def keymap(groups, caption, seed, first=1):
    """groups: [(KEYS, label)] -> (keyboard with those keys highlighted and the rest dimmed, legend items)."""
    marks = [("key", spec, re.sub(r"\s*@(left|right|bottom|top)\b", "", label).strip()) for spec, label in groups]
    return keyboard(None, lambda level: 0, marks, seed, caption, first, focus_only=True)
