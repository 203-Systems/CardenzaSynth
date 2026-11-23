# Cardenza Learn

The Learn site for Cardenza Synth, built on [Docusaurus](https://docusaurus.io/) following the layout of 203 Systems' [Matrix wiki](https://github.com/203-Systems/Matrix-Wiki). It combines an interactive USB-connected guide with a multi-chapter reference manual in English and Simplified Chinese (`zh-Hans`). Running `make learn` outputs the site to `build/web/site/learn`, and `make web-deploy` places it at `/learn` alongside the web simulator.

| Path | Description |
|---|---|
| `docs/*.mdx` | English manual chapters (ordered by `sidebar_position`) |
| `i18n/zh-Hans/docusaurus-plugin-content-docs/current/*.mdx` | Simplified Chinese chapters (must match English section IDs) |
| `src/components/cardenza/` | UI components: keycaps, badges, annotated screenshots with LEDs, step strips, key maps, parameter tables, and interactive device/scene maps |
| `static/img/device/cardenza-pcb.svg` | Base PCB artwork used by the interactive device map (`board.tsx`) and LED overlays (`pcb.tsx`) |
| `src/theme/MDXComponents.tsx` | Registers custom MDX components globally so chapters do not need imports |
| `src/pages/index.tsx`, `src/guide/` | Interactive guide landing page and lesson pages (`plugins/guide-routes.js`, routed at `/guide/<lesson>/`) |
| `src/guide/device.ts`, `run.ts` | Persistent Web MIDI connection to a USB-attached Cardenza and lesson step runner |
| `src/theme/SearchBar/`, `src/search/` | Navbar search UI and engine, backed by synonyms in `docs/learn/synonyms.json` |
| `plugins/search-index.js` | Post-build plugin generating the bilingual search index |
| `generated/` | Build artifacts generated from firmware by `tools/wiki/generate.py` (gitignored) |

Lesson definitions live in `docs/learn/guide.json` (see `tools/wiki/lessons.py` for the schema), and the manual version is stored in `docs/learn/manual.json`.

## Production deployment

The `cardenza-learn` Vercel project (under `203nulls-projects`) connects directly to the public `203-Systems/CardenzaSynth` repository. Pushes to `main` deploy to https://cardenza.203.io, and branches/PRs receive preview deployments. No fork, deploy hook, or GitHub token is needed.

Vercel uses the root `vercel.json` and `tools/learn-build.sh` to install dependencies, compile `CardenzaSimShot` and `CardenzaLearnDump` from pinned public submodules, generate lesson data and screenshots, build both locales in Docusaurus, and run search regression tests. If any step fails, the previous production deployment stays live. Firmware CI runs independently.

`/deployment.json` and the search index both record the deployed Git commit. Use **Redeploy** in Vercel to re-trigger a build. Keep the Vercel project root set to the repository root, output directory set to `wiki/build`, and build command set to `bash tools/learn-build.sh`. Never commit credentials or `wiki/.env.local`.

## Local build

```bash
make learn
```

Or run manually using an existing host build containing `CardenzaSimShot` and `CardenzaLearnDump`:

```bash
python3 tools/wiki/generate.py --host-build build/linux-groove
```

```bash
cd wiki && npm ci && npm run build && npm run test:search
```

Run `npm run serve` to preview locally at http://localhost:3000/learn/ (manual at `/learn/manual/`, Chinese at `/learn/zh-Hans/`).

`generate.py` captures screenshots (`tools/wiki/shots.py`), runs every interactive lesson demo against the firmware (failing if any step cannot complete), and writes outputs to `generated/`: PNG screenshots and LED states, firmware parameter dumps (`tools/wiki/learn_dump.cpp`), processed lesson JSON, validated cross-locale section IDs, and `reference.html` (which redirects legacy `#section` links from the old single-page manual).

## Writing documentation

Manual chapters are written in MDX. Every `##` and `###` heading must have an explicit `{#id}` anchor matching between English and Chinese. Custom components are available globally without imports, such as `<Key k="FN+6" />`, `<Scene name="SEQ" />`, `<Led state="half" />`, `<Keywords k="volume, louder, 音量" />`, and:

```mdx
<Shot name="seq-notes">
  <Mark at="footer">ROLL view</Mark>
  <KeyMark keys="G1">Step with notes: half lit</KeyMark>
  <Caption>ROLL view with a few notes on T2.</Caption>
</Shot>
```

Add new screenshots via `shot(...)` in `tools/wiki/shots.py`. Missing screenshots, icons, keys, or section links fail the build. In Chinese docs, the product name is 鸣片. Lesson copy in `docs/learn/guide.json` supports inline tokens (`{{key:..}}`, `[[#section]]`, parsed by `src/guide/text.tsx`).

## Implementation notes

- **React 19**: Used instead of React 18 because React 18's streaming SSR can corrupt multibyte UTF-8 characters into NUL bytes at chunk boundaries (facebook/react#31134), affecting Chinese pages. `plugins/search-index.js` fails the build if any NUL bytes appear.
- **`trailingSlash: true`**: Outputs `manual/seq/index.html` so Vercel serves routes statically without `cleanUrls`.
- **Local `reference.html`**: `npm run serve` strips the base URL when serving `/learn/reference.html` (turning it into `/reference`), so open `/learn/reference#section` locally. Production on Vercel serves `/learn/reference.html` directly.
- **Older host builds**: Pass `--lenient-lessons` to `generate.py` to warn instead of fail on outdated firmware; `make learn` enforces strict checking.
