# Cardenza wiki (prototype)

The Learn manual on [Docusaurus](https://docusaurus.io/), set up like 203 Systems'
[Matrix wiki](https://github.com/203-Systems/Matrix-Wiki): one page per chapter with a sidebar,
English with a zh-Hans translation, the 203 navbar and footer. It is a prototype next to
`docs/learn` + `tools/learn` (`make learn`), which stay the source until the wiki takes over.

| | |
|---|---|
| `docs/*.mdx` | the chapters in English |
| `i18n/zh-Hans/docusaurus-plugin-content-docs/current/*.mdx` | the same chapters in Chinese (same section ids) |
| `src/components/cardenza/` | key caps, chips, screenshots with key lights, step strips, key maps, reference tables, device and scene maps |
| `src/theme/MDXComponents.tsx` | makes them usable on every page without an import |
| `src/theme/SearchBar/` | the navbar search: the old manual's search core and synonyms |
| `plugins/search-index.js` | builds the search index from the built pages (old format, both languages) |
| `src/pages/index.tsx`, `src/guide/` | the guide: the home page and a page per lesson (`plugins/guide-routes.js`, `/guide/<lesson>/`) |
| `src/guide/device.ts`, `run.ts` | the Cardenza on USB (Web MIDI, one connection for the whole site) and a device lesson as it runs: a port of `tools/learn/guide.js` with its messages, timings and conditions |
| `generated/` | not in git: what `tools/wiki/generate.py` draws from the firmware |

## Build

```bash
python tools/wiki/generate.py --host-build build/linux-groove
```

```bash
cd wiki && npm ci && npm run build && npm run test:search
```

`npm run serve` then shows it on http://localhost:3000/learn/ (the guide; the manual is under
`/learn/manual/`, in Chinese under `/learn/zh-Hans/manual/`).

`generate.py` runs the same screenshot script and the same guide lessons as `make learn` (a step
that does not complete on the firmware fails it) and puts the results in `generated/`: screenshots
and key lights, the firmware's reference dump, the lessons as data (`docs/learn/guide.json` with
keys and demos resolved), and `reference.html`, which sends links of the one-page manual
(`reference.html#section`) to the chapter page of that section. The lessons' words keep the manual's
inline markup (`{{key:..}}`, `[[#id]]`), rendered by `src/guide/text.tsx`; the concept lessons'
animations are `tools/learn/guide-anim.js` as it is.

## Writing

The chapters are MDX. The old `{{...}}` syntax became components (`tools/wiki/convert.py` did the
move and lists them): `<Key k="FN+6" />`, `<Scene name="SEQ" />`, `<Led state="half" />`,

```mdx
<Shot name="seq-notes">
  <Mark at="footer">ROLL view</Mark>
  <KeyMark keys="G1">Has notes: half lit</KeyMark>
  <Caption>ROLL: a few notes on T2.</Caption>
</Shot>
```

A screenshot, icon, key or section that does not exist fails the build, as `make learn` did.

## Notes

- React 19, not the Matrix wiki's React 18: React 18's streaming server render breaks multibyte
  characters at chunk boundaries into NULs (facebook/react#31134), dozens of them on the Chinese
  pages. `plugins/search-index.js` fails the build if one comes back.
- `trailingSlash: true` (pages are `manual/seq/index.html`): the Vercel project serves files as
  they are, without `cleanUrls`.
- Locally `npm run serve` turns `/learn/reference.html` into `/reference` (it drops the base URL);
  open `/learn/reference#section` there. Vercel serves the file as it is.
- On a host build older than the guide's lessons, `generate.py --lenient-lessons` warns instead of
  failing; `make learn` and a release keep failing on them.
