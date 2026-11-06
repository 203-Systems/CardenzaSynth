// The manual's search index for the search core (src/search/learn-search.js), its synonyms
// (docs/learn/synonyms.json) and its regression test (scripts/search.test.js): every h1 / h2 / h3 of a manual page is a section
// with its text (pictures' legends and captions included, the drawings left out) and the words of
// its <Keywords>. Built after each locale (postBuild); locales build one after the other, the
// default first, so each adds its sections to the index of the ones before. Written at the site's
// root (the English search bar and scripts/search.test.js read it there) and next to each other locale.
const fs = require('fs');
const path = require('path');
const {execSync} = require('child_process');

const ENTITIES = {amp: '&', lt: '<', gt: '>', quot: '"', apos: "'", nbsp: ' ', '#39': "'", '#x27': "'"};

function decode(text) {
  return text.replace(/&(#x?[0-9a-f]+|\w+);/gi, (all, name) => {
    if (ENTITIES[name.toLowerCase()] !== undefined) return ENTITIES[name.toLowerCase()];
    if (name[0] === '#') return String.fromCodePoint(name[1].toLowerCase() === 'x' ? parseInt(name.slice(2), 16) : parseInt(name.slice(1), 10));
    return all;
  });
}

function plain(html) {
  return decode(html.replace(/<svg[\s\S]*?<\/svg>/g, ' ').replace(/<[^>]+>/g, ' ')).replace(/​/g, '').replace(/\s+/g, ' ').trim();
}

function pageFile(outDir, relative) {
  for (const candidate of [path.join(outDir, relative + '.html'), path.join(outDir, relative, 'index.html')]) {
    if (fs.existsSync(candidate)) return candidate;
  }
  return null;
}

/** The sections of one manual page. */
function pageSections(html, route, chapter, lang) {
  const start = html.indexOf('theme-doc-markdown');
  if (start < 0) return [];
  const end = html.indexOf('<footer', start);
  // Drawings and the pictures' own labels (legend numbers, "key lights at that moment") are not text.
  const body = html.slice(html.indexOf('>', start) + 1, end < 0 ? undefined : end)
    .replace(/<svg[\s\S]*?<\/svg>/g, ' ').replace(/<span[^>]*data-nosearch[^>]*>[\s\S]*?<\/span>/g, ' ');
  const heading = /<h([123])\b([^>]*)>([\s\S]*?)<\/h\1>/g;
  const found = [];
  let m;
  while ((m = heading.exec(body))) {
    const id = /\bid="([^"]+)"/.exec(m[2]);
    found.push({level: +m[1], id: m[1] === '1' ? chapter : id && id[1], title: plain(m[3]), start: m.index, end: heading.lastIndex});
  }
  const sections = [];
  let chapterTitle = '';
  let parent = '';
  found.forEach((h, i) => {
    if (!h.id) return;
    const segment = body.slice(h.end, i + 1 < found.length ? found[i + 1].start : undefined);
    const keywords = [];
    for (const k of segment.matchAll(/data-keywords="([^"]*)"/g)) {
      keywords.push(...decode(k[1]).split(',').map((w) => w.trim()).filter(Boolean));
    }
    if (h.level === 1) chapterTitle = h.title;
    if (h.level <= 2) parent = h.title;
    sections.push({
      id: h.id, lang, title: h.title,
      path: chapterTitle + (h.level === 1 ? '' : h.level === 3 && parent !== h.title ? ' › ' + parent : ''),
      level: h.level, text: plain(segment), keywords,
      url: route + (h.level === 1 ? '' : '#' + h.id),
    });
  });
  return sections;
}

module.exports = function searchIndex(context) {
  return {
    name: 'cardenza-search-index',
    async postBuild({outDir, routesPaths, baseUrl}) {
      const {i18n, siteDir} = context;
      const lang = i18n.currentLocale.startsWith('zh') ? 'zh' : 'en';
      const prefix = baseUrl + 'manual/';
      const sections = [];
      for (const route of routesPaths.filter((r) => r.startsWith(prefix))) {
        const file = pageFile(outDir, route.slice(baseUrl.length));
        if (!file) continue;
        const html = fs.readFileSync(file, 'utf8');
        // React 18's streaming render broke multibyte characters at chunk boundaries into NULs
        // (facebook/react#31134): browsers drop them from the text but show them in attributes.
        if (html.includes('\u0000')) throw new Error(`search-index: ${route} has NUL characters (React 18 SSR?)`);
        sections.push(...pageSections(html, route, route.slice(prefix.length).replace(/\/$/, ''), lang));
      }
      if (!sections.length) throw new Error('search-index: no manual sections found in the build');
      const ids = sections.map((s) => s.id);
      const twice = ids.filter((id, i) => ids.indexOf(id) !== i);
      if (twice.length) throw new Error(`search-index: [${lang}] section ids on more than one page: ${[...new Set(twice)]}`);

      const root = i18n.currentLocale === i18n.defaultLocale ? outDir : path.dirname(outDir);
      const rootFile = path.join(root, 'search-index.json');
      let index;
      if (i18n.currentLocale === i18n.defaultLocale || !fs.existsSync(rootFile)) {
        const generated = path.join(siteDir, 'generated', 'data');
        const synonyms = JSON.parse(fs.readFileSync(path.join(generated, 'synonyms.json'), 'utf8'));
        const manual = JSON.parse(fs.readFileSync(path.join(generated, 'manual.json'), 'utf8'));
        let build = process.env.CARDENZA_LEARN_COMMIT?.slice(0, 7) || '';
        if (!build) {
          try { build = execSync('git rev-parse --short HEAD', {cwd: siteDir}).toString().trim(); } catch (e) { /* not a checkout */ }
        }
        index = {version: manual.version, build, date: new Date().toISOString().slice(0, 10), sections: [],
          synonyms: synonyms.groups, stopwords: synonyms.stopwords || []};
      } else {
        index = JSON.parse(fs.readFileSync(rootFile, 'utf8'));
      }
      index.sections = index.sections.filter((s) => s.lang !== lang).concat(sections);
      const json = JSON.stringify(index);
      fs.writeFileSync(rootFile, json);
      if (root !== outDir) fs.writeFileSync(path.join(outDir, 'search-index.json'), json);
      console.log(`[search-index] ${lang}: ${sections.length} sections (${index.sections.length} in the index)`);
    },
  };
};
