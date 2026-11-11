// The board as drawn (static/img/device/cardenza-pcb.svg, 252 x 144 units = 88.9 x 50.8 mm) and its
// key area as the pictures show it: the switches with their legends, each key's LED lit as the device
// lit it, keys framed and numbered, the others faded, keys pressed. Coordinates are the drawing's,
// measured from it: switches KEY1..KEY56 row by row from the top left (KeyIndex = row * 14 + column),
// an LED under each switch, the HOME side key, the display (240 x 135 pixels).
import React, {useId} from 'react';
import useBaseUrl from '@docusaurus/useBaseUrl';
import {keysFor} from './keys';

export const BOARD = {w: 252, h: 144};
export const SWITCH = {x: 4.25, y: 69.09, w: 13.18, h: 9.35, dx: (234.57 - 4.25) / 13, dy: (128.62 - 69.09) / 3};
const LED = {x: 10.84, y: 81.525}; // the centre of KEY1's LED
export const DISPLAY = {x: 68.36, y: 9.49, w: 69.93, h: 39.34};
export const HOME = {x: 51.48, y: 22.28, w: 9.35, h: 13.18};
/** The key area: the legends of the top row down to the LEDs of the bottom row. */
const KEYS_VIEW = {x: 0, y: 62.4, w: BOARD.w, h: BOARD.h - 62.4};
/** The device as one white board: above its keys, its display (the LCD module), magnified so the screen
 *  reads, as wide as the keys: its edges on the outer edges of the first and the last column, so nothing
 *  beside it is empty (on the board it is a quarter of the width). The parts around it are left out, so
 *  the picture is one device and not a magnified top over a plain one. */
const FACE = {left: SWITCH.x, right: SWITCH.x + 13 * SWITCH.dx + SWITCH.w, margin: 6, frame: 3.2};
const FACE_SCREEN_W = FACE.right - FACE.left - 2 * FACE.frame;
const FACE_SCREEN_H = (FACE_SCREEN_W * 135) / 240;
const FACE_TOP = FACE.margin * 2 + FACE.frame * 2 + FACE_SCREEN_H;
/** Under the display, the board from its printed row of labels (203 Systems | Cardenza, ESP32-S3, ...)
 *  down: cut there it has no seam. */
const FACE_KEYS_Y = 55.2;
export type Rect = [number, number, number, number];
type Item = [number, React.ReactNode];

/** A key's cell: its legend above the switch, the switch, its LED under it. */
export function cell(row: number, column: number): Rect {
  return [SWITCH.x + column * SWITCH.dx - 2.1, SWITCH.y + row * SWITCH.dy - 5.7, SWITCH.w + 4.2, 19.6];
}

function ledCentre(row: number, column: number): [number, number] {
  return [LED.x + column * SWITCH.dx, LED.y + row * SWITCH.dy];
}

export function useBoardImage(): string {
  return useBaseUrl('/img/device/cardenza-pcb.svg');
}

function Badge({x, y, n}: {x: number; y: number; n: number}) {
  const size = 6.4;
  const text = String(n);
  const w = text.length === 1 ? size : size * 1.45;
  const bx = Math.min(Math.max(x, KEYS_VIEW.x + 0.4), KEYS_VIEW.x + KEYS_VIEW.w - w - 0.4);
  const by = Math.min(Math.max(y, KEYS_VIEW.y + 0.4), KEYS_VIEW.y + KEYS_VIEW.h - size - 0.4);
  return (
    <g className="cz-pcb-badge">
      <rect x={bx} y={by} width={w} height={size} rx="0.6" />
      <text x={bx + w / 2} y={by + size * 0.79} fontSize={size * 0.8}>{text}</text>
    </g>
  );
}

/**
 * The board's key area. levels: the key lights (4 x 14 perceived levels, Devices/LedLight.h) or null:
 * they glow on the white board. marks: groups of keys
 * (KEYS tokens, tools/wiki/keys.py), each token framed; a labelled group is numbered (on its first
 * frame, or on every frame with badgeAll) and listed. focusOnly fades the keys no mark names.
 * pressed: keys (KeyIndex) down at the moment (the guide's demo, or the keys pressed on a Cardenza).
 */
export function usePcbKeys({levels, marks, first = 1, focusOnly = false, badgeAll = false, pressed, label, screen, silent}: {
  levels: number[][] | null; marks: {keys: string; label?: React.ReactNode}[]; first?: number; focusOnly?: boolean;
  badgeAll?: boolean; pressed?: Set<number>; label: string;
  /** Keys inside a frame that do nothing (a keyboard's gaps): faded like the keys outside it. */
  silent?: Set<number>;
  /** The screen (useScreen's inner, in its 240 x 135 pixels): the whole device, its display above its keys. */
  screen?: React.ReactNode;
}): {svg: React.ReactElement; items: Item[]} {
  const board = useBoardImage();
  const uid = 'p' + useId().replace(/[^a-zA-Z0-9]/g, '');
  const marked = new Set<number>();
  const frames: React.ReactElement[] = [];
  const badges: React.ReactElement[] = [];
  const items: Item[] = [];
  let number = first;
  marks.forEach(({keys, label: text}, mi) => {
    keys.split(/\s+/).filter(Boolean).forEach((token, index) => {
      const xs: number[] = [], ys: number[] = [];
      for (const [r, c] of keysFor(token)) {
        if (!silent?.has(r * 14 + c)) marked.add(r * 14 + c);
        const [x, y, w, h] = cell(r, c);
        xs.push(x, x + w);
        ys.push(y, y + h);
      }
      const x = Math.min(...xs), y = Math.min(...ys), w = Math.max(...xs) - x, h = Math.max(...ys) - y;
      frames.push(
        <g key={`${mi}-${index}`} className="cz-pcb-frame">
          <rect className="under" x={x + 0.3} y={y + 0.3} width={w - 0.6} height={h - 0.6} rx="2.2" />
          <rect className="cz-pcb-hl" x={x + 0.3} y={y + 0.3} width={w - 0.6} height={h - 0.6} rx="2.2" />
        </g>,
      );
      if ((index === 0 || badgeAll) && text) badges.push(<Badge key={`${mi}-${index}`} x={x + w - 4.6} y={y - 2.6} n={number} />);
    });
    if (text) items.push([number++, text]);
  });

  const lights: React.ReactElement[] = [];
  const downs: React.ReactElement[] = [];
  for (let row = 0; row < 4; row++) {
    for (let column = 0; column < 14; column++) {
      const index = row * 14 + column;
      const down = pressed?.has(index);
      // On the white board a key shows how bright it looks (its perceived level), as in a lit room: half
      // lit is a clear glow, full lit a strong one; the board's own LED pads are not a light.
      const value = down ? 1 : levels ? Math.min(levels[row][column], 15) / 15 : 0;
      const glow = value ? Math.round(value * 1000) / 1000 : 0;
      const [cx, cy] = ledCentre(row, column);
      if (glow > 0) {
        // The key lit by its LED: the switch glows with it, the LED shines below.
        const [x, y] = cell(row, column);
        lights.push(
          <g key={index} opacity={glow}>
            <rect className="cz-pcb-lit" x={x + 2.1} y={y + 5.7} width={SWITCH.w} height={SWITCH.h} rx="4.6" />
            <circle cx={cx} cy={cy - 3.5} r="10" fill={`url(#${uid}-glow)`} />
            <rect x={cx - 2.27} y={cy - 1.135} width="4.54" height="2.27" rx="0.4" className="cz-pcb-led" />
          </g>,
        );
      }
      if (down) {
        const [x, y] = cell(row, column);
        downs.push(<rect key={index} className="cz-pcb-down" x={x + 2.1} y={y + 5.7} width={SWITCH.w} height={SWITCH.h} rx="4.6" />);
      }
    }
  }

  const fade = focusOnly && (
    <>
      <mask id={`${uid}-focus`}>
        <rect x="0" y="0" width={BOARD.w} height={BOARD.h} fill="#fff" />
        {[...marked].map((index) => {
          const [x, y, w, h] = cell(Math.floor(index / 14), index % 14);
          return <rect key={index} x={x} y={y} width={w} height={h} rx="2.2" fill="#000" />;
        })}
      </mask>
      <rect className="cz-pcb-fade" x="0" y="0" width={BOARD.w} height={BOARD.h} mask={`url(#${uid}-focus)`} />
    </>
  );
  // The keys on the white board, lit as the device lit them: an LED glows on the board, it does not
  // darken it (the board stays the colour it is, as the device does in a lit room).
  const clipY = screen ? FACE_KEYS_Y : KEYS_VIEW.y;
  const keyArea = (
    <>
      <defs>
        <radialGradient id={`${uid}-glow`}>
          <stop offset="0" stopColor="rgb(255, 200, 70)" stopOpacity="0.95" />
          <stop offset="0.4" stopColor="rgb(255, 205, 90)" stopOpacity="0.5" />
          <stop offset="1" stopColor="rgb(255, 210, 110)" stopOpacity="0" />
        </radialGradient>
        <clipPath id={`${uid}-keys`}>
          <rect x={KEYS_VIEW.x} y={clipY} width={KEYS_VIEW.w} height={BOARD.h - clipY} />
        </clipPath>
      </defs>
      <g clipPath={`url(#${uid}-keys)`}>
        <image href={board} x="0" y="0" width={BOARD.w} height={BOARD.h} preserveAspectRatio="none" />
        {fade}
        {downs}
        {lights}
      </g>
      {frames}
      {badges}
    </>
  );
  if (!screen) {
    const svg = (
      <svg className="cz-pcb" viewBox={`${KEYS_VIEW.x} ${KEYS_VIEW.y} ${KEYS_VIEW.w} ${KEYS_VIEW.h}`} role="img" aria-label={label}>
        {keyArea}
      </svg>
    );
    return {svg, items};
  }
  const sx = FACE.left + FACE.frame;
  const sy = FACE.margin + FACE.frame;
  const shift = FACE_TOP - FACE_KEYS_Y;
  const height = FACE_TOP + BOARD.h - FACE_KEYS_Y;
  const svg = (
    <svg className="cz-pcb cz-pcb-face" viewBox={`0 0 ${BOARD.w} ${height}`} role="img" aria-label={label}>
      <rect className="cz-pcb-board" x="0" y="0" width={BOARD.w} height={height} />
      <rect className="cz-pcb-module" x={sx - FACE.frame} y={sy - FACE.frame} width={FACE_SCREEN_W + FACE.frame * 2}
        height={FACE_SCREEN_H + FACE.frame * 2} rx="2.4" />
      <svg x={sx} y={sy} width={FACE_SCREEN_W} height={FACE_SCREEN_H} viewBox="0 0 240 135" preserveAspectRatio="none" overflow="visible">
        {screen}
      </svg>
      <g transform={`translate(0 ${shift})`}>{keyArea}</g>
    </svg>
  );
  return {svg, items};
}
