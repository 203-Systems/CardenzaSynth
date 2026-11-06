"""The keyboard as the guide's lessons name its keys (docs/learn/guide.json "keys", "done", "demo"):
the tables of the web simulator (Devices/CardenzaWeb/web/cardenza-web.js, which mirrors GrooveTypes.h
and the device's keycap prints). wiki/src/components/cardenza/keys.ts has the same for the pages.

KEYS: ESC TAB FN CTRL ADD SUB OK PLAY, a key's legend (1 Q A Z ; , ...), a keycap print (shift opt
alt del space), G1..G16 or G1-G4 (GRID cells), r3c1 or r0c1-r3c8 (row, column), PADS, GRID, LEFT,
RIGHT, ALL; space separated. A token may end in ~label (the word a picture prints on the key).
"""
import re

LEFT_ROLES = ["ESC", "TAB", "FN", "CTRL"]
RIGHT_ROLES = ["ADD", "SUB", "OK", "PLAY"]
LEGEND = [
    ["`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", ""],
    ["", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]", "\\"],
    ["", "", "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", ""],
    ["", "", "", "Z", "X", "C", "V", "B", "N", "M", ",", ".", "/", " "],
]
CAP_PRINT = {(0, 13): "del", (2, 1): "shift", (3, 1): "opt", (3, 2): "alt", (3, 13): "space"}


def keys_for(token):
    """Key positions (row, column) a token names."""
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
    raise SystemExit(f"keys: unknown key {token!r}")
