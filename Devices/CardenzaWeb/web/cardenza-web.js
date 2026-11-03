// CardenzaWeb page: screen, 4x14 keyboard with LEDs, computer-keyboard mapping, Web Audio
// output and persistent storage around the wasm build of the Cardenza firmware app
// (cardenza.js / cardenza.wasm, see WebMain.cpp for the C API).
(function () {
  'use strict';

  const ROWS = 4;
  const COLUMNS = 14;
  const WIDTH = 240;
  const HEIGHT = 135;
  const STORAGE_MOUNT = '/sdcard';
  // The device's internal flash (Device::Storage internal records): the groovebox's autosaved working
  // state lives there, so it is kept in the browser like the card.
  const INTERNAL_MOUNT = '/internal';
  const loadStarted = performance.now();
  const isMac = /Mac|iPhone|iPad/.test(navigator.platform || navigator.userAgent);
  // ?embed=1: just the device (screen, keys, HOME) inside another page, the Learn manual. The host
  // page sizes the frame from the height reported here and can press keys (window.cardenzaEmbed).
  const EMBED = new URLSearchParams(location.search).has('embed');
  if (EMBED) document.documentElement.classList.add('embed');

  // ---------------------------------------------------------------------------------------
  // Device keys (mirrors RoleFor / kLegend in Applications/Synth/UI/Groove/GrooveTypes.h and
  // the keycap prints in Devices/Cardenza/cardenza/keyboard/layout.h).

  const LEFT_ROLES = ['ESC', 'TAB', 'FN', 'CTRL'];
  const RIGHT_ROLES = ['ADD', 'SUB', 'ENTER', 'PLAY'];
  const RIGHT_SYMBOLS = ['+', '−', '↵', '▶'];
  const LEGEND = [
    ['`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', ''],
    ['', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\\'],
    ['', '', 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', "'", ''],
    ['', '', '', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', ' '],
  ];
  // Keycap prints where the groovebox has no legend character.
  const CAP_PRINT = { '0,13': 'del', '2,1': 'shift', '3,1': 'opt', '3,2': 'alt', '3,13': 'space' };

  function roleOf(row, column) {
    if (column === 0) return LEFT_ROLES[row];
    if (column === COLUMNS - 1) return RIGHT_ROLES[row];
    return column <= 8 ? 'PAD' : 'GRID';
  }

  function describeKey(row, column) {
    const role = roleOf(row, column);
    const legend = LEGEND[row][column];
    if (column === 0) return role;
    if (column === COLUMNS - 1) return role + (CAP_PRINT[row + ',' + column] ? ' (' + CAP_PRINT[row + ',' + column] + ')' : legend.trim() ? ' (' + legend + ')' : '');
    const name = legend || CAP_PRINT[row + ',' + column] || '—';
    return name + ' · ' + role;
  }

  // ---------------------------------------------------------------------------------------
  // Computer keyboard, by physical position (KeyboardEvent.code, layout independent).

  const KEYMAP = new Map();
  function map(code, row, column) { KEYMAP.set(code, [row, column]); }
  map('Escape', 0, 0);
  map('Backquote', 0, 0);
  ['Digit1', 'Digit2', 'Digit3', 'Digit4', 'Digit5', 'Digit6', 'Digit7', 'Digit8', 'Digit9', 'Digit0', 'Minus', 'Equal']
    .forEach((code, index) => map(code, 0, index + 1));
  map('Backspace', 0, 13);
  map('Delete', 0, 13);
  map('NumpadAdd', 0, 13);
  map('Tab', 1, 0);
  ['KeyQ', 'KeyW', 'KeyE', 'KeyR', 'KeyT', 'KeyY', 'KeyU', 'KeyI', 'KeyO', 'KeyP', 'BracketLeft', 'BracketRight']
    .forEach((code, index) => map(code, 1, index + 1));
  map('Backslash', 1, 13);
  map('NumpadSubtract', 1, 13);
  map('CapsLock', 2, 0);
  map('ShiftLeft', 2, 0);
  map('ShiftRight', 2, 1);
  ['KeyA', 'KeyS', 'KeyD', 'KeyF', 'KeyG', 'KeyH', 'KeyJ', 'KeyK', 'KeyL', 'Semicolon', 'Quote']
    .forEach((code, index) => map(code, 2, index + 2));
  map('Enter', 2, 13);
  map('NumpadEnter', 2, 13);
  map('ControlLeft', 3, 0);
  map('ControlRight', 3, 0);
  map('AltLeft', 3, 1);
  map('AltRight', 3, 2);
  ['KeyZ', 'KeyX', 'KeyC', 'KeyV', 'KeyB', 'KeyN', 'KeyM', 'Comma', 'Period', 'Slash']
    .forEach((code, index) => map(code, 3, index + 3));
  map('Space', 3, 13);
  const HOME_CODE = 'Home';

  const CODE_LABEL = {
    Escape: 'Esc', Backquote: '`', Minus: '-', Equal: '=', Backspace: '⌫', Delete: 'Del', NumpadAdd: 'Num+',
    Tab: 'Tab', BracketLeft: '[', BracketRight: ']', Backslash: '\\', NumpadSubtract: 'Num−',
    CapsLock: 'Caps', ShiftLeft: '⇧L', ShiftRight: '⇧R', Semicolon: ';', Quote: "'", Enter: 'Enter', NumpadEnter: 'NumEnter',
    ControlLeft: 'Ctrl', ControlRight: 'CtrlR', AltLeft: isMac ? '⌥L' : 'AltL', AltRight: isMac ? '⌥R' : 'AltR',
    Comma: ',', Period: '.', Slash: '/', Space: 'Space', Home: 'Home',
  };
  function codeLabel(code) {
    if (CODE_LABEL[code]) return CODE_LABEL[code];
    if (code.startsWith('Key')) return code.slice(3);
    if (code.startsWith('Digit')) return code.slice(5);
    return code;
  }
  // Codes shown on each key cap (extras such as the numpad keys stay in the help only).
  const SHOWN_CODES = new Set(['NumpadAdd', 'NumpadSubtract', 'NumpadEnter', 'Delete', 'ControlRight']);
  function pcLabelListFor(row, column) {
    const labels = [];
    for (const [code, position] of KEYMAP) {
      if (position[0] === row && position[1] === column && !SHOWN_CODES.has(code)) labels.push(codeLabel(code));
    }
    return labels;
  }
  function pcLabelsFor(row, column) {
    return pcLabelListFor(row, column).join(' ');
  }

  // ---------------------------------------------------------------------------------------
  // DOM

  const $ = (id) => document.getElementById(id);
  const ui = {
    state: $('state'),
    canvas: $('screen'),
    overlay: $('overlay'),
    overlayTitle: $('overlay-title'),
    overlayText: $('overlay-text'),
    keyboard: $('keyboard'),
    status: $('statusline'),
    home: $('home'),
    audioButton: $('audio-button'),
    audioDot: $('audio-dot'),
    audioLabel: $('audio-label'),
    audioInfo: $('audio-info'),
    audioHint: $('audio-hint'),
    projectButton: $('project-button'),
    projectInput: $('project-input'),
    wavButton: $('wav-button'),
    wavInput: $('wav-input'),
    storageInfo: $('storage-info'),
    footer: $('footer'),
  };

  function setState(text, kind) {
    ui.state.textContent = text;
    ui.state.className = 'state' + (kind ? ' ' + kind : '');
  }

  function showOverlay(title, text, isError) {
    ui.overlayTitle.textContent = title;
    ui.overlayText.textContent = text || '';
    ui.overlay.classList.toggle('err', !!isError);
    ui.overlay.hidden = false;
  }

  const keyElements = [];
  function gridColumnFor(column) {
    if (column === 0) return 1;
    if (column <= 8) return column + 2;
    if (column <= 12) return column + 3;
    return 17;
  }

  function buildKeyboard() {
    for (let row = 0; row < ROWS; row++) {
      for (let column = 0; column < COLUMNS; column++) {
        const role = roleOf(row, column);
        const key = document.createElement('div');
        key.className = 'key';
        key.style.gridColumn = String(gridColumnFor(column));
        key.style.gridRow = String(row + 1);
        key.dataset.row = String(row);
        key.dataset.column = String(column);

        const legend = document.createElement('span');
        legend.className = 'legend';
        const tag = document.createElement('span');
        tag.className = 'tag';
        if (column === 0) {
          key.classList.add('left', 'role-' + role.toLowerCase());
          legend.textContent = role;
          tag.textContent = LEGEND[row][column] || '';
        } else if (column === COLUMNS - 1) {
          key.classList.add('right');
          legend.textContent = RIGHT_SYMBOLS[row];
          tag.textContent = role;
        } else {
          key.classList.add(role === 'PAD' ? 'pad' : 'grid');
          const legendText = LEGEND[row][column];
          if (legendText) {
            legend.textContent = legendText;
          } else {
            legend.textContent = CAP_PRINT[row + ',' + column] || '';
            legend.classList.add('small');
          }
          tag.textContent = role === 'PAD' ? 'pad' : 'grid';
        }
        // Computer key(s); narrow layouts show only the first one.
        const pc = document.createElement('span');
        pc.className = 'pc';
        const labels = pcLabelListFor(row, column);
        const main = document.createElement('span');
        main.textContent = labels[0] || '';
        pc.appendChild(main);
        if (labels.length > 1) {
          const more = document.createElement('span');
          more.className = 'pc-more';
          more.textContent = ' ' + labels.slice(1).join(' ');
          pc.appendChild(more);
        }

        key.append(legend, tag, pc);
        key.title = 'row ' + row + ' col ' + column + ' — ' + describeKey(row, column) + ' — computer: ' + (pcLabelsFor(row, column) || 'none');
        ui.keyboard.appendChild(key);
        keyElements.push({ element: key, level: -1 });
      }
    }
  }

  // Size the keys to the window and the screen to an integer number of device pixels per pixel.
  function layout() {
    const style = getComputedStyle(document.documentElement);
    const gap = parseFloat(style.getPropertyValue('--gap')) || 6;
    const device = document.querySelector('.device');
    const deviceStyle = getComputedStyle(device);
    const inner = device.clientWidth - parseFloat(deviceStyle.paddingLeft) - parseFloat(deviceStyle.paddingRight);
    const key = Math.max(18, Math.min(60, Math.floor((inner - 22 * gap) / 14)));
    document.documentElement.style.setProperty('--key', key + 'px');

    const keyboardWidth = 14 * key + 22 * gap;
    const narrow = EMBED || window.matchMedia('(max-width: 900px)').matches;
    const room = Math.min(keyboardWidth - (narrow ? 0 : 110), inner) - (EMBED ? 70 : 22);
    // Leave room for the keyboard below so the whole device fits in the window (embedded, the
    // host page grows the frame to whatever height the device needs).
    const heightRoom = EMBED ? Infinity : Math.max(HEIGHT, window.innerHeight - 430);
    const dpr = window.devicePixelRatio || 1;
    let scale = Math.floor(Math.min(room / WIDTH, heightRoom / HEIGHT) * dpr);
    scale = Math.max(1, scale);
    ui.canvas.style.width = (WIDTH * scale) / dpr + 'px';
    ui.canvas.style.height = (HEIGHT * scale) / dpr + 'px';
    reportSize();
  }

  function reportSize() {
    if (!EMBED || window.parent === window) return;
    const height = Math.ceil(document.querySelector('.device').getBoundingClientRect().height);
    window.parent.postMessage({ type: 'cardenza-size', height }, location.origin);
  }

  // ---------------------------------------------------------------------------------------
  // Module

  let mod = null;
  let booted = false;
  let failed = false;
  let imageData = null;
  let pixels32 = null;
  let lastPresented = -1;
  let lastBacklight = -1;
  const ctx2d = ui.canvas.getContext('2d', { alpha: false });

  // RGB565 -> RGBA (little endian 0xAABBGGRR), same expansion as CardenzaSimShot.
  const RGB565 = new Uint32Array(65536);
  for (let value = 0; value < 65536; value++) {
    const red = (((value >> 11) & 0x1f) * 255 / 31) | 0;
    const green = (((value >> 5) & 0x3f) * 255 / 63) | 0;
    const blue = ((value & 0x1f) * 255 / 31) | 0;
    RGB565[value] = (0xff000000 | (blue << 16) | (green << 8) | red) >>> 0;
  }

  // The guide page around an embedded device (tools/learn/guide.js) hears the firmware's app messages,
  // the same text a Cardenza sends it over USB.
  let guideListener = null;
  function pumpGuide() {
    if (!guideListener) return;
    for (let count = 0; count < 32; count++) {
      const text = mod.UTF8ToString(mod._cardenza_web_app_take());
      if (!text) break;
      try { guideListener(text); } catch (error) { console.error(error); }
    }
  }

  function present() {
    if (!booted) return;
    audio.onDeviceFrame();
    pumpGuide();
    const frame = mod._cardenza_web_frame_counter();
    if (frame === lastPresented) return;
    lastPresented = frame;

    const framebuffer = mod._cardenza_web_framebuffer() >> 1;
    const source = mod.HEAPU16;
    for (let index = 0; index < WIDTH * HEIGHT; index++) pixels32[index] = RGB565[source[framebuffer + index]];
    ctx2d.putImageData(imageData, 0, 0);

    const leds = mod._cardenza_web_leds();
    const heap = mod.HEAPU8;
    const enabled = heap[leds + 57] !== 0;
    const backlight = heap[leds + 58];
    for (let index = 0; index < 56; index++) {
      // The firmware gives the light each key emits on the device (PWM tier, master applied);
      // the eye sees roughly its square root. One LED colour for every key, as on the board.
      const light = enabled ? heap[leds + index] : 0;
      const slot = keyElements[index];
      if (slot.level === light) continue;
      slot.level = light;
      const brightness = Math.pow(light / 255, 0.45);
      slot.element.style.setProperty('--b', brightness.toFixed(3));
      slot.element.classList.toggle('lit', brightness > 0.35);
    }
    if (backlight !== lastBacklight) {
      lastBacklight = backlight;
      ui.canvas.style.filter = 'brightness(' + (0.25 + 0.75 * Math.sqrt(backlight / 255)).toFixed(3) + ')';
    }
  }

  function mountStorage(module) {
    const FS = module.FS;
    for (const mount of [STORAGE_MOUNT, INTERNAL_MOUNT]) {
      try {
        FS.mkdir(mount);
      } catch (error) { /* exists */ }
    }
    let persistent = true;
    try {
      if (typeof indexedDB === 'undefined') throw new Error('IndexedDB unavailable');
      // autoPersist writes every closed file, rename and delete back to IndexedDB.
      FS.mount(module.IDBFS, { autoPersist: true }, STORAGE_MOUNT);
      FS.mount(module.IDBFS, { autoPersist: true }, INTERNAL_MOUNT);
    } catch (error) {
      persistent = false;
      console.warn('CardenzaWeb: no persistent storage, using memory only', error);
    }
    if (!persistent) {
      ui.storageInfo.textContent = 'storage: memory only (not kept after reload)';
      ui.storageInfo.classList.add('err');
      return;
    }
    module.addRunDependency('cardenza-storage');
    FS.syncfs(true, (error) => {
      if (error) {
        console.warn('CardenzaWeb: loading saved storage failed', error);
        ui.storageInfo.textContent = 'storage: could not load saved data (' + error + ')';
        ui.storageInfo.classList.add('err');
      } else {
        ui.storageInfo.textContent = 'storage: kept in this browser';
      }
      module.removeRunDependency('cardenza-storage');
    });
  }

  // Settings are written a second after the last change; leaving the page writes them at once,
  // then commits /sdcard and /internal to IndexedDB.
  function flushStorage() {
    if (booted && mod && typeof mod._cardenza_web_flush_storage === 'function') {
      try { mod._cardenza_web_flush_storage(); } catch (error) { /* ignore */ }
    }
    persistNow();
  }

  function persistNow() {
    if (!mod || !mod.FS) return;
    try {
      mod.FS.syncfs(false, (error) => { if (error) console.warn('CardenzaWeb: sync failed', error); });
    } catch (error) { /* ignore */ }
  }

  function start() {
    if (typeof window.createCardenzaModule !== 'function') {
      fail('cardenza.js did not define createCardenzaModule');
      return;
    }
    setState('loading wasm…');
    showOverlay('LOADING', 'cardenza.wasm\nIf this stays, check the browser console.');
    window.createCardenzaModule({
      preRun: [mountStorage],
      print: (text) => console.log(text),
      printErr: (text) => console.warn(text),
      onAbort: (reason) => fail('The runtime stopped: ' + reason),
      cardenzaBooted: () => onBooted(),
      cardenzaFailed: (message) => fail(message),
      cardenzaPresent: () => present(),
    }).then((module) => {
      mod = module;
    }).catch((error) => fail(String(error && error.message || error)));
  }

  function onBooted() {
    if (booted || failed) return;
    // cardenzaBooted is called from main() before the module promise resolves.
    if (!mod) {
      setTimeout(onBooted, 0);
      return;
    }
    booted = true;
    releaseAll(); // anything pressed while loading was never sent to the device
    imageData = ctx2d.createImageData(WIDTH, HEIGHT);
    pixels32 = new Uint32Array(imageData.data.buffer);
    ui.overlay.hidden = true;
    const ms = Math.round(performance.now() - loadStarted);
    setState('running · booted in ' + ms + ' ms', 'ok');
    ui.footer.textContent = 'CardenzaSynth firmware app compiled to WebAssembly · boot ' + ms + ' ms · ' +
      (navigator.hardwareConcurrency ? navigator.hardwareConcurrency + ' cores · ' : '') + 'no threads';
    window.cardenzaBootMs = ms;
    present();
    audio.updateUi();
    if (EMBED && window.parent !== window) window.parent.postMessage({ type: 'cardenza-booted', ms }, location.origin);
  }

  function fail(message) {
    failed = true;
    console.error('CardenzaWeb:', message);
    setState('error', 'err');
    showOverlay('ERROR', message, true);
  }

  // ---------------------------------------------------------------------------------------
  // Input: every device key is held while any source holds it (computer keys, pointers, latch).

  const holders = Array.from({ length: ROWS * COLUMNS }, () => new Set());
  const pcHeld = new Map(); // code -> key index
  const pointerHeld = new Map(); // pointerId -> key index
  let homeSources = new Set();

  function setHeld(index, source, held) {
    const set = holders[index];
    const wasDown = set.size > 0;
    if (held) set.add(source); else set.delete(source);
    const isDown = set.size > 0;
    const element = keyElements[index].element;
    element.classList.toggle('down', isDown);
    element.classList.toggle('latched', set.has('latch'));
    if (wasDown !== isDown && booted) {
      mod._cardenza_web_set_key(Math.floor(index / COLUMNS), index % COLUMNS, isDown ? 1 : 0);
    }
  }

  function setHome(source, held) {
    const wasDown = homeSources.size > 0;
    if (held) homeSources.add(source); else homeSources.delete(source);
    const isDown = homeSources.size > 0;
    ui.home.classList.toggle('down', isDown);
    if (wasDown !== isDown && booted) mod._cardenza_web_set_home(isDown ? 1 : 0);
  }

  function releaseAll(reason) {
    let held = homeSources.size > 0;
    for (let index = 0; index < holders.length; index++) {
      if (holders[index].size === 0) continue;
      held = true;
      holders[index].clear();
      keyElements[index].element.classList.remove('down', 'latched');
    }
    pcHeld.clear();
    pointerHeld.clear();
    homeSources = new Set();
    ui.home.classList.remove('down');
    if (booted) mod._cardenza_web_release_all();
    if (reason && held) showStatus(reason);
  }

  function showStatus(html) {
    ui.status.innerHTML = html;
  }

  function escapeHtml(text) {
    return text.replace(/[&<>"]/g, (ch) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[ch]));
  }

  function reportKey(source, detail, row, column, held) {
    const what = row < 0 ? '<b>HOME</b> (unused; all stop is FN + ESC)' : '<b>row ' + row + ' col ' + column + '</b>  (' + escapeHtml(describeKey(row, column)) + ')';
    showStatus(source + (detail ? '  ' + escapeHtml(detail) : '') + '  →  ' + what + (held ? '' : '  ↑'));
  }

  function onKeyDown(event) {
    if (event.metaKey) {
      // Browser / OS shortcuts. macOS drops key-ups while Cmd is down, so let go of everything.
      if (event.code === 'MetaLeft' || event.code === 'MetaRight') releaseAll();
      return;
    }
    audio.userGesture();
    if (event.code === HOME_CODE) {
      event.preventDefault();
      if (!event.repeat) {
        setHome('pc:' + event.code, true);
        reportKey('PC', event.code, -1, -1, true);
      }
      return;
    }
    const position = KEYMAP.get(event.code);
    if (!position) {
      if (!event.repeat && event.code) showStatus('PC  ' + escapeHtml(event.code) + '  →  not mapped');
      return;
    }
    event.preventDefault();
    if (event.repeat) return; // the device repeats ADD / SUB itself
    const index = position[0] * COLUMNS + position[1];
    pcHeld.set(event.code, index);
    setHeld(index, 'pc:' + event.code, true);
    reportKey('PC', event.code, position[0], position[1], true);
  }

  function onKeyUp(event) {
    if (event.code === HOME_CODE) {
      event.preventDefault();
      setHome('pc:' + event.code, false);
      return;
    }
    const index = pcHeld.get(event.code);
    if (index === undefined) {
      if (KEYMAP.has(event.code)) event.preventDefault();
      return;
    }
    event.preventDefault();
    pcHeld.delete(event.code);
    setHeld(index, 'pc:' + event.code, false);
  }

  function keyIndexFromTarget(target) {
    const key = target && target.closest ? target.closest('.key') : null;
    if (!key) return -1;
    return Number(key.dataset.row) * COLUMNS + Number(key.dataset.column);
  }

  function onPointerDown(event) {
    if (event.button === 2) return; // right button latches, see contextmenu
    if (event.button !== 0 && event.pointerType === 'mouse') return;
    const index = keyIndexFromTarget(event.target);
    if (index < 0) return;
    event.preventDefault();
    audio.userGesture();
    try { ui.keyboard.setPointerCapture(event.pointerId); } catch (error) { /* ignore */ }
    const source = 'ptr:' + event.pointerId;
    pointerHeld.set(event.pointerId, index);
    const row = Math.floor(index / COLUMNS);
    const column = index % COLUMNS;
    // Pressing a latched key with the pointer releases the latch on release.
    if (holders[index].has('latch')) holders[index].delete('latch');
    setHeld(index, source, true);
    reportKey(event.pointerType === 'touch' ? 'TOUCH' : event.pointerType === 'pen' ? 'PEN' : 'MOUSE', '', row, column, true);
  }

  function onPointerUp(event) {
    const index = pointerHeld.get(event.pointerId);
    if (index === undefined) return;
    pointerHeld.delete(event.pointerId);
    setHeld(index, 'ptr:' + event.pointerId, false);
  }

  function onContextMenu(event) {
    event.preventDefault();
    const index = keyIndexFromTarget(event.target);
    if (index < 0) return;
    audio.userGesture();
    const latched = holders[index].has('latch');
    setHeld(index, 'latch', !latched);
    const row = Math.floor(index / COLUMNS);
    const column = index % COLUMNS;
    reportKey('LATCH', latched ? 'off' : 'on', row, column, !latched);
  }

  function bindInput() {
    window.addEventListener('keydown', onKeyDown, true);
    window.addEventListener('keyup', onKeyUp, true);
    window.addEventListener('blur', () => releaseAll('Window lost focus: all keys released.'));
    document.addEventListener('visibilitychange', () => {
      if (document.hidden) {
        releaseAll('Page hidden: all keys released.');
        flushStorage();
      }
    });
    window.addEventListener('pagehide', flushStorage);

    ui.keyboard.addEventListener('pointerdown', onPointerDown);
    ui.keyboard.addEventListener('pointerup', onPointerUp);
    ui.keyboard.addEventListener('pointercancel', onPointerUp);
    ui.keyboard.addEventListener('lostpointercapture', onPointerUp);
    ui.keyboard.addEventListener('contextmenu', onContextMenu);

    const homeDown = (event) => {
      event.preventDefault();
      audio.userGesture();
      try { ui.home.setPointerCapture(event.pointerId); } catch (error) { /* ignore */ }
      setHome('ptr:' + event.pointerId, true);
      reportKey(event.pointerType === 'touch' ? 'TOUCH' : 'MOUSE', '', -1, -1, true);
    };
    const homeUp = (event) => setHome('ptr:' + event.pointerId, false);
    ui.home.addEventListener('pointerdown', homeDown);
    ui.home.addEventListener('pointerup', homeUp);
    ui.home.addEventListener('pointercancel', homeUp);
    ui.home.addEventListener('contextmenu', (event) => event.preventDefault());

    // Buttons must not keep focus: Space / Enter belong to the device.
    for (const button of [ui.audioButton, ui.projectButton, ui.wavButton]) {
      button.addEventListener('mouseup', () => button.blur());
    }
    ui.audioButton.addEventListener('click', () => audio.toggle());
    ui.projectButton.addEventListener('click', () => ui.projectInput.click());
    ui.projectInput.addEventListener('change', () => {
      const files = Array.from(ui.projectInput.files || []);
      ui.projectInput.value = '';
      importProjectFiles(files);
    });
    ui.wavButton.addEventListener('click', () => ui.wavInput.click());
    ui.wavInput.addEventListener('change', () => {
      const files = Array.from(ui.wavInput.files || []);
      ui.wavInput.value = '';
      importWavFiles(files);
    });
    // Touch only grants a user activation on pointerup / touchend.
    for (const type of ['pointerdown', 'pointerup', 'touchend']) {
      document.addEventListener(type, () => audio.userGesture(), true);
    }
    window.addEventListener('resize', layout);
    // Ctrl is the device's CTRL key, and Ctrl+W / Ctrl+T cannot be intercepted: ask before leaving.
    window.addEventListener('beforeunload', (event) => {
      if (pcHeld.has('ControlLeft') || pcHeld.has('ControlRight')) {
        event.preventDefault();
        event.returnValue = '';
      }
    });
  }

  // ---------------------------------------------------------------------------------------
  // Audio: the synth renders on the main thread (where the wasm module lives); an AudioWorklet
  // plays the posted frames from a ring buffer. ScriptProcessorNode is the fallback where
  // AudioWorklet is unavailable (for example plain http on a LAN address).

  const audio = (() => {
    const BASE_TARGET = 2048; // source frames buffered ahead (about 46 ms at 44.1 kHz)
    const MAX_TARGET = 6144;
    const CHUNK = 256; // one AMY block
    let context = null;
    let workletLoaded = false;
    let node = null;
    let mode = 'off'; // off | starting | worklet | script
    let wanted = true; // start on the first gesture unless the user switched audio off
    let sourceRate = 44100;
    let posted = 0;
    let consumed = 0;
    let underruns = 0;
    let target = BASE_TARGET;
    let lastUnderrunAt = 0;
    let silentClock = 0;
    let feedTimer = 0;
    // ScriptProcessor fallback state.
    let fifo = new Float32Array(16384);
    let fifoStart = 0;
    let fifoEnd = 0;
    let fifoPosition = 0;

    function running() {
      return (mode === 'worklet' || mode === 'script') && context && context.state === 'running';
    }

    function render(frames) {
      const pointer = mod._cardenza_web_render(frames) >> 2;
      return mod.HEAPF32.subarray(pointer, pointer + frames);
    }

    function feed() {
      if (!booted || mode !== 'worklet' || !running()) return;
      let buffered = posted - consumed;
      let guard = 0;
      while (buffered < target && guard++ < 32) {
        const frames = Math.min(8192, Math.ceil((target - buffered) / CHUNK) * CHUNK);
        const samples = render(frames).slice();
        node.port.postMessage({ samples }, [samples.buffer]);
        posted += frames;
        buffered += frames;
      }
    }

    // Without an audio clock (no gesture yet, audio off or suspended) the synth still has to run
    // in real time: the sequencer and live notes queue events and meters decay per rendered block.
    function renderSilently() {
      const now = performance.now();
      if (!silentClock) silentClock = now;
      let frames = Math.round((now - silentClock) * sourceRate / 1000);
      if (frames < CHUNK) return;
      silentClock += frames * 1000 / sourceRate;
      if (frames > 8192) frames = 8192;
      render(frames);
    }

    function onDeviceFrame() {
      if (running()) {
        silentClock = 0;
        if (mode === 'worklet') feed();
        return;
      }
      renderSilently();
    }

    function onWorkletMessage(event) {
      const data = event.data;
      if (typeof data.consumed === 'number') consumed = data.consumed;
      if (typeof data.underruns === 'number' && data.underruns !== underruns) {
        underruns = data.underruns;
        const now = performance.now();
        if (now - lastUnderrunAt > 250 && target < MAX_TARGET) target += 512;
        lastUnderrunAt = now;
        updateUi();
      } else if (target > BASE_TARGET && performance.now() - lastUnderrunAt > 30000) {
        target -= 256;
        lastUnderrunAt = performance.now();
        updateUi();
      }
      feed();
    }

    function scriptProcess(event) {
      const output = event.outputBuffer.getChannelData(0);
      const step = sourceRate / context.sampleRate;
      const needed = Math.ceil(output.length * step) + 2;
      if (fifoEnd - fifoStart < needed) {
        // Compact, then render enough for this callback plus one chunk of slack.
        fifo.copyWithin(0, fifoStart, fifoEnd);
        fifoEnd -= fifoStart;
        fifoPosition -= fifoStart;
        fifoStart = 0;
        const frames = Math.ceil((needed - fifoEnd) / CHUNK) * CHUNK + CHUNK;
        if (fifoEnd + frames > fifo.length) {
          const bigger = new Float32Array((fifoEnd + frames) * 2);
          bigger.set(fifo.subarray(0, fifoEnd));
          fifo = bigger;
        }
        fifo.set(render(frames), fifoEnd);
        fifoEnd += frames;
      }
      let position = fifoPosition;
      for (let index = 0; index < output.length; index++) {
        const base = Math.floor(position);
        const frac = position - base;
        const a = fifo[base];
        output[index] = frac === 0 ? a : a + (fifo[base + 1] - a) * frac;
        position += step;
      }
      fifoPosition = position;
      fifoStart = Math.floor(position);
    }

    async function startOutput() {
      if (!booted || mode !== 'off') return;
      mode = 'starting';
      updateUi();
      sourceRate = mod._cardenza_web_sample_rate();
      try {
        const Context = window.AudioContext || window.webkitAudioContext;
        if (!Context) throw new Error('Web Audio is not available');
        if (!context) {
          try {
            context = new Context({ sampleRate: sourceRate, latencyHint: 'interactive' });
          } catch (error) {
            context = new Context({ latencyHint: 'interactive' });
          }
          context.addEventListener('statechange', updateUi);
        }
        // Without a user activation (a touch pointerdown is not one) the context stays suspended;
        // the next gesture resumes it, see userGesture().
        if (context.state !== 'running') context.resume().then(updateUi, () => {});

        posted = 0;
        consumed = 0;
        underruns = 0;
        target = BASE_TARGET;
        if (context.audioWorklet && typeof AudioWorkletNode === 'function') {
          if (!workletLoaded) {
            await context.audioWorklet.addModule('cardenza-audio-worklet.js');
            workletLoaded = true;
          }
          node = new AudioWorkletNode(context, 'cardenza-output', {
            numberOfInputs: 0,
            numberOfOutputs: 1,
            outputChannelCount: [2],
            processorOptions: { sourceRate, prefillFrames: 1024, reportEveryQuanta: 2 },
          });
          node.port.onmessage = onWorkletMessage;
          node.connect(context.destination);
          mode = 'worklet';
          feed();
          clearInterval(feedTimer);
          feedTimer = setInterval(feed, 10);
        } else {
          node = context.createScriptProcessor(2048, 0, 1);
          fifoStart = fifoEnd = fifoPosition = 0;
          node.onaudioprocess = scriptProcess;
          node.connect(context.destination);
          mode = 'script';
        }
      } catch (error) {
        console.warn('CardenzaWeb: audio could not start', error);
        mode = 'off';
        ui.audioInfo.textContent = 'audio failed: ' + (error && error.message || error);
        ui.audioInfo.classList.add('err');
      }
      updateUi();
    }

    function stopOutput() {
      clearInterval(feedTimer);
      if (node) {
        try { node.disconnect(); } catch (error) { /* ignore */ }
        if (node.port) node.port.onmessage = null;
        node.onaudioprocess = null;
        node = null;
      }
      if (context) context.suspend();
      mode = 'off';
      updateUi();
    }

    function userGesture() {
      if (!booted || !wanted) return;
      if (mode === 'off') startOutput();
      else if (context && context.state === 'suspended') context.resume().then(updateUi, () => {});
    }

    function toggle() {
      if (mode === 'off') {
        wanted = true;
        startOutput();
      } else {
        wanted = false;
        stopOutput();
      }
    }

    function updateUi() {
      const isRunning = running();
      ui.audioDot.className = 'tool-dot' + (isRunning ? ' on' : mode === 'starting' ? ' warn' : '');
      ui.audioLabel.textContent = isRunning ? 'Audio on' : mode === 'starting' ? 'Starting audio…' : 'Start audio';
      ui.audioHint.hidden = !booted || isRunning || !wanted || mode === 'starting';
      if (isRunning) {
        const ms = (frames) => Math.round(frames * 1000 / sourceRate);
        const output = Math.round(((context.baseLatency || 0) + (context.outputLatency || 0)) * 1000);
        ui.audioInfo.classList.remove('err');
        ui.audioInfo.textContent = (mode === 'worklet' ? 'AudioWorklet' : 'ScriptProcessor') + ' · ' + context.sampleRate + ' Hz' +
          (context.sampleRate !== sourceRate ? ' (resampled from ' + sourceRate + ')' : '') +
          ' · buffer ' + (mode === 'worklet' ? ms(target) : ms(4096 * sourceRate / context.sampleRate)) + ' ms' +
          (output ? ' + output ' + output + ' ms' : '') + (underruns ? ' · underruns ' + underruns : '');
      } else if (mode === 'off' && !ui.audioInfo.classList.contains('err')) {
        ui.audioInfo.textContent = wanted ? '' : 'audio off';
      }
    }

    return { onDeviceFrame, userGesture, toggle, updateUi, stats: () => ({ mode, posted, consumed, target, underruns, state: context && context.state, sampleRate: context && context.sampleRate }) };
  })();

  // ---------------------------------------------------------------------------------------
  // Samples: the sampler lists WAV files from Device::Storage::SampleDirectoryPath().

  const MAX_SAMPLE_FRAMES = 44100 * 5; // sampler::kMaxSampleFrames

  function readMonoPcm16Info(bytes) {
    const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    const text = (offset) => String.fromCharCode(bytes[offset], bytes[offset + 1], bytes[offset + 2], bytes[offset + 3]);
    if (bytes.length < 44 || text(0) !== 'RIFF' || text(8) !== 'WAVE') return null;
    let offset = 12;
    let format = null;
    let dataBytes = 0;
    while (offset + 8 <= bytes.length) {
      const id = text(offset);
      const size = view.getUint32(offset + 4, true);
      if (id === 'fmt ' && size >= 16) {
        format = {
          tag: view.getUint16(offset + 8, true),
          channels: view.getUint16(offset + 10, true),
          rate: view.getUint32(offset + 12, true),
          bits: view.getUint16(offset + 22, true),
        };
      } else if (id === 'data') {
        dataBytes = Math.min(size, bytes.length - offset - 8);
        break;
      }
      offset += 8 + size + (size & 1);
    }
    if (!format || format.tag !== 1 || format.channels !== 1 || format.bits !== 16) return null;
    if (format.rate < 8000 || format.rate > 96000) return null;
    return { rate: format.rate, frames: dataBytes / 2 };
  }

  function encodeMonoPcm16Wav(samples, rate) {
    const bytes = new Uint8Array(44 + samples.length * 2);
    const view = new DataView(bytes.buffer);
    const writeText = (offset, text) => { for (let i = 0; i < 4; i++) bytes[offset + i] = text.charCodeAt(i); };
    writeText(0, 'RIFF');
    view.setUint32(4, 36 + samples.length * 2, true);
    writeText(8, 'WAVE');
    writeText(12, 'fmt ');
    view.setUint32(16, 16, true);
    view.setUint16(20, 1, true);
    view.setUint16(22, 1, true);
    view.setUint32(24, rate, true);
    view.setUint32(28, rate * 2, true);
    view.setUint16(32, 2, true);
    view.setUint16(34, 16, true);
    writeText(36, 'data');
    view.setUint32(40, samples.length * 2, true);
    for (let i = 0; i < samples.length; i++) {
      const value = Math.max(-1, Math.min(1, samples[i]));
      view.setInt16(44 + i * 2, value < 0 ? value * 32768 : value * 32767, true);
    }
    return bytes;
  }

  async function toDeviceWav(file) {
    const original = new Uint8Array(await file.arrayBuffer());
    const info = readMonoPcm16Info(original);
    if (info && info.frames <= MAX_SAMPLE_FRAMES) return { bytes: original, note: '' };
    // Anything else (stereo, 24-bit, float, mp3, too long): decode, mix down, 44.1 kHz PCM16.
    const Offline = window.OfflineAudioContext || window.webkitOfflineAudioContext;
    const decoder = new Offline(1, 1, 44100);
    const buffer = await decoder.decodeAudioData(original.buffer.slice(0));
    const frames = Math.min(buffer.length, MAX_SAMPLE_FRAMES);
    const mono = new Float32Array(frames);
    for (let channel = 0; channel < buffer.numberOfChannels; channel++) {
      const data = buffer.getChannelData(channel);
      for (let i = 0; i < frames; i++) mono[i] += data[i] / buffer.numberOfChannels;
    }
    const notes = ['converted to mono 16-bit 44.1 kHz'];
    if (buffer.length > MAX_SAMPLE_FRAMES) notes.push('trimmed to 5 s');
    return { bytes: encodeMonoPcm16Wav(mono, 44100), note: notes.join(', ') };
  }

  function sampleFileName(name) {
    let base = name.replace(/\.[^.]*$/, '').replace(/[^A-Za-z0-9 _\-]+/g, '_').trim() || 'sample';
    if (base.length > 40) base = base.slice(0, 40);
    return base + '.wav';
  }

  async function importWavFiles(files) {
    if (!booted || files.length === 0) return;
    const directory = mod.UTF8ToString(mod._cardenza_web_sample_directory());
    const done = [];
    for (const file of files) {
      try {
        const converted = await toDeviceWav(file);
        const name = sampleFileName(file.name);
        mod.FS.writeFile(directory + '/' + name, converted.bytes);
        done.push(name + (converted.note ? ' (' + converted.note + ')' : ''));
      } catch (error) {
        done.push(file.name + ': ' + (error && error.message || 'not a readable audio file'));
      }
    }
    persistNow();
    showStatus('Samples: ' + escapeHtml(done.join(' · ')) + '  →  INSTR on the SAMPLER track, SAMPLE page, FILE');
  }

  // A name the device lists (IsProjectNameValid in ProjectStorage.cpp): "DEMO-2(1).ccproj" is not one.
  function projectBaseName(fileName) {
    let base = fileName.replace(/\.ccproj(\.bak)?$/i, '').replace(/[^A-Za-z0-9 _\-]+/g, '_').slice(0, 24).trim() || 'PROJECT';
    if (/^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$/i.test(base)) base = base + '_';
    return base;
  }

  // Never over another project: the same file is already there, anything else gets a free name.
  function freeProjectName(directory, base, bytes) {
    for (let number = 1; ; number++) {
      const suffix = number === 1 ? '' : ' ' + number;
      const name = base.slice(0, 24 - suffix.length).trim() + suffix;
      const path = directory + '/' + name + '.ccproj';
      if (!mod.FS.analyzePath(path).exists) return name;
      const existing = mod.FS.readFile(path);
      if (existing.length === bytes.length && existing.every((value, index) => value === bytes[index])) return name;
    }
  }

  async function importProjectFiles(files) {
    if (!booted || files.length === 0) return;
    const directory = mod.UTF8ToString(mod._cardenza_web_project_directory());
    const done = [];
    let opening = null;
    for (const file of files) {
      try {
        const bytes = new Uint8Array(await file.arrayBuffer());
        if (new TextDecoder().decode(bytes.subarray(0, 17)) !== 'CARDENZA_PROJECT ') {
          done.push(file.name + ': not a Cardenza project');
          continue;
        }
        const name = freeProjectName(directory, projectBaseName(file.name), bytes);
        mod.FS.writeFile(directory + '/' + name + '.ccproj', bytes);
        done.push(name);
        opening = opening || name;
      } catch (error) {
        done.push(file.name + ': ' + (error && error.message || 'could not be read'));
      }
    }
    if (opening) mod.ccall('cardenza_web_open_project', 'number', ['string'], [opening]);
    persistNow();
    showStatus('Projects: ' + escapeHtml(done.join(' · ')) + (opening ? '  →  all in SETUP, FILES' : ''));
  }

  // ---------------------------------------------------------------------------------------

  buildKeyboard();
  layout();
  bindInput();
  setState('loading…');

  window.CardenzaWeb = {
    start,
    fail,
    audioStats: () => audio.stats(),
    module: () => mod,
    persist: persistNow,
  };

  // Keys pressed from outside the frame: the Learn page's "go to this screen" steps and a real
  // Cardenza on USB. They hold keys like any other source, so they never fight the mouse.
  window.cardenzaEmbed = {
    booted: () => booted,
    press(row, column, down, source) {
      if (row < 0 || row >= ROWS || column < 0 || column >= COLUMNS) return;
      setHeld(row * COLUMNS + column, source || 'remote', !!down);
      if (down) reportKey(source === 'usb' ? 'USB' : 'GUIDE', '', row, column, true);
    },
    home(down, source) {
      setHome(source || 'remote', !!down);
      if (down) reportKey(source === 'usb' ? 'USB' : 'GUIDE', '', -1, -1, true);
    },
    releaseAll: () => releaseAll(),
    gesture: () => audio.userGesture(),
    // The guide: a message to the firmware, and who hears its messages (one listener).
    guide(text) {
      if (booted) mod.ccall('cardenza_web_app_send', null, ['string'], [String(text)]);
    },
    listen(fn) { guideListener = typeof fn === 'function' ? fn : null; },
  };
  if (EMBED) {
    new ResizeObserver(reportSize).observe(document.querySelector('.device'));
    reportSize();
  }
})();
