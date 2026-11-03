// Cardenza guide (built by tools/learn/guide.py): lessons by hash (#/lesson-id), the device of the
// lessons (the web simulator in a frame, or a Cardenza on USB through Web MIDI), and steps that follow
// what happens on it. The device speaks the guide link (Applications/Synth/GrooveAppGuide.cpp):
// short text messages, as SysEx F0 23 7F <text> F7 to a Cardenza and F0 24 7F <text> F7 back.
(function () {
  'use strict';
  const root = document.documentElement;
  const $ = (selector, scope) => (scope || document).querySelector(selector);
  const $$ = (selector, scope) => Array.from((scope || document).querySelectorAll(selector));
  const store = {
    get(key) { try { return localStorage.getItem(key); } catch (error) { return null; } },
    set(key, value) { try { localStorage.setItem(key, value); } catch (error) { /* private mode */ } },
  };
  const data = window.GUIDE || { order: [] };
  const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
  const say = (zh, en) => (lang === 'zh' ? zh : en);

  // ---- Language (shared with the reference) ---------------------------------------------------------
  const params = new URLSearchParams(location.search);
  let lang = params.get('lang') || store.get('learn.lang') || ((navigator.language || '').toLowerCase().startsWith('zh') ? 'zh' : 'en');
  if (lang !== 'zh' && lang !== 'en') lang = 'zh';

  function setLang(next) {
    lang = next;
    root.dataset.lang = lang;
    root.lang = lang === 'zh' ? 'zh-CN' : 'en';
    store.set('learn.lang', lang);
    $$('[data-set-lang]').forEach((b) => b.setAttribute('aria-pressed', String(b.dataset.setLang === lang)));
    document.title = say('Cardenza 入门', 'Cardenza guide');
    $('#search').placeholder = say('查字典：音量、录音、BPM …  ( / )', 'Look up: volume, record, BPM …  ( / )');
    paintProgress();
    paintDevice();
    runSearch();
  }
  $$('[data-set-lang]').forEach((button) => button.addEventListener('click', () => setLang(button.dataset.setLang)));

  // ---- Toast --------------------------------------------------------------------------------------
  const toastBox = $('#toast');
  let toastTimer = 0;
  function toast(text, ms) {
    toastBox.textContent = text;
    toastBox.hidden = false;
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => { toastBox.hidden = true; }, ms || 3600);
  }

  // ---- Progress -------------------------------------------------------------------------------------
  let done = {};
  try { done = JSON.parse(store.get('guide.done') || '{}') || {}; } catch (error) { done = {}; }
  function markDone(id) {
    done[id] = true;
    store.set('guide.done', JSON.stringify(done));
    paintProgress();
  }
  function paintProgress() {
    $$('.g-part a[data-lesson], .card[data-lesson]').forEach((a) => a.classList.toggle('done', !!done[a.dataset.lesson]));
    const count = data.order.filter((id) => done[id]).length;
    $('#redo-all').hidden = count === 0;
    $('#progress-bar').style.width = (100 * count / Math.max(1, data.order.length)) + '%';
    $('#progress-text').textContent = say(`完成 ${count} / ${data.order.length} 课`, `${count} of ${data.order.length} lessons done`);
  }

  // ---- Rail ---------------------------------------------------------------------------------------
  const rail = $('#rail');
  const menuButton = $('#menu-btn');
  menuButton.addEventListener('click', () => {
    const open = !rail.classList.contains('open');
    rail.classList.toggle('open', open);
    menuButton.setAttribute('aria-expanded', String(open));
  });
  rail.addEventListener('click', (event) => {
    if (event.target.closest('a')) {
      rail.classList.remove('open');
      menuButton.setAttribute('aria-expanded', 'false');
    }
  });

  // ---- Search: the reference ---------------------------------------------------------------------
  const input = $('#search');
  const results = $('#results');
  let index = null;
  let items = [];
  let selected = -1;
  fetch('search-index.json').then((r) => r.json()).then((json) => {
    index = window.LearnSearch.createIndex(json);
    if (input.value) runSearch();
  }).catch(() => { /* the guide works without it */ });
  const escapeHtml = (text) => text.replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
  function highlight(snippet) {
    let out = '';
    let at = 0;
    for (const [start, end] of snippet.marks) {
      const s = start + snippet.offset;
      const e = end + snippet.offset;
      if (s < at) continue;
      out += escapeHtml(snippet.text.slice(at, s)) + '<mark>' + escapeHtml(snippet.text.slice(s, e)) + '</mark>';
      at = e;
    }
    return out + escapeHtml(snippet.text.slice(at));
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
    items = window.LearnSearch.search(index, query, { lang, limit: 10 });
    selected = items.length ? 0 : -1;
    if (!items.length) {
      results.innerHTML = `<div class="r-empty">${say('字典里没有找到。试试别的说法。', 'Nothing in the reference. Try other words.')}</div>`;
      return;
    }
    results.innerHTML = `<div class="r-head"><span>${say('字典', 'Reference')}</span><span>${say('↑↓ 选择 · Enter 打开', '↑↓ choose · Enter open')}</span></div>` +
      items.map((r, i) => `<a class="r" role="option" id="r-${i}" href="reference.html#${r.id}" aria-selected="${i === selected}">` +
        `<div class="r-title">${escapeHtml(r.title)}</div><div class="r-path">${escapeHtml(r.path)}</div>` +
        `<div class="r-snip">${highlight(r.snippet)}</div></a>`).join('');
  }
  let searchTimer = 0;
  input.addEventListener('input', () => { clearTimeout(searchTimer); searchTimer = setTimeout(runSearch, 60); });
  input.addEventListener('focus', () => { if (input.value.trim()) runSearch(); });
  input.addEventListener('keydown', (event) => {
    if ((event.key === 'ArrowDown' || event.key === 'ArrowUp') && items.length) {
      event.preventDefault();
      selected = (selected + (event.key === 'ArrowDown' ? 1 : -1) + items.length) % items.length;
      $$('.r', results).forEach((a, i) => a.setAttribute('aria-selected', String(i === selected)));
    } else if (event.key === 'Enter' && selected >= 0 && items[selected]) {
      event.preventDefault();
      location.href = 'reference.html#' + items[selected].id;
    } else if (event.key === 'Escape') {
      input.value = '';
      results.hidden = true;
      input.blur();
    }
  });
  document.addEventListener('click', (event) => { if (!event.target.closest('.search')) results.hidden = true; });
  document.addEventListener('keydown', (event) => {
    const typing = /^(INPUT|TEXTAREA|SELECT)$/.test(document.activeElement.tagName);
    if (!typing && event.key === '/' && !event.metaKey && !event.ctrlKey && !event.altKey) {
      event.preventDefault();
      input.focus();
    }
  });

  // ---- The device -----------------------------------------------------------------------------------
  // One device for the lessons: the simulator (always there), or a Cardenza on USB once connected. A
  // lesson that uses it starts the guide on it (GE): the device sets the user's work aside in a
  // practice project, goes where the lesson starts (GL), blinks the keys of the step (GH) and reports
  // its state (GS) and every key pressed (GK).
  const aside = $('#device');
  const layout = $('.g-layout');
  const device = { mode: 'sim', guide: false, entering: false, leaving: false, state: null, want: false };
  const sim = { frame: $('#sim-frame'), booted: false };
  const usb = { midi: null, input: null, output: null, hello: 0, button: $('#usb-btn') };

  function api() {
    try { return sim.frame.contentWindow && sim.frame.contentWindow.cardenzaEmbed; } catch (error) { return null; }
  }
  function send(text) {
    if (device.mode === 'usb') usbSend(text);
    else if (sim.booted && api()) api().guide(text);
  }

  window.addEventListener('message', (event) => {
    if (event.origin !== location.origin || event.source !== sim.frame.contentWindow) return;
    const message = event.data || {};
    if (message.type === 'cardenza-size' && message.height > 0) sim.frame.style.height = message.height + 'px';
    else if (message.type === 'cardenza-booted') {
      sim.booted = true;
      api().listen((text) => { if (device.mode === 'sim') heard(text); });
      if (device.mode === 'sim' && device.want) beginGuide();
    }
  });

  function beginGuide() {
    device.want = true;
    if (device.guide || device.entering) return;
    if (device.mode === 'sim' && !sim.booted) return;
    device.entering = true;
    device.leaving = false;
    send('GE');
    // An old firmware never answers; the simulator always does.
    setTimeout(() => { if (device.entering) { device.entering = false; paintDevice(); } }, 3000);
    paintDevice();
  }
  function endGuide() {
    device.want = false;
    if (!device.guide) return;
    device.leaving = true;
    send('GX');
    device.guide = false;
    paintDevice();
  }

  const REASONS = {
    rec: ['机器正在录音：先停下录音，再点这里重试。', 'The device is recording: stop it, then click here to try again.'],
    save: ['你现在的工程太长，教程没法替你暂存。先用 FN+S 存到卡上，在 SETUP 里新建一个空工程，再点这里重试。',
      'Your project is too long to set aside. Save it with FN+S, start a new project in SETUP, then click here to try again.'],
    mem: ['机器的内存不够：重启一下，再点这里重试。', 'The device is short of memory: restart it, then click here to try again.'],
  };
  const message = $('#d-msg');
  message.addEventListener('click', () => { message.hidden = true; beginGuide(); });

  function heard(text) {
    if (text === 'GE1') {
      device.guide = true;
      device.entering = false;
      message.hidden = true;
      paintDevice();
      if (run) applyStart();
    } else if (text.startsWith('GE0')) {
      device.entering = false;
      const reason = REASONS[text.slice(4)] || ['机器没能开始教程。', 'The device could not start the guide.'];
      message.textContent = say(reason[0], reason[1]);
      message.hidden = false;
      paintDevice();
    } else if (text === 'GX') {
      const ours = device.leaving;
      device.guide = false;
      device.leaving = false;
      paintDevice();
      // The device ended it by itself (the page was quiet): back in when someone is here.
      if (!ours && device.want && document.visibilityState === 'visible') setTimeout(beginGuide, 400);
    } else if (text.startsWith('GS')) {
      device.state = parseState(text.slice(2));
      onState(device.state);
    } else if (/^GK[0-9a-f]{2}[01]$/.test(text)) {
      const key = parseInt(text.slice(2, 4), 16);
      const down = text[4] === '1';
      liveKey(key, down);
      onKey(key, down);
    } else if (/^GD[0-9a-f]{56}$/.test(text)) {
      liveLeds(text.slice(2));
    }
  }

  function parseState(text) {
    const at = text.indexOf('n');
    const state = { n: at >= 0 ? text.slice(at + 1) : '' };
    (at >= 0 ? text.slice(0, at) : text).replace(/([a-z])(\d+)/g, (_, letter, value) => { state[letter] = Number(value); return ''; });
    return state;
  }

  setInterval(() => { if (device.guide) send('GP'); }, 4000);
  document.addEventListener('visibilitychange', () => {
    if (document.visibilityState === 'visible' && device.want && !device.guide) beginGuide();
  });
  window.addEventListener('pagehide', () => { if (device.guide) send('GX'); });

  function paintDevice() {
    const dot = $('#d-dot');
    dot.classList.toggle('on', device.guide);
    dot.classList.toggle('wait', device.entering);
    $('#d-title').textContent = device.mode === 'usb'
      ? say('你的 Cardenza', 'Your Cardenza') + (device.guide ? say(' · 教程中', ' · in the guide') : '')
      : say('模拟器', 'Simulator') + (sim.booted ? '' : say(' · 加载中…', ' · loading…'));
    $('#d-sim').hidden = device.mode === 'usb';
    $('#d-usb').hidden = device.mode !== 'usb';
    const swap = $('#d-switch');
    swap.hidden = device.mode !== 'usb';
    swap.textContent = say('改用模拟器', 'Use the simulator');
    usb.button.classList.toggle('on', device.mode === 'usb');
    $('.btn-text', usb.button).innerHTML = device.mode === 'usb'
      ? `<span>${say('已连接', 'Connected')}</span>` : `<span>${say('连接设备', 'Connect')}</span>`;
  }
  $('#d-switch').addEventListener('click', () => switchTo('sim'));

  function switchTo(mode) {
    if (mode === device.mode) return;
    const want = device.want;
    if (device.guide) {
      device.leaving = true;
      send('GX');
    }
    device.mode = mode;
    device.guide = false;
    device.entering = false;
    device.state = null;
    if (mode === 'sim') usbDrop();
    liveLeds(null);
    paintDevice();
    if (want) beginGuide();
  }

  // ---- A Cardenza on USB (Web MIDI) ---------------------------------------------------------------
  const hasMidi = 'requestMIDIAccess' in navigator;
  if (hasMidi) usb.button.hidden = false;
  const isCardenza = (port) => /cardenza/i.test(`${port.name || ''} ${port.manufacturer || ''}`);

  function usbSend(text) {
    if (!usb.output) return;
    const bytes = [0xF0, 0x23, 0x7F];
    for (let i = 0; i < text.length && i < 60; i++) bytes.push(text.charCodeAt(i) & 0x7F);
    bytes.push(0xF7);
    try { usb.output.send(bytes); } catch (error) { usbLost(); }
  }

  function usbDrop() {
    clearTimeout(usb.hello);
    if (usb.input) usb.input.onmidimessage = null;
    usb.input = null;
    usb.output = null;
  }

  function usbLost() {
    usbDrop();
    if (device.mode === 'usb') {
      toast(say('设备断开了：改用模拟器。', 'The device is gone: back to the simulator.'));
      switchTo('sim');
    }
  }

  async function usbConnect(quiet) {
    if (device.mode === 'usb') {
      switchTo('sim');
      return;
    }
    if (!hasMidi) return;
    try {
      usb.midi = usb.midi || await navigator.requestMIDIAccess({ sysex: true });
    } catch (error) {
      if (!quiet) toast(say('浏览器没有允许 MIDI（SysEx）：在地址栏的权限里打开它。', 'The browser did not allow MIDI (SysEx): allow it in the site settings.'));
      return;
    }
    usb.midi.onstatechange = (event) => {
      if (usb.input && event.port.state === 'disconnected' && (event.port === usb.input || event.port === usb.output)) usbLost();
    };
    const input = [...usb.midi.inputs.values()].find(isCardenza);
    const output = [...usb.midi.outputs.values()].find(isCardenza);
    if (!input || !output) {
      if (!quiet) toast(say('没有找到 Cardenza：用 USB 线接好、开机，再点一次。', 'No Cardenza found: connect it with a USB cable, turn it on, and try again.'));
      return;
    }
    usb.input = input;
    usb.output = output;
    input.onmidimessage = (event) => {
      const bytes = event.data;
      if (bytes.length < 4 || bytes[0] !== 0xF0 || bytes[1] !== 0x24 || bytes[2] !== 0x7F) return;
      let text = '';
      for (let i = 3; i < bytes.length && bytes[i] !== 0xF7; i++) text += String.fromCharCode(bytes[i]);
      if (text.startsWith('G!')) {
        clearTimeout(usb.hello);
        if (device.mode !== 'usb') {
          switchTo('usb');
          toast(say('已连接：你的 Cardenza 会跟着课程走。', 'Connected: your Cardenza follows the lessons.'));
        }
        return;
      }
      if (device.mode === 'usb') heard(text);
    };
    usbSend('G?');
    clearTimeout(usb.hello);
    usb.hello = setTimeout(() => {
      usbDrop();
      if (!quiet) toast(say('这台 Cardenza 的固件还不支持教程：先更新到最新固件。', 'This Cardenza\'s firmware has no guide yet: update it first.'), 6000);
    }, 1500);
  }
  usb.button.addEventListener('click', () => usbConnect(false));
  // A page that was allowed before connects by itself.
  if (hasMidi && navigator.permissions && navigator.permissions.query) {
    navigator.permissions.query({ name: 'midi', sysex: true }).then((status) => {
      if (status.state === 'granted') usbConnect(true);
    }).catch(() => {});
  }

  // The keyboard beside the lesson while a Cardenza is used: the keys pressed on it go down here, and
  // its key lights show as the device shows them (GD: a level 0-15 per key, as the eye sees it; the
  // device blinks the hints itself then). Levels to light as in the manual's pictures (build.py
  // KEY_LEVEL_FOR_PERCEIVED, at full master level).
  function liveKey(key, down) {
    const g = $(`#d-live [data-k="${key}"]`);
    if (g) g.classList.toggle('press', down);
  }
  const LED_DUTY = [0, 1, 1, 1, 1, 1, 1, 2, 3, 4, 5, 7, 8, 10, 13, 15];
  function liveLeds(digits) {
    const live = $('#d-live');
    live.classList.toggle('mirror', !!digits);
    $$('.k', live).forEach((g) => {
      const level = digits ? parseInt(digits[Number(g.dataset.k)] || '0', 16) : 0;
      const glow = level ? Math.pow(LED_DUTY[level] / 15, 0.45) : 0;
      g.style.setProperty('--b', glow.toFixed(3));
      g.classList.toggle('lit', glow > 0.55);
    });
  }

  // ---- Device lessons ---------------------------------------------------------------------------
  let run = null;  // { section, steps, index, base, latched, startFields, startTimer }
  let demoTimer = 0;
  let demoToken = 0;

  function startDeviceLesson(section) {
    run = { section, steps: $$('.step', section), index: 0, base: null, latched: [], pressed: [], waitingStart: true };
    run.steps.forEach((li) => li.classList.remove('current', 'done', 'ticked'));
    $('.l-done', section).hidden = true;
    section.classList.remove('complete');
    activateStep(0);
    beginGuide();
    applyStart();
  }

  function applyStart() {
    if (!run || !device.guide) return;
    const start = run.section.dataset.start;
    run.waitingStart = true;
    run.startFields = parseState(start.replace(/[rd]\d/g, ''));
    if (run.startFields.t !== undefined) run.startFields.t -= 1;
    delete run.startFields.n;
    send('GL' + start);
    sendHint();
    clearTimeout(run.startTimer);
    // The state the lesson starts from: the first one that matches the start, or the latest.
    run.startTimer = setTimeout(() => { if (run && run.waitingStart && device.state) takeBase(device.state); }, 900);
  }

  function takeBase(state) {
    run.waitingStart = false;
    run.base = Object.assign({}, state);
    evaluate(state);
  }

  function conditions(li) {
    try { return JSON.parse(li.dataset.done || '[]'); } catch (error) { return []; }
  }

  function activateStep(index) {
    if (!run) return;
    run.index = index;
    run.steps.forEach((li, i) => li.classList.toggle('current', i === index));
    const li = run.steps[index];
    resetConditions(li);
    run.base = run.waitingStart ? null : (device.state ? Object.assign({}, device.state) : null);
    sendHint();
    playDemo(li, 3);
    if (run.base) evaluate(run.base);
    const rect = li.getBoundingClientRect();
    if (index > 0 && (rect.top < 70 || rect.bottom > window.innerHeight)) li.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
  }

  function sendHint() {
    if (!run) return;
    const li = run.steps[run.index];
    const keys = li && !li.classList.contains('done') ? (li.dataset.keys || '').split(',').filter(Boolean).map(Number) : [];
    if (device.guide) send('GH' + keys.map((k) => k.toString(16).padStart(2, '0')).join(''));
    $$('#d-live .k').forEach((g) => g.classList.toggle('hint', keys.includes(Number(g.dataset.k))));
  }

  function onState(state) {
    if (!run || run.demo) return;
    if (run.waitingStart) {
      const matches = Object.keys(run.startFields).every((f) => state[f] === run.startFields[f]);
      if (matches) takeBase(state);
      return;
    }
    if (!run.base) run.base = Object.assign({}, state);
    evaluate(state);
  }

  // Nothing met yet; ["press", keys, n] counts the different keys of its list pressed.
  function resetConditions(li) {
    run.latched = conditions(li).map(() => false);
    run.pressed = conditions(li).map(() => new Set());
  }

  function onKey(key, down) {
    if (!run || run.waitingStart || run.demo || !down) return;
    const li = run.steps[run.index];
    stopDemo();
    conditions(li).forEach((condition, i) => {
      if (condition[0] !== 'press' || !condition[1].includes(key)) return;
      run.pressed[i].add(key);
      if (run.pressed[i].size >= (condition[2] || 1)) run.latched[i] = true;
    });
    finishIfDone();
  }

  function evaluate(state) {
    if (!run || run.waitingStart || run.demo || !run.base) return;
    const li = run.steps[run.index];
    if (li.classList.contains('done')) return;
    conditions(li).forEach((condition, i) => {
      const [kind, field, value] = condition;
      if ((kind === 'state' || kind === 'seen') && state[field] === value) run.latched[i] = true;
      if (kind === 'min' && state[field] >= value) run.latched[i] = true;
      if (kind === 'change' && state[field] !== run.base[field]) run.latched[i] = true;
    });
    finishIfDone();
  }

  function finishIfDone() {
    if (!run || !run.latched.length || !run.latched.every(Boolean)) return;
    completeStep(run.index);
  }

  function completeStep(index) {
    const li = run.steps[index];
    if (li.classList.contains('done')) return;
    li.classList.add('done', 'ticked');
    stopDemo();
    sendHint();
    const section = run.section;
    setTimeout(() => {
      if (!run || run.section !== section) return;
      li.classList.remove('ticked');
      const next = run.steps.findIndex((s, i) => i > index && !s.classList.contains('done'));
      if (next >= 0) activateStep(next);
      else lessonComplete(section);
    }, 650);
  }

  function lessonComplete(section) {
    $('.l-done', section).hidden = false;
    section.classList.add('complete');
    markDone(section.dataset.lesson);
    if (run && run.section === section) {
      run.steps.forEach((li) => li.classList.remove('current'));
      if (device.guide) send('GH');
      $$('#d-live .k').forEach((g) => g.classList.remove('hint'));
    }
    toast(say('完成！这一课学会了。', 'Done! You have got this one.'));
  }

  document.addEventListener('click', (event) => {
    const li = event.target.closest('.lesson[data-kind="device"] .step');
    if (!li || !run || !run.steps.includes(li)) return;
    const index = run.steps.indexOf(li);
    if (run.demo) return;  // the device is showing a step: it finishes first
    if (event.target.closest('.s-show')) showOnDevice(li);
    else if (event.target.closest('.s-skip')) completeStep(index);
    else if (event.target.closest('.s-head')) {
      li.classList.remove('done');
      $('.l-done', run.section).hidden = true;
      activateStep(index);
    }
  });

  // The step's demo on its key picture: keys go down and light as a person would press them. "Show me"
  // has the device do it as well (keys the page presses, GK, which never count for the step): the
  // device goes to where the step begins (a fresh practice project at the lesson's start, and the
  // earlier steps' demos again), does the step so it is seen and heard, and goes back there. The step
  // is then the user's to do.
  const injected = new Set();
  function inject(key, down) {
    if (down) injected.add(key);
    else injected.delete(key);
    send('GK' + key.toString(16).padStart(2, '0') + (down ? '1' : '0'));
  }
  function releaseInjected() { [...injected].forEach((key) => inject(key, false)); }
  function demoActions(li) {
    try { return JSON.parse(li.dataset.demo || '[]'); } catch (error) { return []; }
  }

  function cancelDemo() {
    demoToken++;
    clearTimeout(demoTimer);
    releaseInjected();
    $$('.s-keys .k.press').forEach((g) => g.classList.remove('press'));
  }
  function stopDemo() {
    cancelDemo();
    if (run && run.demo) endDeviceDemo();
  }

  function playDemo(li, loops, onDevice, after) {
    cancelDemo();
    const token = demoToken;
    const actions = demoActions(li);
    const keys = (li.dataset.keys || '').split(',').filter(Boolean).map(Number);
    const figure = $('.s-keys', li);
    if (!figure) return;
    $$('.k', figure).forEach((g) => g.classList.toggle('hint', keys.includes(Number(g.dataset.k))));
    const key = (k) => $(`[data-k="${k}"]`, figure);
    let at = 0;
    const step = () => {
      if (token !== demoToken) return;
      if (at >= actions.length) {
        $$('.k.press', figure).forEach((g) => g.classList.remove('press'));
        if (onDevice) releaseInjected();
        if (--loops > 0) demoTimer = setTimeout(() => { at = 0; step(); }, 1500);
        else if (after) after(token);
        return;
      }
      const [verb, value] = actions[at++];
      let wait = 260;
      const g = verb === 'wait' || verb === 'shot' ? null : key(value);
      if (verb === 'tap') {
        if (g) g.classList.add('press');
        // On the device a tap is short: held 200 ms a GRID cell opens its value picker instead.
        if (onDevice) {
          inject(value, true);
          setTimeout(() => { if (token === demoToken) inject(value, false); }, 70);
        }
        setTimeout(() => { if (token === demoToken && g) g.classList.remove('press'); }, 200);
        wait = 330;
      } else if (verb === 'down') {
        if (g) g.classList.add('press');
        if (onDevice) inject(value, true);
        wait = 380;
      } else if (verb === 'up') {
        if (g) g.classList.remove('press');
        if (onDevice) inject(value, false);
      } else if (verb === 'wait') {
        // On the device a wait is the time the demo needs (a held key, a song playing).
        wait = onDevice ? Math.max(160, Math.min(3200, value * 16)) : Math.max(160, Math.min(1400, value * 16));
      } else if (verb === 'shot') {
        wait = 0;
      }
      demoTimer = setTimeout(step, wait);
    };
    demoTimer = setTimeout(step, 450);
  }

  function showOnDevice(li) {
    if (!run || !device.guide || run.waitingStart) {
      playDemo(li, 1);
      return;
    }
    const index = run.steps.indexOf(li);
    const button = $('.s-show', li);
    run.demo = { button };
    button.disabled = true;
    button.dataset.label = button.innerHTML;
    button.innerHTML = say('演示中…', 'Showing…');
    const toStepStart = (token, then) => {
      const start = run.section.dataset.start;
      send('GL' + (/[rd]1/.test(start) ? start : 'r1' + start));
      replay(run.steps.slice(0, index).flatMap(demoActions), token, then);
    };
    cancelDemo();
    const first = demoToken;
    toStepStart(first, () => {
      if (first !== demoToken || !run) return;
      playDemo(li, 1, true, (token) => {
        // The result stays a moment, then the device goes back.
        demoTimer = setTimeout(() => {
          if (token !== demoToken || !run) return;
          toStepStart(token, () => {
            if (token !== demoToken || !run) return;
            endDeviceDemo();
            run.base = null;
            resetConditions(li);
            sendHint();
            send('GQ');
            toast(say('轮到你了：在机器上照着做一遍。', 'Your turn: now do it on the device.'));
          });
        }, 1300);
      });
    });
  }

  function endDeviceDemo() {
    const { button } = run.demo;
    run.demo = null;
    button.disabled = false;
    if (button.dataset.label) button.innerHTML = button.dataset.label;
  }

  // The earlier steps' demos, quickly, after the lesson's start (GL) has taken effect. Held keys keep
  // their waits, so a key map still opens.
  function replay(actions, token, done) {
    let at = 0;
    const next = () => {
      if (token !== demoToken) return;
      if (at >= actions.length) {
        releaseInjected();
        demoTimer = setTimeout(done, 300);
        return;
      }
      const [verb, value] = actions[at++];
      let wait = 90;
      if (verb === 'tap') {
        inject(value, true);
        setTimeout(() => { if (token === demoToken) inject(value, false); }, 60);
        wait = 130;
      } else if (verb === 'down') {
        inject(value, true);
        wait = 130;
      } else if (verb === 'up') {
        inject(value, false);
      } else if (verb === 'wait') {
        wait = Math.max(60, Math.min(600, value * 16));
      } else {
        wait = 0;
      }
      demoTimer = setTimeout(next, wait);
    };
    demoTimer = setTimeout(next, 400);
  }

  // ---- Concept lessons --------------------------------------------------------------------------
  let anim = null;  // { section, steps, index, player }
  let soundOn = false;

  function startAnimLesson(section) {
    const stage = $('.anim-stage', section);
    // Each step loops from the top when shown (GuideAnim); the player goes with the lesson.
    anim = { section, steps: $$('.step', section), index: 0, player: window.GuideAnim ? window.GuideAnim.create(stage, section.dataset.anim) : null };
    $('.l-done', section).hidden = true;
    section.classList.remove('complete');
    showAnimStep(0);
  }

  function showAnimStep(index) {
    anim.index = index;
    anim.steps.forEach((li, i) => {
      li.classList.toggle('current', i === index);
      li.classList.toggle('done', i < index);
    });
    if (anim.player) anim.player.setStep(index);
    $('.anim-text', anim.section).innerHTML = $('.s-say', anim.steps[index]).innerHTML;
    $$('.a-dot', anim.section).forEach((dot, i) => {
      dot.classList.toggle('on', i === index);
      dot.classList.toggle('seen', i < index);
    });
    const last = index === anim.steps.length - 1;
    const next = $('.a-next', anim.section);
    next.innerHTML = last ? say('明白了 ✓', 'Got it ✓') : say('下一步 →', 'Next →');
    $('.a-back', anim.section).disabled = index === 0;
  }

  document.addEventListener('click', (event) => {
    if (!anim) return;
    const section = event.target.closest('.lesson[data-kind="anim"]');
    if (section !== anim.section) return;
    if (event.target.closest('.a-next')) {
      if (anim.index < anim.steps.length - 1) showAnimStep(anim.index + 1);
      else {
        anim.steps.forEach((li) => { li.classList.add('done'); li.classList.remove('current'); });
        lessonComplete(section);
        const nextLink = $('.l-pager .next', section);
        if (nextLink) setTimeout(() => { location.hash = nextLink.getAttribute('href'); }, 900);
      }
    } else if (event.target.closest('.a-back')) {
      if (anim.index > 0) showAnimStep(anim.index - 1);

    } else if (event.target.closest('.a-sound')) {
      soundOn = !soundOn;
      $$('.a-sound').forEach((b) => b.setAttribute('aria-pressed', String(soundOn)));
      if (window.GuideAnim) window.GuideAnim.setSound(soundOn);
    } else if (event.target.closest('.a-dot')) {
      showAnimStep(Number(event.target.closest('.a-dot').dataset.step));
    }
  });

  // ---- Routing ----------------------------------------------------------------------------------
  const home = $('#home');
  const sections = Object.fromEntries($$('.lesson').map((s) => [s.dataset.lesson, s]));
  let shown = null;

  function route() {
    const hash = location.hash;
    const id = hash.startsWith('#/') ? decodeURIComponent(hash.slice(2)) : '';
    show(sections[id] ? id : '');
  }

  function show(id) {
    if (shown === id) return;
    shown = id;
    stopDemo();
    if (anim && anim.player) anim.player.destroy();
    anim = null;
    run = null;
    home.hidden = id !== '';
    Object.values(sections).forEach((s) => { s.hidden = s.dataset.lesson !== id; });
    $$('.g-part a').forEach((a) => a.classList.toggle('active', a.dataset.lesson === id));
    $('.g-home-link').classList.toggle('active', id === '');
    const section = sections[id];
    const isDevice = section && section.dataset.kind === 'device';
    aside.hidden = !isDevice;
    layout.classList.toggle('with-device', !!isDevice);
    window.scrollTo(0, 0);
    if (!section) {
      endGuide();
      return;
    }
    if (isDevice) {
      if (!sim.frame.src) sim.frame.src = new URL('../?embed=1', location.href).href;
      startDeviceLesson(section);
    } else {
      startAnimLesson(section);
    }
  }
  window.addEventListener('hashchange', route);

  // Learning again: a lesson's tick goes and it starts over from its first step; "all" clears every
  // tick (after asking) and starts from the first lesson.
  function startOver(id) {
    if (shown !== id) {
      location.hash = '#/' + id;
      return;
    }
    stopDemo();
    if (anim && anim.player) anim.player.destroy();
    anim = null;
    run = null;
    window.scrollTo(0, 0);
    const section = sections[id];
    if (section.dataset.kind === 'device') startDeviceLesson(section);
    else startAnimLesson(section);
  }
  function learnAgain(id) {
    delete done[id];
    store.set('guide.done', JSON.stringify(done));
    paintProgress();
    startOver(id);
  }
  function learnAllAgain() {
    const count = data.order.filter((id) => done[id]).length;
    if (!count || !window.confirm(say(`清除全部 ${count} 课的完成记录，从第一课重新学习？`,
      `Clear the ticks of all ${count} finished lessons and start again from lesson 1?`))) return;
    done = {};
    store.set('guide.done', JSON.stringify(done));
    paintProgress();
    startOver(data.order[0]);
  }
  document.addEventListener('click', (event) => {
    const redo = event.target.closest('.redo[data-lesson]');
    if (redo) {
      event.preventDefault();
      event.stopPropagation();
      learnAgain(redo.dataset.lesson);
    } else if (event.target.closest('#redo-all')) {
      learnAllAgain();
    }
  }, true);
  document.addEventListener('keydown', (event) => {
    const redo = event.target.closest && event.target.closest('span.redo[data-lesson]');
    if (redo && (event.key === 'Enter' || event.key === ' ')) {
      event.preventDefault();
      learnAgain(redo.dataset.lesson);
    }
  });

  // Small looping pictures on the home page and the cards.
  if (window.GuideAnim) $$('[data-anim-thumb]').forEach((el) => window.GuideAnim.thumb(el, el.dataset.animThumb));

  setLang(lang);
  route();
})();
