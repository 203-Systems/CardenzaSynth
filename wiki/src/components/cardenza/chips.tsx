// Inline chips, as the screen and the simulator draw them: key caps (in their role colour, chords
// and sequences), key lights, scenes, tracks and pages.
import React from 'react';
import {keysFor, keyRole, capText} from './keys';
import {useLang, say} from './common';

function Cap({token, hold}: {token: string; hold: boolean}) {
  const lang = useLang();
  const [name, ...rest] = token.trim().split('~');
  const custom = rest.join('~').trim();
  const keys = keysFor(name);
  const up = name.trim().toUpperCase();
  let role: string, legend: string, tag: string;
  if (keys.length === 1) {
    const [row, column] = keys[0];
    role = keyRole(row, column);
    [legend, tag] = capText(row, column);
    if (column === 0) tag = '';
    if (custom) {
      legend = custom;
      if (column !== 13) tag = '';
    } else if (!legend.trim()) {
      legend = 'PAD';
    }
  } else {
    const columns = keys.map(([, c]) => c);
    role = columns.every((c) => c >= 9 && c <= 12) ? 'grid' : columns.every((c) => c >= 1 && c <= 8) ? 'pad' : up === 'RIGHT' ? 'right' : 'pad';
    legend = custom || ({PADS: 'PADS', GRID: 'GRID', LEFT: 'LEFT', RIGHT: 'RIGHT'} as Record<string, string>)[up] || up;
    tag = '';
    const m = /^G(\d+)-G?(\d+)$/.exec(up);
    if (m && !custom) legend = `G${m[1]}–G${m[2]}`;
  }
  if (/^G\d+$/.test(up) && !custom) tag = up; // the GRID cell number next to the key's own legend
  return (
    <span className={`cz-kc k-${role}${hold ? ' hold' : ''}`}>
      {hold && <i className="held">{say(lang, '按住', 'HOLD')}</i>}
      <b>{legend}</b>
      {tag && <small>{tag}</small>}
    </span>
  );
}

/** "FN+6" -> keys joined by +; "hold ESC + G3" -> a held ESC, then G3. */
function Chord({spec}: {spec: string}) {
  const keys = spec.trim().split('+').map((p) => p.trim()).filter(Boolean);
  return (
    <span className="cz-combo">
      {keys.map((part, i) => {
        const hold = part.toLowerCase().startsWith('hold ');
        return (
          <React.Fragment key={i}>
            {i > 0 && <span className="cz-plus">+</span>}
            <Cap token={hold ? part.slice(5).trim() : part} hold={hold} />
          </React.Fragment>
        );
      })}
    </span>
  );
}

/** {{key:...}}: "TAB, TAB" are presses one after another, "FN+6" a chord, "hold ESC + G3". */
export function Key({k}: {k: string}) {
  if ([',', 'FN+,'].includes(k.trim())) return <Chord spec={k} />; // the comma key itself
  const steps = k.split(',').filter((s) => s.trim());
  return (
    <>
      {steps.map((step, i) => (
        <React.Fragment key={i}>
          {i > 0 && <span className="cz-then">›</span>}
          <Chord spec={step} />
        </React.Fragment>
      ))}
    </>
  );
}

const LED_STATES = ['off', 'half', 'full', 'blink'];

export function Led({state, children}: {state: string; children?: React.ReactNode}) {
  if (!LED_STATES.includes(state)) throw new Error(`cardenza: unknown light ${state}`);
  return (
    <span className="cz-ledc">
      <span className={`cz-led ${state}`} aria-hidden="true" />
      {children}
    </span>
  );
}

const SCENES: Record<string, string> = {INSTR: '1', SEQ: '2', TAPE: '3', SETUP: '0', FN: 'FN'};
const TRACKS: Record<string, string> = {'1': 'DRUMS', '2': 'JUNO', '3': 'DX7', '4': 'SAMPLER'};

export function Scene({name}: {name: string}) {
  const number = SCENES[name.toUpperCase()];
  if (number === undefined) throw new Error(`cardenza: unknown scene ${name}`);
  if (number === 'FN') return <span className="cz-chip scene fn"><b>FN</b> MENU</span>;
  return <span className="cz-chip scene"><b>{number}</b> {name.toUpperCase()}</span>;
}

export function Track({n}: {n: string}) {
  if (!TRACKS[n]) throw new Error(`cardenza: unknown track ${n}`);
  return <span className={`cz-chip trk t${n}`}><b>T{n}</b> {TRACKS[n]}</span>;
}

export function Page({name}: {name: string}) {
  return <span className="cz-chip page">{name}</span>;
}

/** Extra search words for the section it sits in (plugins/search-index.js reads them). */
export function Keywords({k}: {k: string}) {
  return <span hidden data-keywords={k} />;
}
