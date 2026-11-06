// The device map (every key and screen area links to its section) and the scene map (every page,
// with its screenshot, ./scene-map.ts).
import React from 'react';
import Link from '@docusaurus/Link';
import {SCENE_MAP} from './scene-map';
import {useLang, useUrls} from './common';

const MAP_LEGEND = [
  ['ESC', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '↑'],
  ['TAB', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '↓'],
  ['FN', '', 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', "'", '↵'],
  ['CTRL', '', '', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', '▶'],
];
const LEFT_LINKS = ['nav-scenes', 'nav-tracks', 'fn-menu', 'nav-pages'];
const RIGHT_LINKS = ['keys-right', 'keys-right', 'keys-right', 'seq-transport'];
const LEFT = ['esc', 'tab', 'fn', 'ctrl'];
const RIGHT = ['add', 'sub', 'enter', 'play'];
const QUICK = new Set(['0,6', '0,7', '0,8', '1,6', '1,7', '1,8']);

const TEXT = {
  zh: {esc: 'ESC：切换场景 / 返回', tab: 'TAB：切换轨道', fn: 'FN：当前画面的菜单', ctrl: 'CTRL：切换页面',
    add: 'ADD：加', sub: 'SUB：减', enter: 'ENTER：确认', play: 'PLAY：播放 / 停止',
    pads: '演奏区：弹当前轨', grid: 'GRID：屏幕 4×4', quick: 'FN 快捷：BPM / 音量 / 八度',
    screen: '屏幕', header: '状态行', footer: '页签', content: '内容区（对应 GRID）', strips: '两侧色块 = 两侧按键',
    home: 'HOME 侧键：不用（全部停止是 FN + ESC）', hold: '按住左侧键'},
  en: {esc: 'ESC: scenes / back', tab: 'TAB: tracks', fn: 'FN: the menu of this screen', ctrl: 'CTRL: pages',
    add: 'ADD: increase', sub: 'SUB: decrease', enter: 'ENTER: confirm', play: 'PLAY: play / stop',
    pads: 'Pads: play the current track', grid: "GRID: the screen's 4×4", quick: 'FN quick: BPM / volume / octave',
    screen: 'Screen', header: 'Status line', footer: 'Page tabs', content: 'Content (mirrors the GRID)',
    strips: 'Edge strips = edge keys', home: 'HOME side key: unused (stop all is FN + ESC)', hold: 'Hold a left key'},
};
type Words = typeof TEXT.en;

export function DeviceMap() {
  const lang = useLang();
  const urls = useUrls();
  const m: Words = TEXT[lang];
  const w = 34, h = 30, gap = 4, ox = 10, oy = 196;
  const width = ox * 2 + 14 * (w + gap) + 12;
  const height = oy + 4 * (h + gap) + 14;
  const sx = width / 2 - 170, sy = 10, sw = 340, sh = 172;
  const area = (key: string, x: number, y: number, aw: number, ah: number, href: string, label: string, cls: string) => (
    <a key={key} href={urls.section(href)} className={`map-link ${cls}`}>
      <title>{label}</title>
      <rect x={x} y={y} width={aw} height={ah} rx="3" />
    </a>
  );
  const keys: React.ReactElement[] = [];
  for (let row = 0; row < 4; row++) {
    for (let col = 0; col < 14; col++) {
      const x = ox + col * (w + gap) + (col >= 1 ? 4 : 0) + (col >= 9 ? 4 : 0) + (col >= 13 ? 4 : 0);
      const y = oy + row * (h + gap);
      let href: string, label: string, cls: string;
      if (col === 0) [href, label, cls] = [LEFT_LINKS[row], m[LEFT[row] as keyof Words], LEFT[row]];
      else if (col === 13) [href, label, cls] = [RIGHT_LINKS[row], m[RIGHT[row] as keyof Words], 'right'];
      else if (col >= 9) [href, label, cls] = ['keys-grid', m.grid, 'grid'];
      else if (QUICK.has(`${row},${col}`)) [href, label, cls] = ['fn-quick', m.quick, 'pad quick'];
      else [href, label, cls] = ['play-keyboard', m.pads, 'pad'];
      keys.push(
        <a key={`${row}-${col}`} href={urls.section(href)} className={`map-key ${cls}`}>
          <title>{label}</title>
          <rect x={x} y={y} width={w} height={h} rx="5" />
          <text x={x + w / 2} y={y + h / 2 + 4}>{MAP_LEGEND[row][col]}</text>
        </a>,
      );
    }
  }
  const legend: [string, keyof Words][] = [['esc', 'esc'], ['tab', 'tab'], ['fn', 'fn'], ['ctrl', 'ctrl'], ['pad', 'pads'],
    ['grid', 'grid'], ['quick', 'quick'], ['right', 'play']];
  return (
    <figure className="cz-device">
      <svg className="cz-device-map" viewBox={`0 0 ${width} ${height}`} role="img" aria-label="Cardenza">
        <rect className="body" x="4" y="4" width={width - 8} height={oy + 4 * (h + gap) + 6} rx="16" />
        <rect className="screen" x={sx} y={sy} width={sw} height={sh} rx="6" />
        {area('header', sx + 14, sy + 6, sw - 28, 16, 'screen-layout', m.header, 'zone')}
        {area('content', sx + 14, sy + 26, sw - 28, sh - 52, 'keys-grid', m.content, 'zone')}
        {area('footer', sx + 14, sy + sh - 22, sw - 28, 16, 'nav-pages', m.footer, 'zone')}
        {LEFT.map((color, row) => area(`l${row}`, sx + 2, sy + 6 + (row * (sh - 12)) / 4, 9, (sh - 12) / 4 - 3, 'lights-edges', m.strips, `strip ${color}`))}
        {LEFT.map((_, row) => area(`r${row}`, sx + sw - 11, sy + 6 + (row * (sh - 12)) / 4, 9, (sh - 12) / 4 - 3, 'lights-edges', m.strips, 'strip right'))}
        <text className="map-label" x={sx + sw / 2} y={sy + sh / 2 + 4}>{m.screen}</text>
        {area('home', width - 26, 60, 12, 40, 'keys-home', m.home, 'home')}
        {keys}
      </svg>
      <ul className="cz-map-legend">
        {legend.map(([c, k]) => <li key={c}><span className={`sw ${c}`} />{m[k]}</li>)}
      </ul>
    </figure>
  );
}

export function SceneMap() {
  const lang = useLang();
  const urls = useUrls();
  return (
    <div className="cz-scene-map">
      {SCENE_MAP.map(([title, section, items]) => (
        <section key={title} className="scene-col">
          <Link className="scene-title" to={urls.sectionPath(section)}>{title === 'HOLD' ? TEXT[lang].hold : title}</Link>
          {items.map(([label, sec, shot]) => (
            <Link key={label} className="scene-card" to={urls.sectionPath(sec)}>
              <img loading="lazy" src={urls.shot(shot)} width="480" height="270" alt={label} />
              <span>{label}</span>
            </Link>
          ))}
        </section>
      ))}
    </div>
  );
}
