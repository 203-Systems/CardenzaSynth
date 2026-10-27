// Cardenza Learn page: language switch, navigation by hash (#section-id, the same in both
// languages), table of contents that follows the reading, the map dialog and the search box.
(function () {
  'use strict';
  const root = document.documentElement;
  const $ = (selector, scope) => (scope || document).querySelector(selector);
  const $$ = (selector, scope) => Array.from((scope || document).querySelectorAll(selector));
  const store = {
    get(key) { try { return localStorage.getItem(key); } catch (error) { return null; } },
    set(key, value) { try { localStorage.setItem(key, value); } catch (error) { /* private mode */ } },
  };

  // ---- Language ---------------------------------------------------------------------------------
  const params = new URLSearchParams(location.search);
  let lang = params.get('lang') || store.get('learn.lang') || ((navigator.language || '').toLowerCase().startsWith('zh') ? 'zh' : 'en');
  if (lang !== 'zh' && lang !== 'en') lang = 'zh';

  function setLang(next, keepPlace) {
    const anchor = keepPlace ? currentSectionId() : null;
    lang = next;
    root.dataset.lang = lang;
    root.lang = lang === 'zh' ? 'zh-CN' : 'en';
    store.set('learn.lang', lang);
    $$('[data-set-lang]').forEach((b) => b.setAttribute('aria-pressed', String(b.dataset.setLang === lang)));
    document.title = lang === 'zh' ? 'Cardenza 学习 · 说明书' : 'Cardenza Learn · Manual';
    $('#search').placeholder = lang === 'zh' ? '搜索说明书：音量、录音、BPM …  ( / )' : 'Search: volume, record, BPM …  ( / )';
    if (anchor) go(anchor, false);
    runSearch();
    updateToc();
    if (typeof followLanguage === 'function') followLanguage();
  }
  $$('[data-set-lang]').forEach((button) => button.addEventListener('click', () => setLang(button.dataset.setLang, true)));

  // ---- Navigation -------------------------------------------------------------------------------
  function sectionElement(id) {
    return document.getElementById(lang + '-' + id);
  }

  function go(id, flash) {
    const element = sectionElement(id);
    if (!element) return false;
    element.scrollIntoView({ block: 'start' });
    if (flash) {
      element.classList.remove('flash');
      void element.offsetWidth;
      element.classList.add('flash');
    }
    return true;
  }

  function fromHash() {
    const id = decodeURIComponent(location.hash.slice(1));
    if (id && !go(id, true)) go('start', false);
  }
  window.addEventListener('hashchange', fromHash);

  document.addEventListener('click', (event) => {
    const link = event.target.closest('a[href^="#"]');
    if (!link) return;
    const id = link.getAttribute('href').slice(1);
    if (!id || !sectionElement(id)) return;
    event.preventDefault();
    closeMap();
    closeSidebar();
    if (location.hash.slice(1) === id) go(id, true);
    else location.hash = id;
  });

  // ---- Table of contents follows the reading ----------------------------------------------------
  function currentSectionId() {
    const headings = $$(`.content[data-l="${lang}"] [data-id]`);
    let current = headings[0];
    for (const heading of headings) {
      if (heading.getBoundingClientRect().top - 90 <= 0) current = heading;
      else break;
    }
    return current ? current.dataset.id : null;
  }

  let tocFrame = 0;
  function updateToc() {
    cancelAnimationFrame(tocFrame);
    tocFrame = requestAnimationFrame(() => {
      const id = currentSectionId();
      const toc = $(`.sidebar [data-l="${lang}"]`);
      if (!toc) return;
      let chapterId = id;
      const heading = id && sectionElement(id);
      if (heading) chapterId = heading.closest('[data-chapter]').dataset.chapter;
      $$('a', toc).forEach((a) => a.classList.toggle('active', a.getAttribute('href') === '#' + id));
      $$('.toc-chapter', toc).forEach((li) => {
        const open = li.firstElementChild.getAttribute('href') === '#' + chapterId;
        li.classList.toggle('open', open);
      });
      // Keep the active entry visible by scrolling the sidebar alone: scrollIntoView would also
      // scroll the page and fight the reader's wheel.
      const active = $('a.active', toc);
      const box = $('#sidebar');
      if (active && box && box.scrollHeight > box.clientHeight) {
        const boxRect = box.getBoundingClientRect();
        const r = active.getBoundingClientRect();
        if (r.top < boxRect.top + 40 || r.bottom > boxRect.bottom - 40)
          box.scrollTop += r.top - boxRect.top - box.clientHeight / 2;
      }
    });
  }
  document.addEventListener('scroll', updateToc, { passive: true });

  // ---- Sidebar (phones) -------------------------------------------------------------------------
  const sidebar = $('#sidebar');
  const menuButton = $('#menu-btn');
  function closeSidebar() {
    sidebar.classList.remove('open');
    menuButton.setAttribute('aria-expanded', 'false');
  }
  menuButton.addEventListener('click', () => {
    const open = !sidebar.classList.contains('open');
    sidebar.classList.toggle('open', open);
    menuButton.setAttribute('aria-expanded', String(open));
  });

  // ---- Map --------------------------------------------------------------------------------------
  const map = $('#map');
  function openMap() {
    map.hidden = false;
    $('#map-close').focus();
  }
  function closeMap() {
    map.hidden = true;
  }
  $('#map-btn').addEventListener('click', openMap);
  $('#map-close').addEventListener('click', closeMap);
  map.addEventListener('click', (event) => { if (event.target === map) closeMap(); });

  // ---- Search -----------------------------------------------------------------------------------
  const input = $('#search');
  const results = $('#results');
  let index = null;
  let items = [];
  let selected = -1;

  fetch('search-index.json')
    .then((response) => response.json())
    .then((data) => {
      index = window.LearnSearch.createIndex(data);
      if (input.value) runSearch();
    })
    .catch(() => { /* the page still reads without search */ });

  function escapeHtml(text) {
    return text.replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
  }

  function highlight(snippet) {
    const text = snippet.text;
    const offset = snippet.offset;
    let out = '';
    let at = 0;
    for (const [start, end] of snippet.marks) {
      const s = start + offset;
      const e = end + offset;
      if (s < at) continue;
      out += escapeHtml(text.slice(at, s)) + '<mark>' + escapeHtml(text.slice(s, e)) + '</mark>';
      at = e;
    }
    return out + escapeHtml(text.slice(at));
  }

  function runSearch() {
    const query = input.value.trim();
    if (!query) {
      results.hidden = true;
      input.setAttribute('aria-expanded', 'false');
      return;
    }
    results.hidden = false;
    input.setAttribute('aria-expanded', 'true');
    if (!index) {
      results.innerHTML = '<div class="r-empty">…</div>';
      return;
    }
    items = window.LearnSearch.search(index, query, { lang, limit: 14 });
    selected = items.length ? 0 : -1;
    const words = lang === 'zh'
      ? { none: '没有找到。试试别的说法，比如「音量」「保存」「录音」「BPM」。', count: (n) => `${n} 个结果`, hint: '↑↓ 选择 · Enter 打开' }
      : { none: 'Nothing found. Try other words, like “volume”, “save”, “record”, “BPM”.', count: (n) => `${n} result${n === 1 ? '' : 's'}`, hint: '↑↓ choose · Enter open' };
    if (!items.length) {
      results.innerHTML = `<div class="r-empty">${words.none}</div>`;
      return;
    }
    results.innerHTML = `<div class="r-head"><span>${words.count(items.length)}</span><span>${words.hint}</span></div>` +
      items.map((r, i) => `<a class="r" role="option" id="r-${i}" href="#${r.id}" aria-selected="${i === selected}">` +
        `<div class="r-title">${escapeHtml(r.title)}</div><div class="r-path">${escapeHtml(r.path)}</div>` +
        `<div class="r-snip">${highlight(r.snippet)}</div></a>`).join('');
  }

  let searchTimer = 0;
  input.addEventListener('input', () => {
    clearTimeout(searchTimer);
    searchTimer = setTimeout(runSearch, 60);
  });
  input.addEventListener('focus', () => { if (input.value.trim()) runSearch(); });
  input.addEventListener('keydown', (event) => {
    if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
      if (!items.length) return;
      event.preventDefault();
      selected = (selected + (event.key === 'ArrowDown' ? 1 : -1) + items.length) % items.length;
      $$('.r', results).forEach((a, i) => a.setAttribute('aria-selected', String(i === selected)));
      const active = $('#r-' + selected);
      if (active) {
        // Inside the results list only.
        if (active.offsetTop < results.scrollTop) results.scrollTop = active.offsetTop;
        else if (active.offsetTop + active.offsetHeight > results.scrollTop + results.clientHeight)
          results.scrollTop = active.offsetTop + active.offsetHeight - results.clientHeight;
      }
    } else if (event.key === 'Enter') {
      if (selected >= 0 && items[selected]) {
        event.preventDefault();
        const id = items[selected].id;
        results.hidden = true;
        input.blur();
        if (location.hash.slice(1) === id) go(id, true);
        else location.hash = id;
      }
    } else if (event.key === 'Escape') {
      input.value = '';
      results.hidden = true;
      input.blur();
    }
  });
  results.addEventListener('click', () => {
    results.hidden = true;
  });
  document.addEventListener('click', (event) => {
    if (!event.target.closest('.search')) results.hidden = true;
  });

  // ---- Keys: "/" search, "m" map, Esc closes ----------------------------------------------------
  document.addEventListener('keydown', (event) => {
    const typing = /^(INPUT|TEXTAREA|SELECT)$/.test(document.activeElement.tagName);
    if (event.key === 'Escape') {
      closeMap();
      closeSidebar();
    }
    if (typing || event.metaKey || event.ctrlKey || event.altKey) return;
    if (event.key === '/') {
      event.preventDefault();
      input.focus();
      input.select();
    } else if (event.key === 'm' || event.key === 'M') {
      map.hidden ? openMap() : closeMap();
    }
  });

  // ---- The live device ---------------------------------------------------------------------------
  // One simulator (the firmware compiled to WebAssembly, in a frame) for the whole page. It sits
  // over the figure last opened with "try it", in that figure's place; opening another figure moves
  // it there without restarting it, so only one device is ever operated. Keyboard input goes to
  // it only while the simulator has focus.
  const layer = $('#device-layer');
  const frame = $('#device-frame');
  const toastBox = $('#toast');
  let liveFigure = null;
  let deviceHeight = 420;
  let deviceBooted = false;
  let bootWaiters = [];
  let toastTimer = 0;

  function toast(zh, en) {
    toastBox.textContent = lang === 'zh' ? zh : en;
    toastBox.hidden = false;
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => { toastBox.hidden = true; }, 3200);
  }

  function device() {
    try { return frame.contentWindow && frame.contentWindow.cardenzaEmbed; } catch (error) { return null; }
  }

  function whenBooted() {
    if (deviceBooted) return Promise.resolve();
    return new Promise((resolve) => bootWaiters.push(resolve));
  }

  window.addEventListener('message', (event) => {
    if (event.origin !== location.origin || event.source !== frame.contentWindow) return;
    const data = event.data || {};
    if (data.type === 'cardenza-size' && data.height > 0) {
      deviceHeight = data.height;
      placeDevice();
    } else if (data.type === 'cardenza-booted') {
      deviceBooted = true;
      bootWaiters.splice(0).forEach((resolve) => resolve());
    }
  });

  function placeDevice() {
    if (!liveFigure) return;
    const slot = $('.shot-live', liveFigure);
    const badge = $('.device-badge', layer);
    frame.style.height = deviceHeight + 'px';
    slot.style.height = deviceHeight + badge.offsetHeight + 'px';
    const rect = slot.getBoundingClientRect();
    layer.style.top = rect.top + window.scrollY + 'px';
    layer.style.left = rect.left + window.scrollX + 'px';
    layer.style.width = rect.width + 'px';
  }
  window.addEventListener('resize', placeDevice);
  new ResizeObserver(() => placeDevice()).observe($('main'));

  function setFigureLive(figure, live) {
    figure.classList.toggle('live', live);
    $('.shot-static', figure).hidden = live;
    $('.shot-live', figure).hidden = !live;
    $('.try', figure).hidden = live;
    $('.goto', figure).hidden = !live;
    $('.untry', figure).hidden = !live;
  }

  function openDevice(figure, runSetup) {
    if (liveFigure && liveFigure !== figure) setFigureLive(liveFigure, false);
    liveFigure = figure;
    setFigureLive(figure, true);
    if (!frame.src) frame.src = new URL('../?embed=1', location.href).href;
    layer.hidden = false;
    placeDevice();
    focusDevice();
    if (runSetup) goTo(figure);
  }

  function closeDevice() {
    if (!liveFigure) return;
    setFigureLive(liveFigure, false);
    liveFigure = null;
    layer.hidden = true;
    const d = device();
    if (d) d.releaseAll();
  }

  function focusDevice() {
    whenBooted().then(() => {
      frame.focus();
      try { frame.contentWindow.focus(); } catch (error) { /* ignore */ }
    });
  }

  // The figure's screen: scene, track and page through the hold maps (ESC, TAB, CTRL + GRID).
  const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
  async function goTo(figure) {
    let steps = [];
    try { steps = JSON.parse(figure.dataset.setup || '[]'); } catch (error) { steps = []; }
    if (!steps.length) return;
    await whenBooted();
    const d = device();
    if (!d) return;
    d.releaseAll();
    const holdKey = { scene: 0, track: 1, page: 3 };
    for (const [what, value] of steps) {
      const left = holdKey[what];
      if (left === undefined) continue;
      const index = what === 'page' ? value : value - 1;
      d.press(left, 0, true, 'guide');
      await sleep(330);
      d.press(Math.floor(index / 4), 9 + (index % 4), true, 'guide');
      await sleep(70);
      d.press(Math.floor(index / 4), 9 + (index % 4), false, 'guide');
      await sleep(70);
      d.press(left, 0, false, 'guide');
      await sleep(260);
    }
    focusDevice();
  }

  document.addEventListener('click', (event) => {
    const figure = event.target.closest('figure.shot');
    if (!figure) return;
    if (event.target.closest('.try')) {
      const d = device();
      if (d) d.gesture();
      openDevice(figure, true);
    } else if (event.target.closest('.goto')) {
      goTo(figure);
    } else if (event.target.closest('.untry')) {
      closeDevice();
    }
  });

  // A focus ring while the device takes the keyboard.
  window.addEventListener('blur', () => setTimeout(() => {
    layer.classList.toggle('focused', document.activeElement === frame);
  }, 0));
  window.addEventListener('focus', () => layer.classList.remove('focused'));

  // The language switch shows the other copy of the text: the device follows to the same figure.
  function followLanguage() {
    if (!liveFigure) return;
    const shot = liveFigure.dataset.shot;
    const twin = $(`.content[data-l="${lang}"] figure.shot[data-shot="${shot}"]`);
    if (twin && twin !== liveFigure) openDevice(twin, false);
    else placeDevice();
  }

  setLang(lang, false);
  if (location.hash.length > 1) setTimeout(fromHash, 0);
})();
