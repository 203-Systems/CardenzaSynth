// The Launcher's lessons (docs/learn/guide.json, "anim": "launcher"): the device as the other lessons
// draw it (the board, its screen in its display, the keys framed) showing the 203 Launcher's real
// screens, drawn by the Launcher's own code (tools/wiki/launcher/shots.py -> static/img/launcher/<lang>/).
// Each step plays its frames in a loop: [screen, what the hands do as it shows, seconds].
//   screen: a Launcher screen (home-1, store, ...) in the reader's language ("en:home-9": in English
//           whatever the language), "off" (dark: unplugged, or the chip's download mode),
//           "shot:<name>" (the groovebox's screenshot of that name) or "web:<name>" (a part of the 203
//           community's page, tools/wiki/launcher/community.mjs -> static/img/community/<lang>/, with the
//           button or field the step points at framed). A step's frames are all of the device or all web.
//   action: null; a key pressed as the frame ends ("OK", "/"); "hold:FN" held through the frame;
//           "type:V6QK28" typed key by key through it. HOME (the side key, GPIO0) shows on the whole board.
// A step with "board": true shows the whole board (the HOME key left of the screen); otherwise the keys
// under the screen magnified, as the device lessons do.
import React, {useEffect, useMemo, useState} from 'react';
import {useBaseUrlUtils} from '@docusaurus/useBaseUrl';
import {useLang, useUrls} from '@site/src/components/cardenza/common';
import {keysFor} from '@site/src/components/cardenza/keys';
import {BOARD, DISPLAY, HOME, cell, usePcbKeys, useBoardImage} from '@site/src/components/cardenza/pcb';
import communityShots from './community-shots.json';
import type {Words} from './run';

type WebShot = {w: number; h: number; marks: number[][]};
const WEB = (communityShots as {shots: Record<string, Record<string, WebShot>>}).shots;

export type Frame = [string, string | null, number];
export type LauncherStep = {say: Words; frames: Frame[]; keys?: string[]; board?: boolean};

const PAUSE = 1.4; // the last frame stays this long before the step plays again
const TYPE_EVERY = 0.2;

/** The tokens a frame's action names: the key pressed, held, or typed (letters and digits). */
function actionKeys(action: string | null): string[] {
  if (!action) return [];
  if (action.startsWith('hold:')) return [action.slice(5)];
  if (action.startsWith('type:')) return [...action.slice(5)].map((c) => c.toUpperCase());
  return [action];
}

/** The keys a step frames: its own list, or what its frames press and hold (not what they type). */
export function stepKeys(step: LauncherStep): string[] {
  if (step.keys) return step.keys;
  const out: string[] = [];
  for (const [, action] of step.frames) {
    if (action && !action.startsWith('type:')) for (const key of actionKeys(action)) if (!out.includes(key)) out.push(key);
  }
  return out;
}

/** The frame at t seconds (the step loops) and the keys down at that moment. */
function moment(step: LauncherStep, t: number): {index: number; down: string[]} {
  const total = step.frames.reduce((sum, f) => sum + f[2], 0) + PAUSE;
  let left = t % total;
  let index = 0;
  while (index < step.frames.length - 1 && left >= step.frames[index][2]) {
    left -= step.frames[index][2];
    index++;
  }
  const [, action, seconds] = step.frames[index];
  const down: string[] = [];
  if (index === step.frames.length - 1 && left >= seconds) return {index, down}; // the pause after the last frame
  if (action?.startsWith('hold:')) down.push(action.slice(5));
  else if (action?.startsWith('type:')) {
    const word = action.slice(5);
    const n = Math.floor(left / TYPE_EVERY);
    if (n < word.length && left % TYPE_EVERY < TYPE_EVERY * 0.6) down.push(word[n].toUpperCase());
  } else if (action && left > seconds - 0.35) down.push(action);
  return {index, down};
}

/** The step's moment, from when it was shown, updated only when the frame or the keys down change; it
 *  waits while the page is hidden, and with reduced motion the first frame stays. */
function useMoment(step: LauncherStep): {index: number; down: string[]} {
  // The moment belongs to its step: a step just shown starts at its first frame.
  const [now, setNow] = useState(() => ({step, ...moment(step, 0)}));
  useEffect(() => {
    let last = moment(step, 0);
    setNow({step, ...last});
    if (window.matchMedia('(prefers-reduced-motion: reduce)').matches) return undefined;
    let start = performance.now();
    let hiddenAt = 0;
    let raf = 0;
    const tick = (time: number) => {
      raf = requestAnimationFrame(tick);
      if (document.hidden) {
        hiddenAt = hiddenAt || time;
        return;
      }
      if (hiddenAt) {
        start += time - hiddenAt;
        hiddenAt = 0;
      }
      // A frame's time can be a little before the effect started.
      const next = moment(step, Math.max(0, time - start) / 1000);
      if (next.index !== last.index || next.down.join() !== last.down.join()) {
        last = next;
        setNow({step, ...next});
      }
    };
    raf = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(raf);
  }, [step]);
  return now.step === step ? now : moment(step, 0);
}

function matrixIndices(tokens: string[]): Set<number> {
  const out = new Set<number>();
  for (const token of tokens) if (token !== 'HOME') for (const [r, c] of keysFor(token)) out.add(r * 14 + c);
  return out;
}

/** The step's screens, all drawn once and shown one at a time (no reload between frames). */
function useScreens(step: LauncherStep, shown: string): React.ReactElement {
  const lang = useLang();
  const urls = useUrls();
  const {withBaseUrl} = useBaseUrlUtils();
  const names = useMemo(() => [...new Set(step.frames.map((f) => f[0]))], [step]);
  // "en:home-9": that screen as the English Launcher shows it, whatever the reader's language.
  const screenUrl = (name: string) => {
    if (name.startsWith('shot:')) return urls.shot(name.slice(5));
    const [, fixed, base] = /^(?:(zh|en):)?(.*)$/.exec(name)!;
    return withBaseUrl(`/img/launcher/${fixed || lang}/${base}.png`);
  };
  return (
    <>
      <rect x="0" y="0" width="240" height="135" fill="#000" />
      {names.filter((name) => name !== 'off').map((name) => (
        <image key={name} className="cz-screen-shot" x="0" y="0" width="240" height="135" preserveAspectRatio="none"
          href={screenUrl(name)}
          opacity={name === shown ? 1 : 0} />
      ))}
    </>
  );
}

/** The whole board (HOME left of the screen), the screen in its display, keys and HOME framed. */
function Board({screen, framed, down, label}: {screen: React.ReactElement; framed: string[]; down: string[]; label: string}) {
  const board = useBoardImage();
  const rects: [number, number, number, number][] = [];
  const frame = (token: string): [number, number, number, number][] =>
    token === 'HOME' ? [[HOME.x - 1, HOME.y - 1, HOME.w + 2, HOME.h + 2]] : keysFor(token).map(([r, c]) => cell(r, c));
  framed.forEach((token) => rects.push(...frame(token)));
  const pressed: [number, number, number, number][] = [];
  down.forEach((token) => pressed.push(...frame(token)));
  return (
    <svg className="cz-pcb" viewBox={`0 0 ${BOARD.w} ${BOARD.h}`} role="img" aria-label={label}>
      <image href={board} x="0" y="0" width={BOARD.w} height={BOARD.h} preserveAspectRatio="none" />
      <svg x={DISPLAY.x} y={DISPLAY.y} width={DISPLAY.w} height={DISPLAY.h} viewBox="0 0 240 135" preserveAspectRatio="none">
        {screen}
      </svg>
      {pressed.map(([x, y, w, h], i) => (
        <rect key={`d${i}`} className="cz-pcb-down" x={x + 1} y={y + 1} width={w - 2} height={h - 2} rx="2.2" />
      ))}
      {rects.map(([x, y, w, h], i) => (
        <g key={i} className="cz-pcb-frame">
          <rect className="under" x={x + 0.3} y={y + 0.3} width={w - 0.6} height={h - 0.6} rx="2.2" />
          <rect className="cz-pcb-hl" x={x + 0.3} y={y + 0.3} width={w - 0.6} height={h - 0.6} rx="2.2" />
        </g>
      ))}
    </svg>
  );
}

/** A web step: the community's page as the reader's language shows it, the part the step points at framed;
 *  every frame drawn once in one box (a frame at a time), so the page does not jump between them. */
function Web({step, index}: {step: LauncherStep; index: number}) {
  const lang = useLang();
  const {withBaseUrl} = useBaseUrlUtils();
  const shown = step.frames[index][0];
  const names = [...new Set(step.frames.map((f) => f[0]))];
  // The box takes the first picture's shape (a wide part of a page is not shrunk into a screen's shape).
  const first = WEB[names[0].slice(4)]?.[lang];
  return (
    <div className="cz-web" role="img" aria-label={step.say[lang]} style={first ? {aspectRatio: `${first.w + 40} / ${first.h + 40}`} : undefined}>
      {names.map((frame) => {
        const name = frame.slice(4);
        const shot = WEB[name]?.[lang];
        if (!shot) throw new Error(`cardenza: no community picture ${name}`);
        return (
          <svg key={frame} viewBox={`0 0 ${shot.w} ${shot.h}`} preserveAspectRatio="xMidYMid meet" style={{opacity: frame === shown ? 1 : 0}}>
            <image href={withBaseUrl(`/img/community/${lang}/${name}.png`)} x="0" y="0" width={shot.w} height={shot.h} />
            {shot.marks.map(([x, y, w, h], i) => (
              <g key={i} className="cz-web-mark">
                <rect className="under" x={x - 6} y={y - 6} width={w + 12} height={h + 12} rx="10" />
                <rect className="cz-web-hl" x={x - 6} y={y - 6} width={w + 12} height={h + 12} rx="10" />
              </g>
            ))}
          </svg>
        );
      })}
    </div>
  );
}

/** One step of a Launcher lesson on the stage. */
export function LauncherStage({step}: {step: LauncherStep}) {
  const lang = useLang();
  const {index, down} = useMoment(step);
  const web = step.frames.every((f) => f[0].startsWith('web:'));
  const screen = useScreens(web ? {...step, frames: []} : step, step.frames[index][0]);
  const framed = stepKeys(step);
  const label = step.say[lang];
  const marks = framed.filter((k) => k !== 'HOME').map((k) => ({keys: k}));
  const keys = usePcbKeys({levels: null, focusOnly: marks.length > 0, label, screen, marks, pressed: matrixIndices(down)});
  return (
    <div className="st-page">
      <div className="st-keys">
        {web ? <Web step={step} index={index} /> : (
          <div className="cz-device">
            {step.board ? <Board screen={screen} framed={framed} down={down} label={label} /> : keys.svg}
          </div>
        )}
      </div>
    </div>
  );
}

/** A Launcher lesson's card picture: the screen its "thumb" names (docs/learn/guide.json). */
export function LauncherThumb({name, className}: {name: string; className: string}) {
  const lang = useLang();
  const urls = useUrls();
  const {withBaseUrl} = useBaseUrlUtils();
  const [, fixed, base] = /^(?:(zh|en):)?(.*)$/.exec(name)!;
  const src = name.startsWith('shot:') ? urls.shot(name.slice(5)) : withBaseUrl(`/img/launcher/${fixed || lang}/${base}.png`);
  return <div className={className}><img src={src} alt="" loading="lazy" /></div>;
}
