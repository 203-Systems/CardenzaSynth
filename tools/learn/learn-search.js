// Learn manual search: one core for the page (window.LearnSearch) and the /api/search function
// (require). The index (search-index.json) holds every section of both languages and the synonym
// groups from docs/learn/synonyms.json, so a query finds a section by any word that means the same,
// in either language: "音量", "volume", "louder", "声音太小" all lead to the volume sections.
(function (root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory();
  else root.LearnSearch = factory();
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  const CJK = /[㐀-鿿豈-﫿]/;
  const WORD = /[a-z0-9+#'./]+(?:-[a-z0-9+#'./]+)*|[㐀-鿿豈-﫿]+/g;

  function norm(text) {
    return String(text || '').normalize('NFKC').toLowerCase().replace(/[‐-―−]/g, '-');
  }

  function words(text) {
    return norm(text).match(WORD) || [];
  }

  // One edit (two for long words) between two latin words: typos still find their section.
  function close(a, b) {
    if (a === b) return true;
    const limit = Math.max(a.length, b.length) >= 8 ? 2 : 1;
    if (Math.abs(a.length - b.length) > limit) return false;
    const previous = new Array(b.length + 1);
    for (let j = 0; j <= b.length; j++) previous[j] = j;
    for (let i = 1; i <= a.length; i++) {
      let diagonal = previous[0];
      previous[0] = i;
      let rowMin = previous[0];
      for (let j = 1; j <= b.length; j++) {
        const above = previous[j];
        previous[j] = Math.min(previous[j] + 1, previous[j - 1] + 1, diagonal + (a[i - 1] === b[j - 1] ? 0 : 1));
        diagonal = above;
        rowMin = Math.min(rowMin, previous[j]);
      }
      if (rowMin > limit) return false;
    }
    return previous[b.length] <= limit;
  }

  function stems(term) {
    if (CJK.test(term) || term.length < 4) return [];
    const out = [];
    if (term.endsWith('ies')) out.push(term.slice(0, -3) + 'y');
    if (term.endsWith('es')) out.push(term.slice(0, -2));
    if (term.endsWith('s') && !term.endsWith('ss')) out.push(term.slice(0, -1));
    if (term.endsWith('ing') && term.length > 5) out.push(term.slice(0, -3), term.slice(0, -3) + 'e');
    if (term.endsWith('ed') && term.length > 4) out.push(term.slice(0, -2), term.slice(0, -1));
    return out;
  }

  function createIndex(data) {
    const groups = (data.synonyms || []).map((group) => group.map(norm).map((t) => t.trim()).filter(Boolean));
    const termGroups = new Map();
    groups.forEach((group, gi) => group.forEach((term) => {
      if (!termGroups.has(term)) termGroups.set(term, []);
      termGroups.get(term).push(gi);
    }));
    const terms = [...termGroups.keys()];
    return {
      version: data.version,
      groups,
      termGroups,
      // Terms that can hide inside a longer run: Chinese words, and multi-word phrases.
      cjkTerms: terms.filter((t) => CJK.test(t)).sort((a, b) => b.length - a.length),
      phrases: terms.filter((t) => !CJK.test(t) && /\s/.test(t)).sort((a, b) => b.length - a.length),
      stop: new Set((data.stopwords || []).map(norm)),
      sections: (data.sections || []).map((s) => {
        const keywords = (s.keywords || []).join(' ');
        return Object.assign({}, s, {
          nTitle: norm(s.title),
          nPath: norm(s.path),
          nText: norm(s.text),
          nKeys: norm(keywords),
          keyList: (s.keywords || []).map((k) => norm(k).trim()),
          nearWords: words(s.title + ' ' + keywords).filter((w) => !CJK.test(w) && w.length >= 3),
        });
      }),
    };
  }

  function expand(index, term) {
    const list = [{ t: term, w: 1 }];
    const seen = new Set([term]);
    const push = (t, w) => {
      if (!t || seen.has(t)) return;
      seen.add(t);
      list.push({ t, w });
    };
    const addGroups = (key) => (index.termGroups.get(key) || []).forEach((gi) => index.groups[gi].forEach((t) => push(t, 0.85)));
    addGroups(term);
    for (const stem of stems(term)) {
      push(stem, 0.95);
      addGroups(stem);
    }
    return { term, terms: list };
  }

  // A query becomes concepts: each typed word (or known phrase) with everything that means the same.
  function parse(index, query) {
    let q = ' ' + norm(query).trim() + ' ';
    if (!q.trim()) return [];
    const concepts = [];
    for (const phrase of index.phrases) {
      if (q.includes(' ' + phrase + ' ')) {
        concepts.push(expand(index, phrase));
        q = q.split(phrase).join(' ');
      }
    }
    for (const token of q.match(WORD) || []) {
      if (index.stop.has(token)) continue;
      if (!CJK.test(token)) {
        concepts.push(expand(index, token));
        continue;
      }
      // A run of Chinese: cut out every known word, longest first; drop filler words; what is left
      // (two characters or more) is searched as typed.
      let rest = token;
      const found = [];
      for (const term of index.cjkTerms) {
        if (rest.includes(term)) {
          found.push(term);
          rest = rest.split(term).join(' ');
        }
      }
      for (let piece of rest.split(/\s+/)) {
        for (const filler of index.stop) if (CJK.test(filler)) piece = piece.split(filler).join(' ');
        for (const bit of piece.split(/\s+/)) if (bit.length >= 2 || (bit.length === 1 && token.length === 1)) found.push(bit);
      }
      for (const term of found) concepts.push(expand(index, term));
    }
    return concepts;
  }

  // Latin terms match whole words (plus plural and -ed / -ing endings): "tap" is not in "tape".
  // Chinese has no spaces, so it matches anywhere.
  const wordCache = new Map();
  function wordPattern(term) {
    let pattern = wordCache.get(term);
    if (!pattern) {
      const escaped = term.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
      pattern = new RegExp('(^|[^a-z0-9])' + escaped + '(s|es|ed|ing)?(?=[^a-z0-9]|$)', 'g');
      wordCache.set(term, pattern);
    }
    pattern.lastIndex = 0;
    return pattern;
  }

  function count(haystack, needle, max) {
    if (!CJK.test(needle)) {
      const pattern = wordPattern(needle);
      let n = 0;
      while (n < max && pattern.exec(haystack)) n++;
      return n;
    }
    let n = 0;
    let at = haystack.indexOf(needle);
    while (at >= 0 && n < max) {
      n++;
      at = haystack.indexOf(needle, at + needle.length);
    }
    return n;
  }

  function has(haystack, needle) {
    return count(haystack, needle, 1) > 0;
  }

  function scoreConcept(section, concept) {
    let best = 0;
    let hit = null;
    for (const { t, w } of concept.terms) {
      let s = 0;
      // The author's own keywords for a section count most, then its title.
      if (section.nTitle === t) s = 30;
      else if (section.keyList.includes(t)) s = 24;
      else if (section.nTitle.startsWith(t) && has(section.nTitle, t)) s = 20;
      else if (has(section.nTitle, t)) s = 18;
      if (has(section.nKeys, t)) s = Math.max(s, 13);
      if (has(section.nPath, t)) s = Math.max(s, 5);
      const n = count(section.nText, t, 5);
      if (n) s = Math.max(s, 3 + n * 1.6);
      if (!s && t.length >= 4 && !CJK.test(t) && section.nearWords.some((word) => close(word, t))) s = 7;
      // A keyword that is also in the title: this section is about it.
      if (section.keyList.includes(t) && has(section.nTitle, t)) s += 6;
      if (s * w > best) {
        best = s * w;
        hit = t;
      }
    }
    return { score: best, hit };
  }

  function snippet(section, hits) {
    const text = section.text || '';
    const lower = norm(text);
    let at = -1;
    for (const hit of hits) {
      const i = lower.indexOf(hit);
      if (i >= 0 && (at < 0 || i < at)) at = i;
    }
    const start = Math.max(0, at < 0 ? 0 : at - 30);
    const piece = text.slice(start, start + 140);
    const marks = [];
    const pieceLower = norm(piece);
    for (const hit of hits) {
      let i = pieceLower.indexOf(hit);
      while (i >= 0) {
        marks.push([i, i + hit.length]);
        i = pieceLower.indexOf(hit, i + hit.length);
      }
    }
    marks.sort((a, b) => a[0] - b[0]);
    return { text: (start > 0 ? '…' : '') + piece + (start + 140 < text.length ? '…' : ''), offset: start > 0 ? 1 : 0, marks };
  }

  function search(index, query, options) {
    const opts = options || {};
    const limit = opts.limit || 12;
    const concepts = parse(index, query);
    if (!concepts.length) return [];
    const scored = [];
    for (const section of index.sections) {
      if (opts.lang && section.lang !== opts.lang) continue;
      let total = 0;
      let matched = 0;
      const hits = [];
      for (const concept of concepts) {
        const { score, hit } = scoreConcept(section, concept);
        if (score > 0) {
          matched++;
          total += score;
          if (hit) hits.push(hit);
        }
      }
      if (!matched) continue;
      const all = matched === concepts.length;
      // Chapters and sections before sub-sections when they score alike.
      const depth = section.level === 1 ? 1.5 : section.level === 2 ? 1 : 0;
      scored.push({ section, all, hits, score: total * (all ? 1 : 0.4) + depth });
    }
    const anyAll = scored.some((r) => r.all);
    return scored
      .filter((r) => !anyAll || r.all)
      .sort((a, b) => b.score - a.score)
      .slice(0, limit)
      .map((r) => ({
        id: r.section.id,
        lang: r.section.lang,
        title: r.section.title,
        path: r.section.path,
        level: r.section.level,
        score: Math.round(r.score * 10) / 10,
        snippet: snippet(r.section, r.hits),
      }));
  }

  return { createIndex, parse, search, norm };
});
