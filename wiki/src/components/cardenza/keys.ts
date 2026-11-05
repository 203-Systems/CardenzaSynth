// The keyboard as the web simulator draws it (Devices/CardenzaWeb/web/cardenza-web.js, which mirrors
// GrooveTypes.h and the device's keycap prints): the same tables as tools/learn/figures.py.

export type Pos = [number, number]; // [row, column], 4 x 14

export const LEFT_ROLES = ['ESC', 'TAB', 'FN', 'CTRL'];
export const RIGHT_ROLES = ['ADD', 'SUB', 'OK', 'PLAY'];
const RIGHT_SYMBOLS = ['↑', '↓', '↵', '▶'];
// The small print of a right key: its role, or the keycap where it says something else (ADD is
// the backspace key).
const RIGHT_PRINT: Record<number, string> = {0: '←'};
export const LEGEND = [
  ['`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', ''],
  ['', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\\'],
  ['', '', 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', "'", ''],
  ['', '', '', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', ' '],
];
const CAP_PRINT: Record<string, string> = {'0,13': 'del', '2,1': 'shift', '3,1': 'opt', '3,2': 'alt', '3,13': 'space'};

export function keyRole(row: number, column: number): string {
  if (column === 0) return LEFT_ROLES[row].toLowerCase();
  if (column === 13) return row === 3 ? 'play' : 'right';
  return column >= 9 ? 'grid' : 'pad';
}

const range = (a: number, b: number) => Array.from({length: b - a + 1}, (_, i) => a + i);
const block = (rows: number[], cols: number[]): Pos[] => rows.flatMap((r) => cols.map((c) => [r, c] as Pos));

/** Key positions a token names: ESC, G1-G4, r3c1, PADS, a legend (1 Q A ;), a print (shift). */
export function keysFor(token: string): Pos[] {
  const t = token.trim().split('~')[0].trim();
  const up = t.toUpperCase();
  let m = /^R(\d)C(\d+)(?:-R(\d)C(\d+))?$/.exec(up);
  if (m) {
    const r0 = +m[1], c0 = +m[2];
    const r1 = m[3] ? +m[3] : r0, c1 = m[4] ? +m[4] : c0;
    if (Math.max(r0, r1) < 4 && Math.max(c0, c1) < 14)
      return block(range(Math.min(r0, r1), Math.max(r0, r1)), range(Math.min(c0, c1), Math.max(c0, c1)));
  }
  if (up === 'ALL') return block(range(0, 3), range(0, 13));
  if (up === 'PADS') return block(range(0, 3), range(1, 8));
  if (up === 'GRID') return block(range(0, 3), range(9, 12));
  if (up === 'LEFT') return range(0, 3).map((r) => [r, 0] as Pos);
  if (up === 'RIGHT') return range(0, 3).map((r) => [r, 13] as Pos);
  m = /^G(\d+)(?:-G?(\d+))?$/.exec(up);
  if (m) {
    const first = +m[1], last = +(m[2] || m[1]);
    return range(first, last).filter((n) => n >= 1 && n <= 16).map((n) => [Math.floor((n - 1) / 4), 9 + ((n - 1) % 4)] as Pos);
  }
  if (LEFT_ROLES.includes(up)) return [[LEFT_ROLES.indexOf(up), 0]];
  const role = up === 'ENTER' ? 'OK' : up;
  if (RIGHT_ROLES.includes(role)) return [[RIGHT_ROLES.indexOf(role), 13]];
  for (const [rc, cap] of Object.entries(CAP_PRINT)) {
    if (cap.toUpperCase() === up) return [rc.split(',').map(Number) as Pos];
  }
  for (let r = 0; r < 4; r++) {
    for (let c = 1; c < 13; c++) {
      if (LEGEND[r][c].trim() && LEGEND[r][c] === up) return [[r, c]];
    }
  }
  throw new Error(`cardenza: unknown key ${JSON.stringify(token)}`);
}

/** [legend, small tag] the simulator prints on a key. */
export function capText(row: number, column: number): [string, string] {
  if (column === 0) return [LEFT_ROLES[row], LEGEND[row][0]];
  if (column === 13) return [RIGHT_SYMBOLS[row], RIGHT_PRINT[row] ?? RIGHT_ROLES[row]];
  const legend = LEGEND[row][column];
  return legend.trim() ? [legend, ''] : [CAP_PRINT[`${row},${column}`] ?? '', ''];
}

export function isPrinted(row: number, column: number): boolean {
  return LEGEND[row][column].trim() !== '';
}

// Key lights (Devices/LedLight.h): the perceived level through the gamma table to CardenzaHAL's
// linear key duty, times the HAL's gamma-2 global PWM.
const KEY_LEVEL_FOR_PERCEIVED = [0, 1, 1, 1, 1, 1, 1, 2, 3, 4, 5, 7, 8, 10, 13, 15];

export function ledLight(level: number, master = 15): number {
  return (KEY_LEVEL_FOR_PERCEIVED[Math.min(level, 15)] / 15) * (Math.min(master, 15) / 15) ** 2;
}

// Geometry of the key diagram.
export const KEY_W = 40, KEY_H = 36, GAP = 4;

export function keyGeometry(row: number, column: number): [number, number, number, number] {
  const x = column * (KEY_W + GAP) + GAP * (+(column >= 1) + +(column >= 9) + +(column >= 13));
  return [x, row * (KEY_H + GAP), KEY_W, KEY_H];
}

export const KB_W = 13 * (KEY_W + GAP) + 3 * GAP + KEY_W;
export const KB_H = 4 * KEY_H + 3 * GAP;

// Screen targets, in screen pixels (240 x 135).
function cellRect(row: number, column: number): [number, number, number, number] {
  return [11 + (column - 1) * 55, 15 + (row - 1) * 27, 52, 24];
}

const NAMED: Record<string, [number, number, number, number]> = {
  header: [9, 0, 221, 11], scene: [9, 0, 28, 11], track: [40, 0, 18, 11], sound: [59, 0, 64, 11],
  bpm: [190, 0, 40, 11], content: [9, 13, 222, 110], footer: [9, 123, 221, 12], tabs: [9, 123, 221, 12],
};
const STRIPS: Record<string, number> = {ESC: 0, TAB: 1, FN: 2, CTRL: 3, ADD: 0, SUB: 1, OK: 2, PLAY: 3};

export function targetRect(spec: string): [number, number, number, number] {
  const s = spec.trim();
  if (NAMED[s]) return NAMED[s];
  let m = /^cell:(\d),(\d)$/.exec(s);
  if (m) return cellRect(+m[1], +m[2]);
  m = /^cells:(\d),(\d)-(\d),(\d)$/.exec(s);
  if (m) {
    const a = cellRect(+m[1], +m[2]);
    const b = cellRect(+m[3], +m[4]);
    return [a[0], a[1], b[0] + b[2] - a[0], b[1] + b[3] - a[1]];
  }
  m = /^strip:(\w+)$/.exec(s);
  if (m && m[1].toUpperCase() in STRIPS) {
    const name = m[1].toUpperCase();
    return [LEFT_ROLES.includes(name) ? 0 : 232, STRIPS[name] * 34, 8, 31];
  }
  m = /^(-?\d+),(-?\d+),(\d+),(\d+)$/.exec(s);
  if (m) return [+m[1], +m[2], +m[3], +m[4]];
  throw new Error(`cardenza: unknown screen target ${JSON.stringify(spec)}`);
}
