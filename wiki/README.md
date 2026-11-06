# Cardenza Learn

The Learn site on [Docusaurus](https://docusaurus.io/), set up like 203 Systems'
[Matrix wiki](https://github.com/203-Systems/Matrix-Wiki): the guide (short lessons a Cardenza on
USB follows) and the manual (one page per chapter with a sidebar), English with a zh-Hans
translation, the 203 navbar and footer. `make learn` builds it into `build/web/site/learn`;
`make web-deploy` puts it at `/learn` next to the web simulator.

| | |
|---|---|
| `docs/*.mdx` | the manual's chapters in English (their order: `sidebar_position`) |
| `i18n/zh-Hans/docusaurus-plugin-content-docs/current/*.mdx` | the same chapters in Chinese, the same section ids |
| `src/components/cardenza/` | key caps, chips, screenshots with key lights, step strips, key maps, reference tables, device and scene maps |
| `src/theme/MDXComponents.tsx` | makes them usable on every page without an import |
| `src/pages/index.tsx`, `src/guide/` | the guide: its home and a page per lesson (`plugins/guide-routes.js`, `/guide/<lesson>/`) |
| `src/guide/device.ts`, `run.ts` | the Cardenza on USB (Web MIDI, one connection for the whole site) and a device lesson as it runs |
| `src/theme/SearchBar/`, `src/search/` | the navbar search: its core, the synonyms of `docs/learn/synonyms.json` |
| `plugins/search-index.js` | the search index, built from the pages (both languages) |
| `generated/` | not in git: what `tools/wiki/generate.py` draws from the firmware |

The guide's lessons are `docs/learn/guide.json` (see `tools/wiki/lessons.py` for its fields); the
manual's version is `docs/learn/manual.json`.

## Build

```bash
make learn
```

or by hand, with a host build that has `CardenzaSimShot` and `CardenzaLearnDump`:

```bash
python3 tools/wiki/generate.py --host-build build/linux-groove
```

```bash
cd wiki && npm ci && npm run build && npm run test:search
```

`npm run serve` then shows it on http://localhost:3000/learn/ (the guide; the manual is under
`/learn/manual/`, in Chinese under `/learn/zh-Hans/`).

`generate.py` runs the screenshot script (`tools/wiki/shots.py`) and every device lesson's demos on
the firmware (a step that does not complete fails it) and puts the results in `generated/`:
screenshots and key lights, the firmware's reference dump (`tools/wiki/learn_dump.cpp`), the lessons
as data, the section ids of both languages (which must match), and `reference.html`, which sends links
of the one-page manual of before (`reference.html#section`) to that section.

## Writing

The chapters are MDX. Every `##` and `###` has its id (`{#id}`), the same in both languages; the
components are written in place, for example `<Key k="FN+6" />`, `<Scene name="SEQ" />`,
`<Led state="half" />`, `<Keywords k="volume, louder, 音量" />` and

```mdx
<Shot name="seq-notes">
  <Mark at="footer">ROLL view</Mark>
  <KeyMark keys="G1">Has notes: half lit</KeyMark>
  <Caption>ROLL: a few notes on T2.</Caption>
</Shot>
```

A new screenshot is a `shot(...)` in `tools/wiki/shots.py`. A screenshot, icon, key or section that
does not exist fails the build. In Chinese the product is 鸣片. The lessons' words in
`docs/learn/guide.json` keep the inline markup the pages render (`{{key:..}}`, `[[#section]]`,
`src/guide/text.tsx`).

## Notes

- React 19, not the Matrix wiki's React 18: React 18's streaming server render breaks multibyte
  characters at chunk boundaries into NULs (facebook/react#31134), dozens of them on the Chinese
  pages. `plugins/search-index.js` fails the build if one comes back.
- `trailingSlash: true` (pages are `manual/seq/index.html`): the Vercel project serves files as
  they are, without `cleanUrls`.
- Locally `npm run serve` turns `/learn/reference.html` into `/reference` (it drops the base URL);
  open `/learn/reference#section` there. Vercel serves the file as it is.
- On a host build older than the guide's lessons, `generate.py --lenient-lessons` warns instead of
  failing; `make learn` keeps failing on them.
