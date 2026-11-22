// The Launcher's lessons (docs/learn/guide.json, "anim": "launcher"): the device as the other lessons
// draw it (the board, its screen in its display, the keys framed) showing the 203 Launcher's real
// screens, drawn by the Launcher's own code (tools/wiki/launcher/shots.py -> static/img/launcher/<lang>/).
// Each step plays its frames in a loop: [screen, what the hands do as it shows, seconds].
//   screen: a Launcher screen (home-1, store, ...) in the reader's language ("en:home-9": in English
//           whatever the language), "off" (dark: unplugged, or the chip's download mode),
//           "shot:<name>" (the groovebox's screenshot of that name) or "web:<name>" (a part of the 203
//           community's page, tools/wiki/launcher/community.mjs -> static/img/community/<lang>/, with the
//           button or field the step points at framed). A step's frames are all of the device or all web.
//           "wire:<name>": a line drawing of what is not the device or the community (the computer's browser on
//           the community, the browser's port chooser), drawn here (WIRES), in the reader's language.
//   action: null; a key pressed as the frame ends ("OK", "/"); "hold:FN" held through the frame;
//           "type:V6QK28" typed key by key through it; "cable:in" / "cable:out" the USB cable in the
//           device's port or pulled out of it from this frame on (a board step).
//           Parts go together with a space: "hold:HOME cable:in". HOME (the side key, GPIO0) shows on the
//           whole board.
// A step with "board": true shows the whole board (the HOME key left of the screen); otherwise the keys
// under the screen magnified, as the device lessons do. A step's frames are all of the device, all web or all
// wire.
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

/** A frame's action in its parts ("hold:HOME cable:in"). */
const actionParts = (action: string | null): string[] => (action ? action.split(/\s+/).filter(Boolean) : []);

/** The tokens a frame's action names: the keys pressed, held, or typed (letters and digits); not the cable. */
function actionKeys(action: string | null, typed = true): string[] {
  const out: string[] = [];
  for (const part of actionParts(action)) {
    if (part.startsWith('hold:')) out.push(part.slice(5));
    else if (part.startsWith('type:')) { if (typed) out.push(...[...part.slice(5)].map((c) => c.toUpperCase())); }
    else if (!part.startsWith('cable:')) out.push(part);
  }
  return out;
}

/** The keys a step frames: its own list, or what its frames press and hold (not what they type). */
export function stepKeys(step: LauncherStep): string[] {
  if (step.keys) return step.keys;
  const out: string[] = [];
  for (const [, action] of step.frames) for (const key of actionKeys(action, false)) if (!out.includes(key)) out.push(key);
  return out;
}

type Cable = 'in' | 'out' | null;
/** The USB cable at frame `index`: the last "cable:" up to it, or null (a step that never names it draws none). */
function cableAt(step: LauncherStep, index: number): Cable {
  for (let i = index; i >= 0; i--) {
    const part = actionParts(step.frames[i][1]).find((p) => p.startsWith('cable:'));
    if (part) return part === 'cable:in' ? 'in' : 'out';
  }
  const first = step.frames.flatMap((f) => actionParts(f[1])).find((p) => p.startsWith('cable:'));
  return first ? (first === 'cable:in' ? 'out' : 'in') : null;
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
  for (const part of actionParts(action)) {
    if (part.startsWith('cable:')) continue;
    if (part.startsWith('hold:')) down.push(part.slice(5));
    else if (part.startsWith('type:')) {
      const word = part.slice(5);
      const n = Math.floor(left / TYPE_EVERY);
      if (n < word.length && left % TYPE_EVERY < TYPE_EVERY * 0.6) down.push(word[n].toUpperCase());
    } else if (left > seconds - 0.35) down.push(part);
  }
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

// The board drawing's USB-C receptacle (U4), beside the antenna.
const USB_C = {x: 218.45, y: 0.27};
const CABLE_ROOM = 72;
const PLUG_BOOT_Y = -43;

function UsbPlug({unplugged = false}: {unplugged?: boolean}) {
  return (
    <g>
      <rect className="cz-cable-tip" x="-9.3" y="-3.4" width="18.6" height={unplugged ? 11 : 3.4} rx="1.1" />
      <rect className="cz-cable-boot" x="-5" y={PLUG_BOOT_Y} width="10" height="8.5" rx="1.4" />
      <rect className="cz-cable-grip" x="-11.6" y="-37" width="23.2" height="36" rx="2.2" />
      <path className="cz-cable-bevel" d="M-9.8 -3.6 V-34 Q-9.8 -35.2 -8.5 -35.2 H8.6" />
    </g>
  );
}

/** The plug moves straight out from the receptacle, exposing its metal tip. */
function UsbCable({state}: {state: 'in' | 'out'}) {
  const unplugged = state === 'out';
  const plugY = USB_C.y - (unplugged ? 20 : 0);
  return (
    <g>
      <path className="cz-cable-line"
        d={`M${USB_C.x} ${plugY + PLUG_BOOT_Y + 1} C${USB_C.x} ${plugY - 51} ${USB_C.x + 14} ${-CABLE_ROOM + 8} ${BOARD.w + 6} ${-CABLE_ROOM + 5}`} />
      <g transform={`translate(${USB_C.x} ${plugY})`}>
        <UsbPlug unplugged={unplugged} />
      </g>
    </g>
  );
}

/** The whole board (HOME left of the screen), the screen in its display, keys and HOME framed; the USB cable when the
 *  step names it (the picture keeps its room above the board for every frame then). */
function Board({screen, framed, down, label, cable}: {screen: React.ReactElement; framed: string[]; down: string[]; label: string;
  cable: Cable}) {
  const board = useBoardImage();
  const rects: [number, number, number, number][] = [];
  const frame = (token: string): [number, number, number, number][] =>
    token === 'HOME' ? [[HOME.x - 1, HOME.y - 1, HOME.w + 2, HOME.h + 2]] : keysFor(token).map(([r, c]) => cell(r, c));
  framed.forEach((token) => rects.push(...frame(token)));
  const pressed: [number, number, number, number][] = [];
  down.forEach((token) => pressed.push(...frame(token)));
  return (
    <svg className="cz-pcb" viewBox={cable ? `0 ${-CABLE_ROOM} ${BOARD.w} ${BOARD.h + CABLE_ROOM}` : `0 0 ${BOARD.w} ${BOARD.h}`} role="img" aria-label={label}>
      <image href={board} x="0" y="0" width={BOARD.w} height={BOARD.h} preserveAspectRatio="none" />
      {cable && <UsbCable state={cable} />}
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

// ---------------------------------------------------------------------------------------------
// Line drawings (wire:<name>): what the lesson needs that is neither the device nor a page of the community. Flat
// lines in the ink, the part the step points at framed as the community pictures are; text only where the real thing
// has it (an address, the chooser's words), in the reader's language. No hands.

/** A Cardenza from above, small: its outline, its screen (dark: download mode), its keys as a grid. */
function MiniDevice({x, y, w}: {x: number; y: number; w: number}) {
  const h = w * 0.57;
  const keys: React.ReactElement[] = [];
  for (let r = 0; r < 4; r++) {
    for (let c = 0; c < 14; c++) {
      keys.push(<rect key={`${r}-${c}`} x={x + w * 0.04 + c * (w * 0.92 / 14)} y={y + h * 0.5 + r * (h * 0.44 / 4)} width={w * 0.92 / 14 - 1.6} height={h * 0.44 / 4 - 1.6} className="wf-key" />);
    }
  }
  return (
    <g>
      <rect className="wf-line wf-page" x={x} y={y} width={w} height={h} rx="5" />
      <rect className="wf-ink" x={x + w * 0.27} y={y + h * 0.07} width={w * 0.28} height={h * 0.34} rx="1.5" />
      <rect className="wf-line" x={x + w * 0.2} y={y + h * 0.15} width={w * 0.04} height={h * 0.09} rx="1" />
      {keys}
    </g>
  );
}

const miniBoard = {x: 10, y: 196, w: 110};
const miniScale = miniBoard.w / BOARD.w;
const miniPortX = miniBoard.x + USB_C.x * miniScale;
const miniPortY = miniBoard.y + USB_C.y * miniScale;

const WIRES: Record<string, (lang: 'zh' | 'en') => React.ReactElement> = {
  // The recovery page highlights the Launcher-only action. The connected device is in download mode.
  browser: (lang) => (
    <>
      <rect className="wf-line wf-page" x="150" y="22" width="460" height="292" rx="12" />
      <path className="wf-line" d="M118 330 H642 L622 350 H138 Z" />
      <rect className="wf-line" x="170" y="40" width="420" height="258" rx="4" />
      <path className="wf-line" d="M170 66 H590" />
      <circle className="wf-faint" cx="184" cy="53" r="4" /><circle className="wf-faint" cx="198" cy="53" r="4" /><circle className="wf-faint" cx="212" cy="53" r="4" />
      <rect className="wf-line" x="190" y="76" width="380" height="24" rx="12" />
      <text className="wf-text" x="210" y="93">stage.203.io/recover</text>
      <rect className="wf-ink" x="170" y="108" width="420" height="26" />
      <rect className="wf-paper" x="182" y="116" width="34" height="10" />
      <text className="wf-title" x="188" y="158">{lang === 'zh' ? '恢复刷机' : 'Recovery flash'}</text>
      <rect className="wf-line wf-page" x="188" y="171" width="384" height="91" rx="6" />
      <rect className="wf-faint" x="202" y="185" width="290" height="8" />
      <rect className="wf-faint" x="202" y="199" width="216" height="6" />
      <rect className="wf-ink" x="202" y="217" width="248" height="32" rx="5" />
      <text className="wf-text wf-on-ink" x="214" y="239">{lang === 'zh' ? '刷入最新启动器' : 'Flash the newest Launcher'}</text>
      <g className="cz-web-mark"><rect className="under" x="182" y="165" width="396" height="103" rx="8" /><rect className="cz-web-hl" x="182" y="165" width="396" height="103" rx="8" /></g>
      <MiniDevice {...miniBoard} />
      <rect className="wf-grey" x={miniPortX - 5.5} y={miniPortY} width="11" height="9" rx="1" />
      <path className="cz-cable-line wf-cable"
        d={`M${miniPortX} ${miniPortY + (PLUG_BOOT_Y + 1) * miniScale} V175 Q${miniPortX} 167 115 167 H119 Q133 167 133 181 V282 Q133 296 145 296`} />
      <rect className="wf-ink" x="140" y="291" width="12" height="10" rx="2" />
      <g transform={`translate(${miniPortX} ${miniPortY}) scale(${miniScale})`}>
        <UsbPlug />
      </g>
    </>
  ),
  // The browser's port chooser, its one port chosen and Connect framed.
  chooser: (lang) => (
    <>
      <rect className="wf-line wf-page" x="130" y="40" width="380" height="300" rx="10" />
      <text className="wf-title" x="154" y="82">{lang === 'zh' ? 'stage.203.io 想要连接到串口' : 'stage.203.io wants to connect to a serial port'}</text>
      <rect className="wf-line" x="154" y="104" width="332" height="150" rx="4" />
      <rect className="wf-ink" x="156" y="106" width="328" height="34" rx="3" />
      <text className="wf-text wf-on-ink" x="170" y="128">USB JTAG/serial debug unit</text>
      <rect className="wf-faint" x="170" y="160" width="150" height="9" /><rect className="wf-faint" x="170" y="190" width="120" height="9" />
      <rect className="wf-ink" x="388" y="282" width="98" height="34" rx="6" />
      <text className="wf-text wf-on-ink wf-mid" x="437" y="304">{lang === 'zh' ? '连接' : 'Connect'}</text>
      <rect className="wf-line" x="278" y="282" width="98" height="34" rx="6" />
      <text className="wf-text wf-mid" x="327" y="304">{lang === 'zh' ? '取消' : 'Cancel'}</text>
      <g className="cz-web-mark"><rect className="under" x="148" y="100" width="344" height="46" rx="8" /><rect className="cz-web-hl" x="148" y="100" width="344" height="46" rx="8" /></g>
      <g className="cz-web-mark"><rect className="under" x="380" y="274" width="114" height="50" rx="10" /><rect className="cz-web-hl" x="380" y="274" width="114" height="50" rx="10" /></g>
    </>
  ),
};
export const WIRE_NAMES = Object.keys(WIRES);

/** A wire step: its drawings, one shown at a time in the community pictures' box. */
function Wire({step, index}: {step: LauncherStep; index: number}) {
  const lang = useLang();
  const shown = step.frames[index][0];
  const names = [...new Set(step.frames.map((f) => f[0]))];
  return (
    <div className="cz-web cz-wire" role="img" aria-label={step.say[lang]}>
      {names.map((frame) => {
        const draw = WIRES[frame.slice(5)];
        if (!draw) throw new Error(`cardenza: no line drawing ${frame}`);
        return (
          <svg key={frame} viewBox="0 0 640 400" preserveAspectRatio="xMidYMid meet" style={{opacity: frame === shown ? 1 : 0}}>
            {draw(lang)}
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
  const wire = step.frames.every((f) => f[0].startsWith('wire:'));
  const screen = useScreens(web || wire ? {...step, frames: []} : step, step.frames[index][0]);
  const framed = stepKeys(step);
  const label = step.say[lang];
  const cable = step.board ? cableAt(step, index) : null;
  const marks = framed.filter((k) => k !== 'HOME').map((k) => ({keys: k}));
  const keys = usePcbKeys({levels: null, focusOnly: marks.length > 0, label, screen, marks, pressed: matrixIndices(down)});
  return (
    <div className="st-page">
      <div className="st-keys">
        {web ? <Web step={step} index={index} /> : wire ? <Wire step={step} index={index} /> : (
          <div className={cable ? 'cz-device cz-device-with-cable' : 'cz-device'}>
            {step.board ? <Board screen={screen} framed={framed} down={down} label={label} cable={cable} /> : keys.svg}
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
