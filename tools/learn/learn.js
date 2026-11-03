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
  // it there without restarting it, so only one device is ever operated. Keys typed while it has
  // focus, and the keys of a Cardenza on USB, go to it.
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
      // Scenes by their on-screen number (0 SETUP ... 3 TAPE) and pages from 0; tracks are 1-4.
      const index = what === 'track' ? value - 1 : value;
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

  // ---- A real Cardenza on USB ----------------------------------------------------------------------
  // The firmware writes every key change to its console as "@K<row>,<column>,<0|1>" (HOME: "@H<0|1>").
  // The device is a USB MIDI device: Web MIDI asks it for the console over SysEx (CardenzaHAL's system
  // channel: F0 23 43 01 F7 starts it, the text comes back as F0 24 43 <text> F7). A device started
  // with HOME held keeps its serial port instead; Web Serial reads that one.
  const usbButton = $('#usb-btn');
  const usb = { port: null, reader: null, midi: null, input: null, output: null, buffer: '' };
  const hasMidi = 'requestMIDIAccess' in navigator;
  const hasSerial = 'serial' in navigator;
  if (hasMidi || hasSerial) usbButton.hidden = false;

  function usbKey(row, column, down) {
    if (!liveFigure) {
      // No device open yet: open the one in the figure nearest the reading position.
      const figures = $$(`.content[data-l="${lang}"] figure.shot`);
      const near = figures.find((f) => f.getBoundingClientRect().bottom > 80) || figures[0];
      if (!near) return;
      openDevice(near, false);
      toast('设备已在这一节打开：用机器上的键操作它', 'Opened the device in this section: play it with your Cardenza');
    }
    whenBooted().then(() => {
      const d = device();
      if (!d) return;
      if (row < 0) d.home(down, 'usb');
      else d.press(row, column, down, 'usb');
    });
    usbButton.classList.add('blink');
    setTimeout(() => usbButton.classList.remove('blink'), 90);
  }

  // Console text in, key lines out.
  function usbText(text) {
    usb.buffer += text;
    let newline;
    while ((newline = usb.buffer.indexOf('\n')) >= 0) {
      const line = usb.buffer.slice(0, newline);
      usb.buffer = usb.buffer.slice(newline + 1);
      const key = /@K(\d+),(\d+),([01])/.exec(line);
      if (key) usbKey(Number(key[1]), Number(key[2]), key[3] === '1');
      const home = /@H([01])/.exec(line);
      if (home) usbKey(-1, -1, home[1] === '1');
    }
    if (usb.buffer.length > 4096) usb.buffer = usb.buffer.slice(-256);
  }

  function usbConnected() { return Boolean(usb.port || usb.input); }

  async function usbDisconnect() {
    if (usb.input) {
      usb.input.onmidimessage = null;
      try { usb.output.send([0xF0, 0x23, 0x43, 0x00, 0xF7]); } catch (error) { /* ignore */ }
    }
    try { if (usb.reader) await usb.reader.cancel(); } catch (error) { /* ignore */ }
    try { if (usb.port) await usb.port.close(); } catch (error) { /* ignore */ }
    usb.port = null;
    usb.reader = null;
    usb.input = null;
    usb.output = null;
    usb.buffer = '';
    usbButton.classList.remove('on');
    const d = device();
    if (d) d.releaseAll();
  }

  const isCardenza = (port) => /cardenza/i.test(`${port.name || ''} ${port.manufacturer || ''}`);

  // True when a Cardenza answered on MIDI; false when there is none (or no Web MIDI).
  async function usbConnectMidi() {
    if (!hasMidi) return false;
    try {
      usb.midi = usb.midi || await navigator.requestMIDIAccess({ sysex: true });
    } catch (error) {
      toast('浏览器没有允许 MIDI（SysEx）', 'The browser did not allow MIDI (SysEx)');
      return false;
    }
    const input = [...usb.midi.inputs.values()].find(isCardenza);
    const output = [...usb.midi.outputs.values()].find(isCardenza);
    if (!input || !output) return false;
    usb.input = input;
    usb.output = output;
    input.onmidimessage = (event) => {
      const data = event.data;
      if (data.length < 4 || data[0] !== 0xF0 || data[1] !== 0x24 || data[2] !== 0x43) return;
      let text = '';
      for (let index = 3; index < data.length && data[index] !== 0xF7; ++index) text += String.fromCharCode(data[index]);
      usbText(text);
    };
    output.send([0xF0, 0x23, 0x43, 0x01, 0xF7]);
    usb.midi.onstatechange = (event) => {
      if (usb.input && event.port.state === 'disconnected' && (event.port === usb.input || event.port === usb.output)) {
        usbDisconnect();
        toast('设备连接中断', 'The device connection was lost');
      }
    };
    return true;
  }

  async function usbConnectSerial() {
    let port;
    try {
      port = await navigator.serial.requestPort({ filters: [{ usbVendorId: 0x303a }] });
      await port.open({ baudRate: 115200 });
    } catch (error) {
      if (error && error.name !== 'NotFoundError') toast('无法打开设备：' + error.message, 'Could not open the device: ' + error.message);
      return;
    }
    usb.port = port;
    usbButton.classList.add('on');
    toast('已连接：在机器上按键，操作教程里的设备', 'Connected: keys on your Cardenza play the device in the tutorial');
    const decoder = new TextDecoderStream();
    port.readable.pipeTo(decoder.writable).catch(() => {});
    usb.reader = decoder.readable.getReader();
    try {
      for (;;) {
        const { value, done } = await usb.reader.read();
        if (done) break;
        usbText(value);
      }
    } catch (error) {
      toast('设备连接中断', 'The device connection was lost');
    }
    await usbDisconnect();
  }

  async function usbConnect() {
    if (usbConnected()) {
      await usbDisconnect();
      toast('设备已断开', 'Device disconnected');
      return;
    }
    if (await usbConnectMidi()) {
      usbButton.classList.add('on');
      toast('已连接：在机器上按键，操作教程里的设备', 'Connected: keys on your Cardenza play the device in the tutorial');
      return;
    }
    if (hasSerial) await usbConnectSerial();
    else toast('没有找到 Cardenza：用 USB 线接好再试', 'No Cardenza found: connect it with a USB cable and try again');
  }
  usbButton.addEventListener('click', usbConnect);
  if (hasSerial) navigator.serial.addEventListener('disconnect', (event) => { if (event.target === usb.port) usbDisconnect(); });

  setLang(lang, false);
  if (location.hash.length > 1) setTimeout(fromHash, 0);
})();
