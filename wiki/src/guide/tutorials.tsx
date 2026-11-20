// Firmware tutorials (the user, 2026-11-20: "我们cardenza的教程页也连接到相应固件的教程页按键跳转"): every firmware on the
// 203 community has a 教程 / Tutorials tab, in the format of this guide. The home page lists the Cardenza's firmware that
// have tutorials, each with a button to its tab; the list comes from the community's public API in the browser
// (GET /community/api/tutorials?target=cardenza: COMMUNITY.md "Tutorials"), never at build time. Until it answers, when
// it fails or lists nothing, the section still has its two links: Cardenza Synth's tutorials and all of them.
import React, {useEffect, useState} from 'react';
import {useLang} from '@site/src/components/cardenza/common';
import {useSay} from './text';

export const COMMUNITY = 'https://cardenza.203.io/community';
/** Cardenza Synth, the firmware these lessons teach (its community code). */
export const SYNTH_CODE = 'V6QK28';
export const tutorialsUrl = (code?: string) => (code ? `${COMMUNITY}/${code}/tutorials` : `${COMMUNITY}/tutorials`);

type Picture = {url: string} | null | undefined;
type FirmwareRef = {
  code: string; name: string; official?: boolean;
  cover?: Picture; inherited_cover?: Picture; screen?: (Picture & {width?: number; height?: number}) | null;
};
type TutorialRef = {id: string; kind: 'guide' | 'link'; title: string; official?: boolean; lessons?: number};
type Entry = {firmware: FirmwareRef; total: number; tutorials: TutorialRef[]};

// On cardenza.203.io the community is the same site (/community); anywhere else (a preview, localhost) its full address.
function apiBase(): string {
  return typeof window !== 'undefined' && window.location.hostname === 'cardenza.203.io' ? '/community/api' : `${COMMUNITY}/api`;
}

// A picture's address as the API gives it (absolute), or one on the community's own site.
function pictureUrl(url: string): string {
  return /^https:\/\//.test(url) ? url : url.startsWith('/') ? `https://cardenza.203.io${url}` : '';
}

function entriesOf(data: unknown): Entry[] {
  const items = (data as {items?: unknown})?.items;
  if (!Array.isArray(items)) return [];
  return items.filter((item): item is Entry => {
    const f = (item as Entry)?.firmware;
    return !!f && typeof f.code === 'string' && /^[2-9A-HJ-NP-Z]{6}$/.test(f.code) && typeof f.name === 'string';
  });
}

/** The firmware that have tutorials, as the community lists them (null until it answers; [] when it could not). */
function useFirmwareTutorials(lang: string): Entry[] | null {
  const [entries, setEntries] = useState<Entry[] | null>(null);
  useEffect(() => {
    const ctrl = new AbortController();
    const timer = window.setTimeout(() => ctrl.abort(), 8000);
    fetch(`${apiBase()}/tutorials?target=cardenza&lang=${lang}`, {signal: ctrl.signal, credentials: 'omit', headers: {Accept: 'application/json'}})
      .then((r) => (r.ok ? r.json() : null))
      .then((data) => setEntries(entriesOf(data)))
      .catch(() => setEntries([]))
      .finally(() => window.clearTimeout(timer));
    return () => {
      window.clearTimeout(timer);
      ctrl.abort();
    };
  }, [lang]);
  return entries;
}

function Cover({firmware}: {firmware: FirmwareRef}) {
  const own = firmware.cover?.url || firmware.inherited_cover?.url;
  if (own) return <div className="c-art tf-art"><img src={pictureUrl(own)} alt="" loading="lazy" /></div>;
  const screen = firmware.screen?.url;
  if (screen) return <div className="c-art tf-art tf-screen"><img src={pictureUrl(screen)} alt="" loading="lazy" /></div>;
  return <div className="c-art tf-art tf-plain"><span>{firmware.name}</span></div>;
}

function Card({entry}: {entry: Entry}) {
  const say = useSay();
  const {firmware, total} = entry;
  const first = entry.tutorials.find((t) => t.official) ?? entry.tutorials[0];
  return (
    <a className="g-card tf-card" href={tutorialsUrl(firmware.code)} target="_self">
      <Cover firmware={firmware} />
      <div className="c-text">
        <b>{firmware.name}</b>
        {first && <small>{first.official ? say('官方 · ', 'Official · ') : ''}{first.title}</small>}
        <span className="tf-go">
          <span className="tf-count">{say(`${total} 个教程`, total === 1 ? '1 tutorial' : `${total} tutorials`)}</span>
          <span className="tf-btn">{say('看教程', 'Tutorials')} →</span>
        </span>
      </div>
    </a>
  );
}

/** The home page's section: the firmware with tutorials on the 203 community, and the links to them. */
export function FirmwareTutorials() {
  const lang = useLang();
  const say = useSay();
  const entries = useFirmwareTutorials(lang);
  const shown = (entries ?? []).slice(0, 8);
  return (
    <section className="cards-part g-tutorials" aria-labelledby="g-tutorials-h">
      <h2 id="g-tutorials-h">{say('固件教程', 'Firmware tutorials')}</h2>
      <p className="tf-lead">
        {say('鸣片上的其他固件也有自己的教程，写法和这里的课一样：在 203 社区每个固件页面的「教程」里。',
          'Other firmware for the Cardenza have tutorials of their own, written like these lessons: on the Tutorials tab of each firmware on the 203 community.')}
      </p>
      {shown.length > 0 && (
        <div className="g-cards tf-cards">
          {shown.map((entry) => <Card key={entry.firmware.code} entry={entry} />)}
        </div>
      )}
      <div className="tf-links">
        <a className="tf-link" href={tutorialsUrl(SYNTH_CODE)} target="_self">{say('Cardenza Synth 的教程', 'Cardenza Synth tutorials')} →</a>
        <a className="tf-link" href={tutorialsUrl()} target="_self">
          {say('全部固件教程', 'All firmware tutorials')}{entries && entries.length > shown.length ? ` (${entries.length})` : ''} →
        </a>
      </div>
    </section>
  );
}
