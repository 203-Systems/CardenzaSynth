// Reference tables generated from the firmware (CardenzaLearnDump): an engine's pages and
// parameters, the SETUP settings, the FN quick keys, the lists, the pages of every scene.
// tools/learn/build.py engine_reference / fn_reference / ...
import React from 'react';
import dump from '@site/generated/data/dump.json';
import {LEGEND} from './keys';
import {useLang, type Lang} from './common';

type Param = {kind?: string; min?: number; max?: number; default?: number; options?: string[]; minText?: string; maxText?: string; defaultText?: string; valueText?: string} | null;
type Slot = {index: number; label: string; id?: string; action?: string; param?: Param};
type EnginePage = {tab: string; name: string; slots: Slot[]};
type Dump = {
  engines: {engine: string; track: number; preset: string; pages: EnginePage[]}[];
  scenes: Record<string, {tab: string}[]>;
  settingsPages: EnginePage[];
  fnQuick: {up: string; down: string; id: string; label: string}[];
  lists: Record<string, string[]>;
};
const DUMP = dump as unknown as Dump;

const T = {
  zh: {slot: 'GRID 位置', param: '参数', range: '范围', default: '默认', options: '选项', row: '行', col: '列', engine: '音源',
    track: '默认轨道', preset: '默认音色', key: '键', does: '作用', toggle: '开 / 关',
    actions: {engine: '选择音源', preset: '音色浏览', save: '保存音色', 'save-as': '另存音色', 'sample-file': '采样文件', 'sample-record': '录音', 'sample-preview': '试听'} as Record<string, string>,
    lists: {scales: '音阶', chords: '和弦', voicings: '转位', layouts: '键盘布局', locks: '音阶锁定', arpModes: 'ARP 模式', arpRates: 'ARP 速度'} as Record<string, string>},
  en: {slot: 'GRID key', param: 'Parameter', range: 'Range', default: 'Default', options: 'Options', row: 'row', col: 'col', engine: 'Engine',
    track: 'Default track', preset: 'Default sound', key: 'Key', does: 'Does', toggle: 'on / off',
    actions: {engine: 'Choose engine', preset: 'Browse sounds', save: 'Save sound', 'save-as': 'Save sound as', 'sample-file': 'Sample file', 'sample-record': 'Record', 'sample-preview': 'Preview'} as Record<string, string>,
    lists: {scales: 'Scales', chords: 'Chords', voicings: 'Voicings', layouts: 'Keyboard layouts', locks: 'Scale lock', arpModes: 'Arp modes', arpRates: 'Arp rates'} as Record<string, string>},
};

function GridName({index, lang}: {index: number; lang: Lang}) {
  const t = T[lang];
  return (
    <>
      <span className="cz-kc k-grid"><b>{LEGEND[Math.floor(index / 4)][9 + (index % 4)]}</b></span>{' '}
      <small>{t.row} {Math.floor(index / 4) + 1} · {t.col} {(index % 4) + 1}</small>
    </>
  );
}

function paramCells(param: Param): [string, string] {
  if (!param) return ['—', '—'];
  const range = param.options?.length ? param.options.join(' / ') : `${param.minText ?? param.min} … ${param.maxText ?? param.max}`;
  return [range, param.defaultText ?? param.valueText ?? String(param.default ?? '')];
}

function SlotTable({lang, slots, cells}: {lang: Lang; slots: Slot[]; cells: (slot: Slot) => [string, string]}) {
  const t = T[lang];
  return (
    <table className="cz-ref">
      <thead><tr><th>{t.slot}</th><th>{t.param}</th><th>{t.range}</th><th>{t.default}</th></tr></thead>
      <tbody>
        {slots.map((slot) => {
          const [range, value] = cells(slot);
          return (
            <tr key={slot.index}>
              <td><GridName index={slot.index} lang={lang} /></td>
              <td><b>{slot.label}</b>{slot.id && <> <code>{slot.id}</code></>}</td>
              <td>{range}</td>
              <td>{value}</td>
            </tr>
          );
        })}
      </tbody>
    </table>
  );
}

function EngineReference({engine, lang}: {engine: string; lang: Lang}) {
  const t = T[lang];
  const entry = DUMP.engines.find((e) => e.engine === engine);
  if (!entry) throw new Error(`cardenza: no engine ${engine} in the dump`);
  return (
    <>
      <p className="cz-ref-meta">{t.engine}: <b>{engine}</b> · {t.track}: T{entry.track} · {t.preset}: {entry.preset}</p>
      {entry.pages.map((page) => (
        <React.Fragment key={page.tab}>
          <h4 className="cz-ref-page">{page.tab} <small>{page.name}</small></h4>
          <SlotTable lang={lang} slots={page.slots}
            cells={(slot) => (slot.action ? [t.actions[slot.action] ?? slot.action, '—'] : paramCells(slot.param ?? null))} />
        </React.Fragment>
      ))}
    </>
  );
}

function SettingsReference({lang}: {lang: Lang}) {
  const t = T[lang];
  return (
    <>
      {DUMP.settingsPages.map((page) => (
        <React.Fragment key={page.tab}>
          <h4 className="cz-ref-page">SETUP {page.tab} <small>{page.name}</small></h4>
          <SlotTable lang={lang} slots={page.slots} cells={(slot) => {
            const p = slot.param || {};
            const range = p.kind === 'range' ? `${p.min} … ${p.max}` : p.kind === 'toggle' ? t.toggle : `${(p.max ?? 0) - (p.min ?? 0) + 1} ${t.options}`;
            return [range, p.valueText ?? ''];
          }} />
        </React.Fragment>
      ))}
    </>
  );
}

export function Ref({engine, kind}: {engine?: string; kind?: string}) {
  const lang = useLang();
  const t = T[lang];
  if (engine) return <EngineReference engine={engine} lang={lang} />;
  if (kind === 'fn') return <SettingsReference lang={lang} />;
  if (kind === 'fn-quick') {
    return (
      <table className="cz-ref">
        <thead><tr><th>{t.key}</th><th>{t.does}</th></tr></thead>
        <tbody>
          {DUMP.fnQuick.map((q) => (
            <tr key={q.id}>
              <td><span className="cz-kc k-fn"><b>FN</b></span>+<span className="cz-kc"><b>{q.up}</b></span> / <span className="cz-kc k-fn"><b>FN</b></span>+<span className="cz-kc"><b>{q.down}</b></span></td>
              <td>{q.label} + / −</td>
            </tr>
          ))}
        </tbody>
      </table>
    );
  }
  if (kind === 'lists') {
    return (
      <table className="cz-ref cz-lists">
        <tbody>{Object.entries(DUMP.lists).map(([key, values]) => <tr key={key}><th>{t.lists[key] ?? key}</th><td>{values.join(' · ')}</td></tr>)}</tbody>
      </table>
    );
  }
  if (kind === 'pages') {
    const rows: [string, string][] = [['INSTR', DUMP.engines.map((e) => `${e.engine}: ` + e.pages.map((p) => p.tab).join(' ')).join(' · ')]];
    for (const [scene, pages] of Object.entries(DUMP.scenes)) rows.push([scene, pages.map((p) => p.tab).join(' · ')]);
    return (
      <table className="cz-ref cz-lists">
        <tbody>{rows.map(([a, b]) => <tr key={a}><th>{a}</th><td>{b}</td></tr>)}</tbody>
      </table>
    );
  }
  throw new Error(`cardenza: unknown reference ${kind}`);
}
