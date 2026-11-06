// The device map: the board as it is (static/img/device/cardenza-pcb.svg, the PCB drawing: switches
// KEY1..KEY56 row by row from the top left, their LEDs, the HOME side key key-FUNC, the display) with
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
    ['单击：在 1 INSTR、2 SEQ、3 TAPE 之间轮换；在 SETUP 里回到原来的场景，确认框里是「否」。',
      'Tap: the next of 1 INSTR, 2 SEQ, 3 TAPE; in SETUP back to where you were, in a confirmation "no".'],
    ['按住：GRID 变成场景地图（第 1 列进 0 SETUP）。', 'Hold: the GRID becomes the scene map (column 1 is 0 SETUP).']]},
  TAB: {name: ['轨道', 'Tracks'], section: 'nav-tracks', lines: [
    ['单击：换到下一条轨道。', 'Tap: the next track.'],
    ['按住：GRID 变成轨道地图，直接选一条。', 'Hold: the GRID becomes the track map, pick one.']]},
  FN: {name: ['这一页的菜单', "This screen's menu"], section: 'fn-menu', lines: [
    ['单击：打开这一页的菜单，再点 GRID 执行。', "Tap: opens this screen's menu; a GRID key runs an item."],
    ['按住：菜单显示在 GRID 上；和别的键一起按是第二功能。', 'Hold: the menu on the GRID; with another key, its second function.']]},
  CTRL: {name: ['页面', 'Pages'], section: 'nav-pages', lines: [
    ['单击：换到下一页。', 'Tap: the next page.'],
    ['按住：GRID 变成这个场景的页面地图。', "Hold: the GRID becomes the map of this scene's pages."]]},
};
const RIGHT_TEXT: Record<string, {name: Words; lines: Words[]; section: string}> = {
  ADD: {name: ['调大 / 往上', 'More / up'], section: 'keys-right', lines: [['调大当前的值；在列表里往上。', 'Raises the value in focus; goes up a list.']]},
  SUB: {name: ['调小 / 往下', 'Less / down'], section: 'keys-right', lines: [['调小当前的值；在列表里往下。', 'Lowers the value in focus; goes down a list.']]},
  OK: {name: ['确认', 'Confirm'], section: 'keys-right', lines: [['确认、进入；确认框里是「是」。', 'Confirms and opens; in a confirmation, "yes".']]},
  PLAY: {name: ['播放 / 停止', 'Play / stop'], section: 'seq-transport', lines: [['开始或停止播放。', 'Starts or stops playback.']]},
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
          [`屏幕中间 4×4 的第 ${row + 1} 行第 ${column - 8} 列：点一下选中这一格。`, `Row ${row + 1}, column ${column - 8} of the screen's 4×4: a tap picks that cell.`],
          ['按住一个参数格：琴键变成它的取值。', "Hold a parameter's cell: the pads become its values."]]});
      } else {
        out.push({...base, cap: `r${row}c${column}`, section: 'play-keyboard', name: ['琴键', 'Pads'], lines: [
          ['弹当前轨道：两个八度，下排白键、上排黑键。', 'Plays the current track: two octaves, white keys below, black keys above.'],
          ['鼓轨上一列是一种鼓，越往上越重。', 'On a drum track a column is a drum, harder towards the top.']]});
      }
    }
  }
  out.push({id: 'home', rect: [HOME.x - 1, HOME.y - 1, HOME.w + 2, HOME.h + 2], kind: 'home', name: ['HOME 侧键', 'HOME side key'],
    section: 'keys-home', lines: [['在 groovebox 里不用；要停下所有声音按 FN + ESC。', 'Unused by the groovebox; to stop everything, FN + ESC.']]});
  out.push({id: 'header', rect: screen(9, 0, 221, 11), kind: 'screen', name: ['状态行', 'Status line'], section: 'screen-layout',
    lines: [['场景、轨道、音色和 BPM。', 'Scene, track, sound and BPM.']]});
  out.push({id: 'content', rect: screen(9, 13, 222, 110), kind: 'screen', name: ['内容区', 'Content'], section: 'keys-grid',
    lines: [['4×4 的格子，和右边的 GRID 一一对应。', 'A 4×4 of cells, one for each GRID key on the right.']]});
  out.push({id: 'footer', rect: screen(9, 123, 221, 12), kind: 'screen', name: ['页签', 'Page tabs'], section: 'nav-pages',
    lines: [['这个场景的页面；CTRL 切换。', "This scene's pages; CTRL switches them."]]});
  out.push({id: 'strips-left', rect: screen(0, 0, 8, 135), kind: 'screen', name: ['两侧色块', 'Edge strips'], section: 'lights-edges',
    lines: [['对应左边的 ESC、TAB、FN、CTRL，颜色和亮度跟键灯一样。', 'The left keys ESC, TAB, FN, CTRL, in their colour and as bright as their lights.']]});
  out.push({id: 'strips-right', rect: screen(232, 0, 8, 135), kind: 'screen', name: ['两侧色块', 'Edge strips'], section: 'lights-edges',
    lines: [['对应右边的 ↑、↓、OK、PLAY，颜色和亮度跟键灯一样。', 'The right keys ↑, ↓, OK, PLAY, in their colour and as bright as their lights.']]});
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
        ? say('把鼠标移到一个键或屏幕上，看它做什么；点一下去它的说明。', 'Point at a key or at the screen to see what it does; click to go to its section.')
        : say('点一个键或屏幕看它做什么，再点说明链接。', 'Tap a key or the screen to see what it does, then its link.')}</figcaption>
    </figure>
  );
}
