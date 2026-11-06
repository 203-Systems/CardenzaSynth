// The lessons' words (docs/learn/guide.json) keep the one-page manual's inline markup: `code`,
// [[#section]] / [[#section|text]], {{key:..}} {{hold:..}} {{icon:..}} {{led:state|label}}
// {{scene:..}} {{track:..}} {{page:..}}, **bold**, *em*, [text](url). Rendered with the manual's chips.
import React from 'react';
import Link from '@docusaurus/Link';
import sections from '@site/generated/data/sections.json';
import {Key, Led, Page, Scene, Track} from '@site/src/components/cardenza/chips';
import {Icon, useLang, useUrls, type Lang} from '@site/src/components/cardenza/common';

const INLINE = /`([^`]+)`|\[\[#([\w-]+)(?:\|([^\]]+))?\]\]|\{\{(key|hold|icon|led|scene|track|page):([^}]+)\}\}|\*\*(.+?)\*\*|\*([^*\s][^*]*?)\*|\[([^\]]+)\]\(([^)]+)\)/g;
const TITLES = sections as Record<string, {chapter: string; title: Record<Lang, string>}>;

function chip(kind: string, arg: string, key: number): React.ReactNode {
  const value = arg.trim();
  switch (kind) {
    case 'key': return <Key key={key} k={value} />;
    case 'hold': return <Key key={key} k={'hold ' + value} />;
    case 'icon': return <Icon key={key} name={value} />;
    case 'led': {
      const [state, ...label] = value.split('|');
      return <Led key={key} state={state}>{label.join('|')}</Led>;
    }
    case 'scene': return <Scene key={key} name={value.toUpperCase()} />;
    case 'track': return <Track key={key} n={value} />;
    default: return <Page key={key} name={value} />;
  }
}

function render(text: string, lang: Lang, sectionPath: (id: string) => string): React.ReactNode[] {
  const out: React.ReactNode[] = [];
  let pos = 0;
  let n = 0;
  for (const m of text.matchAll(INLINE)) {
    out.push(text.slice(pos, m.index));
    const key = n++;
    if (m[1] !== undefined) out.push(<code key={key}>{m[1]}</code>);
    else if (m[2] !== undefined) {
      const target = TITLES[m[2]];
      if (!target) throw new Error(`cardenza: guide links to unknown section #${m[2]}`);
      out.push(<Link key={key} to={sectionPath(m[2])}>{m[3] ? render(m[3], lang, sectionPath) : target.title[lang]}</Link>);
    } else if (m[4] !== undefined) out.push(chip(m[4], m[5], key));
    else if (m[6] !== undefined) out.push(<strong key={key}>{render(m[6], lang, sectionPath)}</strong>);
    else if (m[7] !== undefined) out.push(<em key={key}>{render(m[7], lang, sectionPath)}</em>);
    else {
      const href = m[9];
      const label = render(m[8], lang, sectionPath);
      out.push(href.startsWith('http')
        ? <a key={key} href={href} target="_blank" rel="noopener noreferrer">{label}</a>
        : <Link key={key} to={href === './' ? '/' : href}>{label}</Link>);
    }
    pos = (m.index ?? 0) + m[0].length;
  }
  out.push(text.slice(pos));
  return out;
}

/** A lesson's words in the reader's language, markup rendered. */
export function GuideText({words}: {words: {zh: string; en: string}}) {
  const lang = useLang();
  const urls = useUrls();
  return <>{render(words[lang], lang, urls.sectionPath)}</>;
}

/** Plain words in the reader's language. */
export function useSay() {
  const lang = useLang();
  return (zh: string, en: string) => (lang === 'zh' ? zh : en);
}
