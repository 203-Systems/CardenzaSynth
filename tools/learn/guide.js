// Cardenza guide (built by tools/learn/guide.py): lessons by hash (#/lesson-id), and a Cardenza on USB
// (Web MIDI) that follows them: its steps tick themselves off as they are done on it. Without one the
// lessons are pictures and go on with Next. The device speaks the guide link
// (Applications/Synth/GrooveAppGuide.cpp): short text messages, as SysEx F0 23 7F <text> F7 to the
// Cardenza and F0 24 7F <text> F7 back.
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
  // A Cardenza on USB, through Web MIDI. The page asks for MIDI (SysEx) as it opens: the browser asks
  // the user once, and from then on a Cardenza plugged in, now or later, is found by itself. A device
  // lesson starts the guide on it (GE): the device sets the user's work aside in a practice project,
  // goes where the lesson starts (GL), blinks the keys of the step until it is learned (GH, then GT steady) and reports its state (GS) and
  // every key pressed (GK). Its key lights (GD) are on the device itself; the page does not show them.
  //   status: nomidi (no Web MIDI here) | denied | waiting (allowed, no Cardenza) | hello (asking it) |
  //           old (its firmware has no guide) | on
  const device = { status: 'waiting', guide: false, entering: false, leaving: false, state: null, want: false, reason: null };
  const usb = { midi: null, input: null, output: null, hello: 0 };
  const hasMidi = 'requestMIDIAccess' in navigator;
  const isCardenza = (port) => port.state === 'connected' && /cardenza/i.test(`${port.name || ''} ${port.manufacturer || ''}`);
  const connected = () => device.status === 'on';

  function usbSend(text) {
    if (!usb.output) return;
    const bytes = [0xF0, 0x23, 0x7F];
    for (let i = 0; i < text.length && i < 60; i++) bytes.push(text.charCodeAt(i) & 0x7F);
    bytes.push(0xF7);
    try { usb.output.send(bytes); } catch (error) { detach(); }
  }
  function send(text) { if (connected()) usbSend(text); }

  function setStatus(status) {
    device.status = status;
    paintDevice();
  }

  // A Cardenza among the ports: take it and ask whether its firmware has the guide.
  function attach() {
    if (!usb.midi || usb.input) return;
    const input = [...usb.midi.inputs.values()].find(isCardenza);
    const output = [...usb.midi.outputs.values()].find(isCardenza);
    if (!input || !output) {
      if (device.status !== 'old') setStatus('waiting');
      return;
    }
    usb.input = input;
    usb.output = output;
    input.onmidimessage = onMidi;
    setStatus('hello');
    usbSend('G?');
    clearTimeout(usb.hello);
    usb.hello = setTimeout(() => {
      if (device.status !== 'hello') return;
      release();
      setStatus('old');
    }, 2000);
  }

  function release() {
    clearTimeout(usb.hello);
    if (usb.input) usb.input.onmidimessage = null;
    usb.input = null;
    usb.output = null;
  }

  // The Cardenza went away: the lesson goes on as pictures.
  function detach() {
    const was = connected();
    release();
    device.guide = false;
    device.entering = false;
    device.state = null;
    device.reason = null;
    setStatus('waiting');
    if (was) toast(say('Cardenza 断开了：接着看图学，插回去会自动连上。', 'Your Cardenza is gone: go on with the pictures; plug it back in and it reconnects.'));
  }

  function onMidi(event) {
    const bytes = event.data;
    if (bytes.length < 4 || bytes[0] !== 0xF0 || bytes[1] !== 0x24 || bytes[2] !== 0x7F) return;
    let text = '';
    for (let i = 3; i < bytes.length && bytes[i] !== 0xF7; i++) text += String.fromCharCode(bytes[i]);
    if (text.startsWith('G!')) {
      clearTimeout(usb.hello);
      if (connected()) return;
      setStatus('on');
      toast(say('Cardenza 已连接：它会跟着课程走。', 'Your Cardenza is connected: it follows the lessons.'));
      if (device.want) beginGuide();
      return;
    }
    if (connected()) heard(text);
  }

  async function askMidi() {
    if (!hasMidi) {
      setStatus('nomidi');
      return;
    }
    try {
      usb.midi = await navigator.requestMIDIAccess({ sysex: true });
    } catch (error) {
      setStatus('denied');
      return;
    }
    usb.midi.onstatechange = (event) => {
      const port = event.port;
      if (usb.input && port.state === 'disconnected' && (port === usb.input || port === usb.output)) detach();
      // Plugged in: both of its ports appear, one after the other.
      else if (!usb.input && port.state === 'connected' && isCardenza(port)) setTimeout(attach, 300);
    };
    attach();
  }

  function beginGuide() {
    device.want = true;
    if (!connected() || device.guide || device.entering) return;
    device.entering = true;
    device.leaving = false;
    device.reason = null;
    send('GE');
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
    other: ['机器没能开始教程：点这里重试。', 'The device could not start the guide: click here to try again.'],
  };
  const STATUS = {
    nomidi: ['用电脑上的 Chrome 或 Edge 打开这一页，再接上 Cardenza：机器会跟着课程走。', 'Open this page in Chrome or Edge on a computer and plug in your Cardenza: it follows the lessons.'],
    denied: ['要让 Cardenza 跟着课程走：在地址栏左边的网站设置里允许 MIDI 设备，再刷新这一页。', 'To have your Cardenza follow the lessons, allow MIDI devices in the site settings (left of the address bar) and reload.'],
    waiting: ['用 USB 线接上 Cardenza，它会跟着这一课走；没有机器也可以看图跟着学。', 'Plug in your Cardenza with a USB cable and it follows this lesson; no device, follow the pictures.'],
    hello: ['正在连接 Cardenza…', 'Connecting to your Cardenza…'],
    old: ['这台 Cardenza 的固件还不支持教程：更新到最新固件，再接上。', 'This Cardenza\'s firmware has no guide yet: update it, then plug it in again.'],
    on: ['Cardenza 已连接：照着机器上闪的键按，学会了再点「下一步」。', 'Your Cardenza is connected: press the keys blinking on it; when you have got it, press Next.'],
  };
  $$('.l-device').forEach((line) => line.addEventListener('click', () => {
    if (!device.reason) return;
    device.reason = null;
    beginGuide();
  }));

  function paintDevice() {
    const live = connected() && device.guide;
    if (run) paintRun();
    document.body.classList.toggle('usb-on', live);
    const words = device.reason ? REASONS[device.reason] || REASONS.other
      : connected() && !device.guide ? STATUS.hello : STATUS[device.status];
    $$('.l-device').forEach((line) => {
      line.textContent = say(words[0], words[1]);
      line.classList.toggle('on', live);
      line.classList.toggle('wait', device.status === 'hello' || (connected() && !device.guide && !device.reason));
      line.classList.toggle('warn', !!device.reason || device.status === 'old' || device.status === 'denied');
      line.classList.toggle('retry', !!device.reason);
    });
  }

  function heard(text) {
    if (text === 'GE1') {
      device.guide = true;
      device.entering = false;
      paintDevice();
      if (run) applyStart();
    } else if (text.startsWith('GE0')) {
      device.entering = false;
      device.reason = text.slice(4) in REASONS ? text.slice(4) : 'other';
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
      onKey(key, down);
      pressed(key, down);
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

  // A key pressed on the Cardenza goes down on the step's key picture.
  function pressed(key, down) {
    const g = run && $(`.st-keys [data-k="${key}"]`, pageOf(run.index));
    if (g) g.classList.toggle('press', down);
  }

  // ---- Device lessons ---------------------------------------------------------------------------
  let run = null;  // { section, steps, index, base, latched, startFields, startTimer }
  let demoTimer = 0;
  let demoToken = 0;

  function startDeviceLesson(section) {
    run = { section, steps: $$('.step', section), index: 0, base: null, latched: [], pressed: [], waitingStart: true };
    run.steps.forEach((li) => {
      li.classList.remove('current', 'done', 'ticked');
      delete li.dataset.learned;
    });
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
    // Plugged in during the lesson: the device first does the steps already done, then the step's
    // state is taken from where that leaves it.
    const earlier = run.steps.slice(0, run.index).flatMap(demoActions);
    if (earlier.length) {
      cancelDemo();
      const token = demoToken;
      const section = run.section;
      run.catchingUp = true;
      replay(earlier, token, () => {
        if (!run || run.section !== section || token !== demoToken) return;
        run.catchingUp = false;
        run.waitingStart = false;
        run.base = null;
        sendHint();
        send('GQ');
      });
      return;
    }
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

  // The stage shows one step: its picture, its words under it, the steps as dots (as in the picture
  // lessons). The demo plays once on the key picture when the step comes up; Show me plays it again.
  function pageOf(index) { return $(`.st-page[data-step="${index}"]`, run.section); }

  // The last step's Next goes on to the next lesson (or back to all lessons after the last one), and it
  // offers to learn the lesson again.
  function nextLabel(section) {
    return section.dataset.next ? say('学习下一节 →', 'Next lesson →') : say('回到目录 →', 'All lessons →');
  }
  function goNext(section) {
    location.hash = section.dataset.next ? '#/' + section.dataset.next : '#/';
  }

  // With a Cardenza following, Next is grey until the step is learned on it; then "learned" shows
  // under the step's words (nothing covers the picture) and Next lights up. The page never moves on by
  // itself: the user tries more keys, then presses Next. Without one, Next is how the lesson goes on.
  function paintRun(justLearned) {
    const { section, steps, index } = run;
    const li = steps[index];
    const follows = connected() && device.guide;
    const learned = li.dataset.learned === '1';
    $$('.st-page', section).forEach((page) => { page.hidden = Number(page.dataset.step) !== index; });
    $('.anim-text', section).innerHTML = $('.s-say', li).innerHTML;
    $$('.a-dot', section).forEach((dot, i) => {
      dot.classList.toggle('on', i === index);
      dot.classList.toggle('done', steps[i].classList.contains('done'));
    });
    $('.a-back', section).disabled = index === 0;
    const last = index === steps.length - 1;
    const next = $('.a-next', section);
    next.innerHTML = last ? nextLabel(section) : say('下一步 →', 'Next →');
    next.classList.toggle('idle', follows && !learned);
    $('.l-pager .redo', section).hidden = !last;
    const pass = $('.anim-pass', section);
    pass.hidden = !learned;
    const press = last ? (section.dataset.next ? say('学习下一节', 'Next lesson') : say('回到目录', 'All lessons')) : say('下一步', 'Next');
    $('.pass-text', pass).textContent = say(`学会了！再多按几下试试，然后点「${press}」。`, `You have got it! Try a few more keys, then press ${press}.`);
    if (justLearned) {
      for (const node of [pass, next]) {
        node.classList.remove('pop');
        void node.offsetWidth;
        node.classList.add('pop');
      }
    }
  }

  function activateStep(index) {
    if (!run) return;
    run.index = index;
    run.steps.forEach((li, i) => li.classList.toggle('current', i === index));
    const li = run.steps[index];
    resetConditions(li);
    run.base = run.waitingStart ? null : (device.state ? Object.assign({}, device.state) : null);
    paintRun();
    sendHint();
    playDemo(li, 1);
    if (run.base) evaluate(run.base);
  }

  function sendHint() {
    if (!run) return;
    const li = run.steps[run.index];
    const keys = li ? (li.dataset.keys || '').split(',').filter(Boolean).map(Number) : [];
    // Blinking while the step waits for the user, steady once it is learned (they try more keys).
    if (device.guide) send((li && li.dataset.learned === '1' ? 'GT' : 'GH') + keys.map((k) => k.toString(16).padStart(2, '0')).join(''));
  }

  function onState(state) {
    if (!run || run.demo || run.catchingUp) return;
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
    if (!run || run.waitingStart || run.demo || run.catchingUp || !down) return;
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
    if (!run || run.waitingStart || run.demo || run.catchingUp || !run.base) return;
    const li = run.steps[run.index];
    if (li.dataset.learned === '1') return;
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
    completeStep(run.index, 'device');
  }

  // by: 'device' (it was done on the Cardenza: learned, the page stays) or 'next' (the Next button: on to
  // the next step, or the lesson is done).
  function completeStep(index, by) {
    const li = run.steps[index];
    const section = run.section;
    if (by === 'device') {
      if (li.dataset.learned === '1') return;
      stopDemo();
      li.dataset.learned = '1';
      li.classList.add('done');
      sendHint();
      paintRun(true);
      return;
    }
    stopDemo();
    li.classList.add('done');
    if (index + 1 < run.steps.length) activateStep(index + 1);
    else {
      if (!section.classList.contains('complete')) lessonComplete(section);
      goNext(section);
    }
  }

  function lessonComplete(section) {
    $('.l-done', section).hidden = false;
    section.classList.add('complete');
    markDone(section.dataset.lesson);
    if (run && run.section === section) {
      if (device.guide) send('GH');
      paintRun();
    }
    toast(say('完成！这一课学会了。', 'Done! You have got this one.'));
  }

  document.addEventListener('click', (event) => {
    const section = event.target.closest('.lesson[data-kind="device"]');
    if (!section || !run || run.section !== section) return;
    if (run.demo || run.catchingUp) return;  // the device is showing or catching up: it finishes first
    if (event.target.closest('.a-next')) {
      completeStep(run.index, 'next');
    } else if (event.target.closest('.a-back')) {
      if (run.index > 0) activateStep(run.index - 1);
    } else if (event.target.closest('.a-dot')) {
      activateStep(Number(event.target.closest('.a-dot').dataset.step));
    } else if (event.target.closest('.a-show')) {
      showOnDevice(run.steps[run.index]);
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
    $$('.st-keys .k.press').forEach((g) => g.classList.remove('press'));
  }
  function stopDemo() {
    cancelDemo();
    if (run && run.demo) endDeviceDemo();
  }

  // The demo's pace: slow enough to follow with the eye (the first pace, about three keys a second, was
  // far too quick to see what was pressed). A pressed key stays lit, the next waits, and the moment
  // the step wants you to see (its "shot") holds still a while.
  const DEMO_PACE = { start: 800, tap: 900, tapLit: 420, down: 900, up: 650, shot: 900, loop: 2200, waitScale: 1.6, waitMin: 350 };

  function playDemo(li, loops, onDevice, after) {
    cancelDemo();
    const token = demoToken;
    const actions = demoActions(li);
    const figure = $('.st-keys', pageOf(run.steps.indexOf(li)));
    if (!figure) return;
    const key = (k) => $(`[data-k="${k}"]`, figure);
    let at = 0;
    const step = () => {
      if (token !== demoToken) return;
      if (at >= actions.length) {
        $$('.k.press', figure).forEach((g) => g.classList.remove('press'));
        if (onDevice) releaseInjected();
        if (--loops > 0) demoTimer = setTimeout(() => { at = 0; step(); }, DEMO_PACE.loop);
        else if (after) after(token);
        return;
      }
      const [verb, value] = actions[at++];
      let wait = DEMO_PACE.up;
      const g = verb === 'wait' || verb === 'shot' ? null : key(value);
      if (verb === 'tap') {
        if (g) g.classList.add('press');
        // On the device a tap is short: held 200 ms a GRID cell opens its value picker instead.
        if (onDevice) {
          inject(value, true);
          setTimeout(() => { if (token === demoToken) inject(value, false); }, 70);
        }
        setTimeout(() => { if (token === demoToken && g) g.classList.remove('press'); }, DEMO_PACE.tapLit);
        wait = DEMO_PACE.tap;
      } else if (verb === 'down') {
        if (g) g.classList.add('press');
        if (onDevice) inject(value, true);
        wait = DEMO_PACE.down;
      } else if (verb === 'up') {
        if (g) g.classList.remove('press');
        if (onDevice) inject(value, false);
      } else if (verb === 'wait') {
        // On the device a wait is the time the demo needs (a held key, a song playing).
        const ms = value * 16 * DEMO_PACE.waitScale;
        wait = Math.max(DEMO_PACE.waitMin, Math.min(onDevice ? 5000 : 2200, ms));
      } else if (verb === 'shot') {
        wait = DEMO_PACE.shot;
      }
      demoTimer = setTimeout(step, wait);
    };
    demoTimer = setTimeout(step, DEMO_PACE.start);
  }

  function showOnDevice(li) {
    if (!run || !device.guide || run.waitingStart) {
      playDemo(li, 1);
      return;
    }
    const index = run.steps.indexOf(li);
    const button = $('.a-show', run.section);
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
        // The result stays a while to be seen, then the device goes back.
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
        }, 2200);
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
    next.innerHTML = last ? nextLabel(anim.section) : say('下一步 →', 'Next →');
    $('.l-pager .redo', anim.section).hidden = !last;
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
        goNext(section);
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
    window.scrollTo(0, 0);
    if (!section) {
      endGuide();
      return;
    }
    if (isDevice) {
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
  // The lessons build on each other: learning one again clears its tick and those of every lesson
  // after it in the course; the ones before keep theirs.
  function learnAgain(id) {
    const from = data.order.indexOf(id);
    const again = from >= 0 ? data.order.slice(from) : [id];
    const cleared = again.filter((lesson) => done[lesson]).length;
    again.forEach((lesson) => { delete done[lesson]; });
    store.set('guide.done', JSON.stringify(done));
    paintProgress();
    if (cleared > 1) toast(say(`这一课和之后的课一起重新开始：清掉了 ${cleared} 课的完成记录。`,
      `This lesson and the ones after it start over: ${cleared} ticks cleared.`));
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


  // Small looping pictures on the home page and the cards.
  if (window.GuideAnim) $$('[data-anim-thumb]').forEach((el) => window.GuideAnim.thumb(el, el.dataset.animThumb));

  setLang(lang);
  route();
  askMidi();
})();
