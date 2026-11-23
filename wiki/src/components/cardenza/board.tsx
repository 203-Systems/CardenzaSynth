// The device map: the board as it is (static/img/device/cardenza-pcb.svg, the PCB drawing: switches
// KEY1..KEY56 row by row from the top left, their LEDs, the HOME side key key-HOME, the display) with
// the firmware's own screen in its display, and over it a layer of hotspots: every key, the HOME key
// and the screen's areas. Pointing at one (or focusing it) shows what it does, its FN combination
// (from the firmware, CardenzaLearnDump) and its section; a click goes there. On a touch screen the
// first tap shows it and the link in it goes there.
import React, {useEffect, useRef, useState} from 'react';
import Link from '@docusaurus/Link';
import {useHistory} from '@docusaurus/router';
import dump from '@site/generated/data/dump.json';
import sections from '@site/generated/data/sections.json';
import {Key} from './chips';
import {useLang, useUrls, type Lang} from './common';
import {LEFT_ROLES, LEGEND, RIGHT_ROLES} from './keys';
import {BOARD, DISPLAY, HOME, cell, useBoardImage, type Rect} from './pcb';

const BOOT_SHOT = 'instr-boot';

type Words = [string, string];
type Spot = {
  id: string;
  rect: Rect;
  kind: 'key' | 'screen' | 'home';
  cap?: string; // a <Key k> spec
  name: Words;
  lines: Words[];
  fn?: {keys: string; label: string}[];
  section: string;
};

/** A rectangle of the screen in its pixels (240 x 135) on the board. */
function screen(x: number, y: number, w: number, h: number): Rect {
  const sx = DISPLAY.w / 240, sy = DISPLAY.h / 135;
  return [DISPLAY.x + x * sx, DISPLAY.y + y * sy, w * sx, h * sy];
}

type Dump = {fnEntries: {key: string; label: string}[]; fnQuick: {up: string; down: string; label: string}[]};
const FN = dump as unknown as Dump;

/** The FN combinations of a key, from the firmware's list (Controller::FnEntries, the FN quick keys). */
function fnFor(row: number, column: number): {keys: string; label: string}[] {
  const out: {keys: string; label: string}[] = [];
  const legend = LEGEND[row][column].toUpperCase();
  const role = column === 0 ? LEFT_ROLES[row] : column === 13 ? RIGHT_ROLES[row] : column >= 9 ? 'GRID' : '';
  for (const q of FN.fnQuick) {
    if (column > 0 && column < 9 && legend === q.up) out.push({keys: `FN+${q.up}`, label: `${q.label} +`});
    if (column > 0 && column < 9 && legend === q.down) out.push({keys: `FN+${q.down}`, label: `${q.label} −`});
  }
  for (const e of FN.fnEntries) {
    if (e.key === role && role === 'GRID') out.push({keys: 'FN+GRID', label: e.label});
    else if (e.key === role && role) out.push({keys: `FN+${role}`, label: e.label});
    else if (e.key === '+-' && (role === 'ADD' || role === 'SUB')) out.push({keys: `FN+${role}`, label: e.label});
  }
  return out;
}

const LEFT_TEXT: Record<string, {name: Words; lines: Words[]; section: string}> = {
  ESC: {name: ['场景 / 返回', 'Scenes / back'], section: 'nav-scenes', lines: [
    ['单击可在 1 INSTR、2 SEQ、3 TAPE 之间循环；在 SETUP 中返回原场景，在确认框中取消。',
      'Tap to cycle between 1 INSTR, 2 SEQ, and 3 TAPE, exit SETUP, or cancel dialogs.'],
    ['按住可在 GRID 上展开场景地图（第 1 列进入 0 SETUP）。', 'Hold to show the scene map on the GRID (column 1 opens 0 SETUP).']]},
  TAB: {name: ['轨道', 'Tracks'], section: 'nav-tracks', lines: [
    ['单击切换到下一条轨道。', 'Tap to switch to the next track.'],
    ['按住可在 GRID 上展开轨道地图。', 'Hold to open the track map on the GRID.']]},
  FN: {name: ['快捷菜单', 'Context menu'], section: 'fn-menu', lines: [
    ['单击常驻打开当前页面菜单，点按 GRID 执行对应项。', 'Tap to latch the current screen menu open; tap a GRID key to run an item.'],
    ['按住显示 GRID 菜单，并启用全键盘第二功能。', 'Hold to display the menu on the GRID and activate secondary shortcuts.']]},
  CTRL: {name: ['页面', 'Pages'], section: 'nav-pages', lines: [
    ['单击切换到下一页。', 'Tap to move to the next page.'],
    ['按住可在 GRID 上展开当前场景的页面地图。', 'Hold to display the page map for the current scene.']]},
};
const RIGHT_TEXT: Record<string, {name: Words; lines: Words[]; section: string}> = {
  ADD: {name: ['增大 / 上移', 'Increase / up'], section: 'keys-right', lines: [['增大当前焦点数值，或在列表中向上移动。', 'Increase the focused value or move up in lists.']]},
  SUB: {name: ['减小 / 下移', 'Decrease / down'], section: 'keys-right', lines: [['减小当前焦点数值，或在列表中向下移动。', 'Decrease the focused value or move down in lists.']]},
  OK: {name: ['确认', 'Confirm'], section: 'keys-right', lines: [['确认、进入或切换（确认框中表示「是」）。', 'Confirm, open, or toggle (answers "yes" in prompts).']]},
  PLAY: {name: ['播放 / 停止', 'Play / stop'], section: 'seq-transport', lines: [['开始或停止播放。', 'Start or stop playback.']]},
};

function spots(): Spot[] {
  const out: Spot[] = [];
  for (let row = 0; row < 4; row++) {
    for (let column = 0; column < 14; column++) {
      const base = {id: `k${row}-${column}`, rect: cell(row, column), kind: 'key' as const, fn: fnFor(row, column)};
      if (column === 0) {
        const t = LEFT_TEXT[LEFT_ROLES[row]];
        out.push({...base, cap: LEFT_ROLES[row], ...t});
      } else if (column === 13) {
        const t = RIGHT_TEXT[RIGHT_ROLES[row]];
        out.push({...base, cap: RIGHT_ROLES[row], ...t});
      } else if (column >= 9) {
        const n = row * 4 + column - 8;
        out.push({...base, cap: `G${n}`, section: 'keys-grid', name: ['GRID', 'GRID'], lines: [
          [`对应屏幕 4×4 网格第 ${row + 1} 行第 ${column - 8} 列，点按选中该格。`, `Row ${row + 1}, column ${column - 8} of the on-screen 4×4 grid. Tap to select this cell.`],
          ['按住参数格可在演奏区直接选择档位。', 'Hold a parameter cell to pick its value directly on the pads.']]});
      } else {
        out.push({...base, cap: `r${row}c${column}`, section: 'play-keyboard', name: ['演奏区', 'Pads'], lines: [
          ['演奏当前轨道：两个八度，下排白键、上排黑键。', 'Play the active track: two octaves with white keys below and black keys above.'],
          ['鼓轨每列对应一种鼓，越靠上力度越大。', 'On drum tracks, each column is one drum with higher rows playing louder.']]});
      }
    }
  }
  out.push({id: 'home', rect: [HOME.x - 1, HOME.y - 1, HOME.w + 2, HOME.h + 2], kind: 'home', name: ['HOME 侧键', 'HOME side button'],
    section: 'keys-home', lines: [['按下打开系统菜单（Cardenza Synth 0.2.1 起）：返回启动器、重新启动、关机、下载模式。', 'Press to open the system menu (Cardenza Synth 0.2.1+): Back to Launcher, Restart, Power off, Download mode.'],
      ['插线时请勿按住（会进入下载模式）；全部停止请按 FN + ESC。', 'Do not hold while plugging in USB (boots download mode). Use FN + ESC for All Stop.']]});
  out.push({id: 'header', rect: screen(9, 0, 221, 11), kind: 'screen', name: ['状态栏', 'Status bar'], section: 'screen-layout',
    lines: [['显示当前场景、轨道、音色与 BPM。', 'Active scene, track, preset, and BPM.']]});
  out.push({id: 'content', rect: screen(9, 13, 222, 110), kind: 'screen', name: ['内容区', 'Content area'], section: 'keys-grid',
    lines: [['4×4 格子区域，与右侧物理 GRID 一一对应。', '4×4 cell layout matching the physical GRID keys on the right.']]});
  out.push({id: 'footer', rect: screen(9, 123, 221, 12), kind: 'screen', name: ['页签栏', 'Page tabs'], section: 'nav-pages',
    lines: [['当前场景的全部子页面（按 CTRL 切换）。', 'Pages in the current scene (switch with CTRL).']]});
  out.push({id: 'strips-left', rect: screen(0, 0, 8, 135), kind: 'screen', name: ['左侧边条', 'Edge strips'], section: 'lights-edges',
    lines: [['同步显示左侧 ESC、TAB、FN、CTRL 的配色与背光状态。', 'Mirrors the color and LED state of ESC, TAB, FN, and CTRL.']]});
  out.push({id: 'strips-right', rect: screen(232, 0, 8, 135), kind: 'screen', name: ['右侧边条', 'Edge strips'], section: 'lights-edges',
    lines: [['同步显示右侧 ADD、SUB、OK、PLAY 的配色与背光状态。', 'Mirrors the color and LED state of ADD, SUB, OK, and PLAY.']]});
  return out;
}

const SPOTS = spots();
const TITLES = sections as Record<string, {title: Record<Lang, string>}>;

function Pop({spot, lang, below, onEnter, onLeave}: {spot: Spot; lang: Lang; below: boolean; onEnter: () => void; onLeave: () => void}) {
  const urls = useUrls();
  const [x, y, w, h] = spot.rect;
  const say = (words: Words) => (lang === 'zh' ? words[0] : words[1]);
  const style = {
    '--x': ((x + w / 2) / BOARD.w) * 100,
    '--y': ((below ? y + h : y) / BOARD.w) * 100,
  } as React.CSSProperties;
  return (
    <div className="cz-board-pop" role="tooltip" data-below={below || undefined} style={style}
      onMouseEnter={onEnter} onMouseLeave={onLeave}>
      <div className="pop-head">
        {spot.cap && <Key k={spot.cap} />}
        <b>{say(spot.name)}</b>
      </div>
      {spot.lines.map((line, i) => <p key={i}>{say(line)}</p>)}
      {spot.fn && spot.fn.length > 0 && (
        <p className="pop-fn">{spot.fn.map((f) => <span key={f.keys}><Key k={f.keys} /> {f.label}</span>)}</p>
      )}
      <Link className="pop-more" to={urls.sectionPath(spot.section)}>{TITLES[spot.section]?.title[lang] ?? spot.section} →</Link>
    </div>
  );
}

export function DeviceMap() {
  const lang = useLang();
  const urls = useUrls();
  const history = useHistory();
  const board = useBoardImage();
  const [active, setActive] = useState<string | null>(null);
  const [hover, setHover] = useState(true);
  const timer = useRef<ReturnType<typeof setTimeout> | undefined>(undefined);
  useEffect(() => {
    setHover(window.matchMedia('(hover: hover)').matches);
    return () => clearTimeout(timer.current);
  }, []);
  const show = (id: string) => {
    clearTimeout(timer.current);
    setActive(id);
  };
  const hideSoon = () => {
    clearTimeout(timer.current);
    timer.current = setTimeout(() => setActive(null), 160);
  };
  const spot = SPOTS.find((s) => s.id === active) || null;
  const say = (zh: string, en: string) => (lang === 'zh' ? zh : en);
  return (
    <figure className="cz-board">
      <div className="cz-board-frame">
        <img src={board} alt={say('鸣片的电路板：屏幕在上，下面是 4 排 14 列按键', 'The Cardenza board: the screen above, 4 rows of 14 keys below')}
          width={BOARD.w * 4} height={BOARD.h * 4} />
        <svg className="cz-board-spots" viewBox={`0 0 ${BOARD.w} ${BOARD.h}`}>
          <image href={urls.shot(BOOT_SHOT)} x={DISPLAY.x} y={DISPLAY.y} width={DISPLAY.w} height={DISPLAY.h} preserveAspectRatio="none" />
          {SPOTS.map((s) => {
            const [x, y, w, h] = s.rect;
            const label = `${s.cap ? s.cap + ' · ' : ''}${lang === 'zh' ? s.name[0] : s.name[1]}`;
            const href = urls.section(s.section);
            return (
              <a key={s.id} href={href} className={`spot ${s.kind}${active === s.id ? ' on' : ''}`} aria-label={label}
                onMouseEnter={() => show(s.id)} onMouseLeave={hideSoon} onFocus={() => show(s.id)} onBlur={hideSoon}
                onClick={(event) => {
                  event.preventDefault();
                  if (!hover && active !== s.id) {
                    show(s.id);
                    return;
                  }
                  history.push(href); // with the base URL (history knows none)
                }}>
                <rect x={x} y={y} width={w} height={h} rx={s.kind === 'key' ? 2.2 : 1} />
              </a>
            );
          })}
        </svg>
      </div>
      {spot && <Pop spot={spot} lang={lang} below={spot.kind !== 'key'} onEnter={() => show(spot.id)} onLeave={hideSoon} />}
      <figcaption>{hover
        ? say('将鼠标移至任意按键或屏幕区域即可查看说明，点击跳转至对应章节。', 'Hover over any key or screen area to see its role; click to jump to its documentation.')
        : say('点击任意按键或屏幕区域查看说明，再点击链接阅读详情。', 'Tap any key or screen area to see its role, then tap the link to read more.')}</figcaption>
    </figure>
  );
}
