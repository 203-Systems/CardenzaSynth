// Pictures in the product's look: the screenshot in its bezel with numbered highlights, the board's
// keys with their lights at that moment (./pcb.tsx), step strips and key maps, each with a numbered legend.
import React, {useId} from 'react';
import leds from '@site/generated/data/leds.json';
import {Key} from './chips';
import {Icon, marker, nodeText, parts, say, useLang, useUrls} from './common';
import {targetRect} from './keys';
import {DeviceTop, usePcbKeys} from './pcb';

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

/** The screenshot; marked areas stay bright, the rest dims; labelled marks get numbers. svg is it on
 *  its own; inner is what it draws in the screen's 240 x 135 pixels (the device puts it in its display). */
export function useScreen({shot, marks, first = 1, light = false, alt}: {
  shot: string; marks: {at: string; label?: React.ReactNode}[]; first?: number; light?: boolean; alt: string;
}): {svg: React.ReactElement; inner: React.ReactElement; items: Item[]} {
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
  const inner = (
    <>
      <image className="cz-screen-shot" href={urls.shot(shot)} x="0" y="0" width="240" height="135" preserveAspectRatio="none" />
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
    </>
  );
  const svg = <svg className="cz-screen" viewBox="0 0 240 135" role="img" aria-label={alt}>{inner}</svg>;
  return {svg, inner, items};
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

/** The device at a moment: its screen (enlarged, in its display) and its key lights, and a numbered
 *  legend of what is marked. */
export function Shot({name, children}: {name: string; children?: React.ReactNode}) {
  const lang = useLang();
  const levels = needShot(name);
  const cap = caption(children);
  const alt = nodeText(cap) || name;
  const screen = useScreen({
    shot: name, alt,
    marks: parts<{at: string; children?: React.ReactNode}>(children, 'mark').map((m) => ({at: m.at, label: m.children})),
  });
  const keys = usePcbKeys({
    levels, label: alt, first: screen.items.length + 1,
    marks: parts<{keys: string; children?: React.ReactNode}>(children, 'keymark').map((m) => ({keys: m.keys, label: m.children})),
  });
  return (
    <figure className="cz-figure cz-shot" data-shot={name}>
      <span className="cz-keys-label" data-nosearch=""><Icon name="light" />{say(lang, '同一时刻的屏幕和按键灯', 'The screen and the key lights at that moment')}</span>
      <div className="cz-device">
        <DeviceTop screen={screen.inner} label={alt} dim />
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
  const keys = usePcbKeys({
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

