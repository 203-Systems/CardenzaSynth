// The navbar search: the manual's search core (src/search/learn-search.js) over the index
// plugins/search-index.js builds (sections of both languages, the synonyms of
// docs/learn/synonyms.json), showing the reader's language. "/" focuses it.
import React, {useCallback, useEffect, useRef, useState} from 'react';
import Link from '@docusaurus/Link';
import {useHistory} from '@docusaurus/router';
import useBaseUrl from '@docusaurus/useBaseUrl';
import {useLang, say} from '@site/src/components/cardenza/common';
// A UMD script, shared with scripts/search.test.js.
import LearnSearch from '@site/src/search/learn-search.js';
import styles from './styles.module.css';

type Snippet = {text: string; offset: number; marks: number[][]};
type Result = {id: string; title: string; path: string; snippet: Snippet; url?: string};

function Highlight({snippet}: {snippet: Snippet}) {
  const out: React.ReactNode[] = [];
  let at = 0;
  snippet.marks.forEach(([start, end], i) => {
    const s = start + snippet.offset, e = end + snippet.offset;
    if (s < at) return;
    out.push(snippet.text.slice(at, s), <mark key={i}>{snippet.text.slice(s, e)}</mark>);
    at = e;
  });
  out.push(snippet.text.slice(at));
  return <>{out}</>;
}

let loading: Promise<unknown> | null = null;

export default function SearchBar(): React.ReactElement {
  const lang = useLang();
  const history = useHistory();
  const indexUrl = useBaseUrl('/search-index.json');
  const [index, setIndex] = useState<unknown>(null);
  const [failed, setFailed] = useState(false);
  const [query, setQuery] = useState('');
  const [open, setOpen] = useState(false);
  const [selected, setSelected] = useState(0);
  const input = useRef<HTMLInputElement>(null);
  const box = useRef<HTMLDivElement>(null);

  const load = useCallback(() => {
    if (index || failed) return;
    loading = loading || fetch(indexUrl).then((r) => {
      if (!r.ok) throw new Error(String(r.status));
      return r.json();
    }).then((json) => LearnSearch.createIndex(json));
    loading.then(setIndex, () => setFailed(true));
  }, [index, failed, indexUrl]);

  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      const typing = /^(INPUT|TEXTAREA|SELECT)$/.test((document.activeElement as HTMLElement)?.tagName || '');
      if (!typing && event.key === '/' && !event.metaKey && !event.ctrlKey && !event.altKey) {
        event.preventDefault();
        input.current?.focus();
      }
    };
    const onClick = (event: MouseEvent) => {
      if (box.current && !box.current.contains(event.target as Node)) setOpen(false);
    };
    document.addEventListener('keydown', onKey);
    document.addEventListener('click', onClick);
    return () => {
      document.removeEventListener('keydown', onKey);
      document.removeEventListener('click', onClick);
    };
  }, []);

  // The core returns ids; the page of each comes from the index (plugins/search-index.js url).
  const sections = (index as {sections?: {id: string; lang: string; url?: string}[]} | null)?.sections || [];
  const results: Result[] = (index && query.trim() ? LearnSearch.search(index, query, {lang, limit: 10}) : [])
    .map((r: Omit<Result, 'url'>) => ({...r, url: sections.find((s) => s.id === r.id && s.lang === lang)?.url}));
  const go = (r: Result) => {
    setOpen(false);
    setQuery('');
    if (r.url) history.push(r.url);
  };

  return (
    <div className={styles.search} ref={box} role="search">
      <input
        ref={input}
        className={styles.input}
        type="search"
        value={query}
        placeholder={say(lang, '搜索：音量、录音、BPM …  ( / )', 'Search: volume, record, BPM …  ( / )')}
        aria-label={say(lang, '搜索说明书', 'Search the manual')}
        onFocus={() => { load(); setOpen(true); }}
        onChange={(e) => { setQuery(e.target.value); setSelected(0); setOpen(true); load(); }}
        onKeyDown={(e) => {
          if ((e.key === 'ArrowDown' || e.key === 'ArrowUp') && results.length) {
            e.preventDefault();
            setSelected((selected + (e.key === 'ArrowDown' ? 1 : -1) + results.length) % results.length);
          } else if (e.key === 'Enter' && results[selected]) {
            e.preventDefault();
            go(results[selected]);
            input.current?.blur();
          } else if (e.key === 'Escape') {
            setQuery('');
            setOpen(false);
            input.current?.blur();
          }
        }}
      />
      {open && query.trim() && (
        <div className={styles.results} role="listbox">
          {failed ? (
            <div className={styles.empty}>{say(lang, '搜索要在构建后的站点里用（npm run build）。', 'Search works on a built site (npm run build).')}</div>
          ) : !index ? (
            <div className={styles.empty}>…</div>
          ) : !results.length ? (
            <div className={styles.empty}>{say(lang, '没有找到。试试别的说法。', 'Nothing found. Try other words.')}</div>
          ) : (
            <>
              <div className={styles.head}><span>{say(lang, '说明书', 'Manual')}</span><span>{say(lang, '↑↓ 选择 · Enter 打开', '↑↓ choose · Enter open')}</span></div>
              {results.map((r, i) => (
                <Link key={r.id} to={r.url || '#'} autoAddBaseUrl={false} role="option" aria-selected={i === selected}
                  className={styles.result} onMouseEnter={() => setSelected(i)} onClick={() => { setOpen(false); setQuery(''); }}>
                  <div className={styles.title}>{r.title}</div>
                  <div className={styles.path}>{r.path}</div>
                  <div className={styles.snippet}><Highlight snippet={r.snippet} /></div>
                </Link>
              ))}
            </>
          )}
        </div>
      )}
    </div>
  );
}
