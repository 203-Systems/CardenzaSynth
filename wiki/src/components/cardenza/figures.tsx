// Pictures in the product's look: the screenshot in its bezel with numbered
// highlights, the key lights of that moment, step strips and key maps, each with a numbered legend.
import React, {useId} from 'react';
import leds from '@site/generated/data/leds.json';
import {Key} from './chips';
import {Icon, marker, nodeText, parts, say, useLang, useUrls} from './common';
import {KB_H, KB_W, capText, isPrinted, keyGeometry, keyRole, keysFor, ledLight, targetRect} from './keys';

export type Levels = number[][];
export type Item = [number, React.ReactNode];
const LEDS = leds as Record<string, Levels>;

function needShot(name: string): Levels {
  const levels = LEDS[name];
  if (!levels) throw new Error(`cardenza: no screenshot named ${name}`);
  return levels;
}

function Badge({x, y, n, size, w: limitW, h: limitH, low = 0}: {x: number; y: number; n: number; size: number; w: number; h: number; low?: number}) {
  const text = String(n);
  const w = text.length === 1 ? size : size * 1.45;
  const bx = Math.min(Math.max(x, low), limitW - w);
  const by = Math.min(Math.max(y, low), limitH - size);
  return (
    <g className="cz-badge">
      <rect x={bx} y={by} width={w} height={size} />
      <text x={bx + w / 2} y={by + size * 0.78} fontSize={size * 0.82}>{text}</text>
    </g>
  );
}

/** The screenshot; marked areas stay bright, the rest dims; labelled marks get numbers. */
export function useScreen({shot, marks, first = 1, light = false, alt}: {
  shot: string; marks: {at: string; label?: React.ReactNode}[]; first?: number; light?: boolean; alt: string;
}): {svg: React.ReactElement; items: Item[]} {
  const urls = useUrls();
  const mask = 'm' + useId().replace(/[^a-zA-Z0-9]/g, '');
  const items: Item[] = [];
  let number = first;
  const boxes = marks.map(({at, label}) => {
    const [x0, y0, w0, h0] = targetRect(at);
    const box = {x: x0 - 1.5, y: y0 - 1.5, w: w0 + 3, h: h0 + 3, n: 0};
    if (label) {
      box.n = number;
      items.push([number++, label]);
    }
    return box;
  });
  const svg = (
    <svg className="cz-screen" viewBox="0 0 240 135" role="img" aria-label={alt}>
      <image href={urls.shot(shot)} x="0" y="0" width="240" height="135" preserveAspectRatio="none" />
      {boxes.length > 0 && (
        <>
          <mask id={mask}>
            <rect x="0" y="0" width="240" height="135" fill="#fff" />
            <g fill="#000">{boxes.map((b, i) => <rect key={i} x={b.x} y={b.y} width={b.w} height={b.h} rx="1" />)}</g>
          </mask>
          <rect className={'cz-scrim' + (light ? ' light' : '')} x="0" y="0" width="240" height="135" mask={`url(#${mask})`} />
        </>
      )}
      {boxes.map((b, i) => <rect key={i} className="cz-hl" x={b.x} y={b.y} width={b.w} height={b.h} />)}
      {boxes.filter((b) => b.n).map((b) => <Badge key={b.n} x={b.x - 3} y={b.y - 3} n={b.n} size={9} w={240} h={135} />)}
    </svg>
  );
  return {svg, items};
}

/** 4 x 14 keys lit by their LEDs (or unlit), with framed and numbered keys. focusOnly dims the keys
 *  no mark names; badgeAll numbers every frame of a mark (a key and the steps it goes on); pressed
 *  keys (KeyIndex) go down and light, as on the device (the guide's demos). */
export function keyboard({levels, marks, first = 1, focusOnly = false, badgeAll = false, pressed, label}: {
  levels: Levels | null; marks: {keys: string; label?: React.ReactNode}[]; first?: number; focusOnly?: boolean;
  badgeAll?: boolean; pressed?: Set<number>; label: string;
}): {svg: React.ReactElement; items: Item[]} {
  const pad = 8;
  const marked = new Set<string>();
  const frames: React.ReactElement[] = [];
  const badges: React.ReactElement[] = [];
  const items: Item[] = [];
  let number = first;
  marks.forEach(({keys, label: text}, mi) => {
    keys.split(/\s+/).filter(Boolean).forEach((token, index) => {
      const positions = keysFor(token);
      const xs: number[] = [], ys: number[] = [];
      for (const [r, c] of positions) {
        marked.add(`${r},${c}`);
        const [x, y, w, h] = keyGeometry(r, c);
        xs.push(x, x + w);
        ys.push(y, y + h);
      }
      const x = Math.min(...xs), y = Math.min(...ys), w = Math.max(...xs) - x, h = Math.max(...ys) - y;
      frames.push(<rect key={`${mi}-${index}`} className="cz-hl" x={x - 2.5} y={y - 2.5} width={w + 5} height={h + 5} rx="7" />);
      if ((index === 0 || badgeAll) && text) {
        badges.push(<Badge key={`${mi}-${index}`} x={x + w - 8} y={y - 9} n={number} size={15} w={KB_W + pad} h={KB_H + pad} low={-pad} />);
      }
    });
    if (text) items.push([number++, text]);
  });
  const keys: React.ReactElement[] = [];
  for (let row = 0; row < 4; row++) {
    for (let column = 0; column < 14; column++) {
      const [x, y, w, h] = keyGeometry(row, column);
      const role = keyRole(row, column);
      const [legend, tag] = capText(row, column);
      const value = levels ? ledLight(levels[row][column]) : 0;
      const glow = value ? Math.round(value ** 0.45 * 1000) / 1000 : 0;
      const dim = focusOnly && !marked.has(`${row},${column}`);
      const small = column > 0 && column < 13 && !isPrinted(row, column) ? ' small' : '';
      keys.push(
        <g key={row * 14 + column} data-k={row * 14 + column}
          className={`k ${role}${dim ? ' dim' : ''}${glow > 0.55 ? ' lit' : ''}${pressed?.has(row * 14 + column) ? ' press' : ''}`}
          style={{'--b': glow} as React.CSSProperties}>
          <rect className="glow" x={x - 3} y={y - 3} width={w + 6} height={h + 6} rx="9" />
          <rect className="cap" x={x} y={y} width={w} height={h} rx="6" />
          <rect className="lamp" x={x} y={y} width={w} height={h} rx="6" />
          <text className={'lg' + small} x={x + 5} y={y + 13}>{legend}</text>
          {tag.trim() && <text className="tg" x={x + w - 4} y={y + 10}>{tag}</text>}
        </g>,
      );
    }
  }
  const svg = (
    <svg className="cz-keys" viewBox={`${-pad} ${-pad} ${KB_W + 2 * pad} ${KB_H + 2 * pad}`} role="img" aria-label={label}>
      <rect className="kb-body" x={-pad} y={-pad} width={KB_W + 2 * pad} height={KB_H + 2 * pad} rx="12" />
      {keys}
      {frames}
      {badges}
    </svg>
  );
  return {svg, items};
}

export function Legend({items, rows}: {items: Item[]; rows?: number}) {
  if (!items.length) return null;
  return (
    <ol className={'cz-marks' + (rows ? ' cz-marks-cols' : '')} style={rows ? ({'--rows': rows} as React.CSSProperties) : undefined}>
      {items.map(([n, label]) => (
        <li key={n}><span className="cz-mk" data-nosearch="">{n}</span><span className="cz-mk-text">{label}</span></li>
      ))}
    </ol>
  );
}

// Parts of the figures, written as children: <Shot name=".."><Mark at=".."/>..<Caption>..</Caption></Shot>.
export const Mark = marker<{at: string; children?: React.ReactNode}>('mark');
export const KeyMark = marker<{keys: string; children?: React.ReactNode}>('keymark');
export const Caption = marker<{children?: React.ReactNode}>('caption');
export const Step = marker<{keys?: string; shot?: string; at?: string; children?: React.ReactNode}>('step');
export const Group = marker<{keys: string; children?: React.ReactNode}>('group');

function caption(children: React.ReactNode): React.ReactNode {
  return parts<{children?: React.ReactNode}>(children, 'caption')[0]?.children;
}

/** A screenshot with the key lights of that moment and a numbered legend of what is marked. */
export function Shot({name, children}: {name: string; children?: React.ReactNode}) {
  const lang = useLang();
  const levels = needShot(name);
  const cap = caption(children);
  const alt = nodeText(cap) || name;
  const screen = useScreen({
    shot: name, alt,
    marks: parts<{at: string; children?: React.ReactNode}>(children, 'mark').map((m) => ({at: m.at, label: m.children})),
  });
  const keys = keyboard({
    levels, label: alt, first: screen.items.length + 1,
    marks: parts<{keys: string; children?: React.ReactNode}>(children, 'keymark').map((m) => ({keys: m.keys, label: m.children})),
  });
  return (
    <figure className="cz-figure cz-shot" data-shot={name}>
      <div className="cz-bezel">{screen.svg}</div>
      <div className="cz-keys-wrap">
        <span className="cz-keys-label" data-nosearch=""><Icon name="light" />{say(lang, '同一时刻的按键灯', 'Key lights at that moment')}</span>
        {keys.svg}
      </div>
      <Legend items={[...screen.items, ...keys.items]} />
      {cap && <figcaption>{cap}</figcaption>}
    </figure>
  );
}

function StripScreen({shot, at, alt}: {shot: string; at?: string; alt: string}) {
  needShot(shot);
  const screen = useScreen({shot, light: true, alt, marks: (at || '').split(/\s+/).filter(Boolean).map((t) => ({at: t}))});
  return <div className="cz-bezel">{screen.svg}</div>;
}

function StripPanel({number, step}: {number: number; step: {keys?: string; shot?: string; at?: string; children?: React.ReactNode}}) {
  return (
    <div className="cz-panel">
      <div className="cz-panel-head">
        <span className="cz-num">{String(number).padStart(2, '0')}</span>
        {step.keys && <span className="cz-do"><Key k={step.keys} /></span>}
      </div>
      {step.shot && <StripScreen shot={step.shot} at={step.at} alt={nodeText(step.children)} />}
      <p className="cz-say">{step.children}</p>
    </div>
  );
}

/** Numbered steps: the keys to press, the screen it leads to (highlighted), what happens. */
export function Strip({children}: {children?: React.ReactNode}) {
  const steps = parts<{keys?: string; shot?: string; at?: string; children?: React.ReactNode}>(children, 'step');
  if (!steps.length) throw new Error('cardenza: empty <Strip>');
  const cols = steps.length === 2 || steps.length === 4 ? 2 : steps.length === 1 ? 1 : 3;
  return (
    <div className="cz-strip" data-cols={cols} style={{'--cols': cols} as React.CSSProperties}>
      {steps.map((step, i) => <StripPanel key={i} number={i + 1} step={step} />)}
    </div>
  );
}

/** The keyboard with groups of keys framed and numbered, the rest dimmed. */
export function Keymap({children}: {children?: React.ReactNode}) {
  const cap = caption(children);
  const keys = keyboard({
    levels: null, focusOnly: true, label: nodeText(cap),
    marks: parts<{keys: string; children?: React.ReactNode}>(children, 'group').map((g) => ({keys: g.keys, label: g.children})),
  });
  return (
    <figure className="cz-figure cz-keymap">
      {keys.svg}
      <Legend items={keys.items} />
      {cap && <figcaption>{cap}</figcaption>}
    </figure>
  );
}

