// The guide's concept animations (src/guide/LessonPage.tsx plays them): a music box that becomes a
// sequencer, four boxes that make a part, parts in a row that make a song. Drawn in the product's
// look: black, the screen's colours and track colours, 4x4 Bayer dither, a green playhead. And the
// Launcher's lessons (updating it, installing firmware): its screens as it draws them, over the keys.
// GuideAnim.create(container, name) -> { setStep(i), destroy() }; GuideAnim.thumb(el, name) loops a
// small one without sound; GuideAnim.setSound(on) (from a click: browsers start audio on a gesture).
(function () {
  'use strict';
  const NS = 'http://www.w3.org/2000/svg';
  const C = { ink: '#e8e8e0', dim: '#a2a29a', faint: '#6a6a63', line: '#262624', line2: '#34342f', play: '#48ff78', fn: '#ffd030',
    t: ['#ff5cb0', '#4898ff', '#b0ec38', '#ffa428'] };
  const STEP = 0.15;  // seconds per step: 100 BPM in sixteenths
  const BAYER = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5];
  let uid = 0;

  function el(parent, tag, attrs, text) {
    const node = document.createElementNS(NS, tag);
    for (const key in attrs) node.setAttribute(key, attrs[key]);
    if (text !== undefined) node.textContent = text;
    if (parent) parent.appendChild(node);
    return node;
  }
  // Text in both languages; the page shows one (html[data-lang]).
  function label(parent, x, y, zh, en, attrs) {
    const g = el(parent, 'g', {});
    el(g, 'text', Object.assign({ x, y, 'data-l': 'zh' }, attrs || {}), zh);
    el(g, 'text', Object.assign({ x, y, 'data-l': 'en' }, attrs || {}), en);
    return g;
  }
  function makeSvg(container) {
    const svg = el(null, 'svg', { viewBox: '0 0 640 360', preserveAspectRatio: 'xMidYMid meet', role: 'img' });
    svg.style.fontFamily = 'JetBrains Mono, ui-monospace, monospace';
    const defs = el(svg, 'defs', {});
    const id = 'ga' + (++uid);
    // Dither fills: dither(color, density 1..16) -> url(#...).
    const made = {};
    svg.dither = (color, density) => {
      const key = color + density;
      if (!made[key]) {
        const name = `${id}-${Object.keys(made).length}`;
        const pattern = el(defs, 'pattern', { id: name, width: 4, height: 4, patternUnits: 'userSpaceOnUse' });
        BAYER.forEach((level, i) => { if (level < density) el(pattern, 'rect', { x: i % 4, y: i >> 2, width: 1, height: 1, fill: color }); });
        made[key] = `url(#${name})`;
      }
      return made[key];
    };
    container.appendChild(svg);
    return svg;
  }
  const ease = (k) => (k < 0 ? 0 : k > 1 ? 1 : k < 0.5 ? 2 * k * k : 1 - Math.pow(-2 * k + 2, 2) / 2);

  // ---- Sound ------------------------------------------------------------------------------------
  let audio = null;
  let soundOn = false;
  function ctx() {
    if (!audio) {
      const Ctx = window.AudioContext || window.webkitAudioContext;
      if (!Ctx) return null;
      audio = new Ctx();
      audio.master = audio.createGain();
      audio.master.gain.value = 0.35;
      audio.master.connect(audio.destination);
    }
    if (audio.state === 'suspended') audio.resume();
    return audio;
  }
  function noise(a, seconds) {
    const buffer = a.createBuffer(1, Math.ceil(a.sampleRate * seconds), a.sampleRate);
    const d = buffer.getChannelData(0);
    for (let i = 0; i < d.length; i++) d[i] = Math.random() * 2 - 1;
    const source = a.createBufferSource();
    source.buffer = buffer;
    return source;
  }
  function env(a, peak, decay, t) {
    const g = a.createGain();
    g.gain.setValueAtTime(0.0001, t);
    g.gain.exponentialRampToValueAtTime(peak, t + 0.005);
    g.gain.exponentialRampToValueAtTime(0.0001, t + decay);
    g.connect(a.master);
    return g;
  }
  const hz = (midi) => 440 * Math.pow(2, (midi - 69) / 12);
  function play(kind, midi) {
    if (!soundOn) return;
    const a = ctx();
    if (!a) return;
    const t = a.currentTime + 0.01;
    const tone = (type, f, peak, decay, detune) => {
      const o = a.createOscillator();
      o.type = type;
      o.frequency.value = f;
      if (detune) o.detune.value = detune;
      o.connect(env(a, peak, decay, t));
      o.start(t);
      o.stop(t + decay + 0.05);
      return o;
    };
    if (kind === 'bell') { tone('sine', hz(midi), 0.5, 1.3); tone('sine', hz(midi) * 2.76, 0.12, 0.4); tone('sine', hz(midi) * 5.4, 0.05, 0.15); }
    else if (kind === 'piano') { tone('triangle', hz(midi), 0.45, 0.7); tone('sine', hz(midi) * 2, 0.12, 0.3); }
    else if (kind === 'chord') [0, 4, 7].forEach((s) => tone('triangle', hz(midi + s), 0.16, 0.9, s * 3));
    else if (kind === 'bass') {
      const o = a.createOscillator();
      const f = a.createBiquadFilter();
      o.type = 'sawtooth';
      o.frequency.value = hz(midi);
      f.type = 'lowpass';
      f.frequency.value = 420;
      o.connect(f);
      f.connect(env(a, 0.5, 0.35, t));
      o.start(t);
      o.stop(t + 0.4);
    } else if (kind === 'kick') {
      const o = a.createOscillator();
      o.frequency.setValueAtTime(140, t);
      o.frequency.exponentialRampToValueAtTime(42, t + 0.18);
      o.connect(env(a, 0.9, 0.3, t));
      o.start(t);
      o.stop(t + 0.32);
    } else if (kind === 'snare' || kind === 'hat') {
      const n = noise(a, 0.25);
      const f = a.createBiquadFilter();
      f.type = kind === 'hat' ? 'highpass' : 'bandpass';
      f.frequency.value = kind === 'hat' ? 7000 : 1800;
      n.connect(f);
      f.connect(env(a, kind === 'hat' ? 0.18 : 0.45, kind === 'hat' ? 0.05 : 0.16, t));
      n.start(t);
      n.stop(t + 0.25);
    }
  }

  // ---- Music ------------------------------------------------------------------------------------
  // Eight comb teeth / rows, C major from C4 up. The melody: a folk tune everyone knows.
  const SCALE = [60, 62, 64, 65, 67, 69, 71, 72];
  const NAMES = ['C', 'D', 'E', 'F', 'G', 'A', 'B', 'C'];
  const MELODY = [[0, 0], [2, 0], [4, 4], [6, 4], [8, 5], [10, 5], [12, 4]];  // [step, row]
  const DRUMS = { 0: 'kick', 4: 'snare', 8: 'kick', 12: 'snare' };
  const FILL = { 0: 'kick', 4: 'snare', 8: 'kick', 10: 'kick', 12: 'snare', 13: 'snare', 14: 'snare', 15: 'snare' };
  const CHORDS = { 0: 48, 8: 53, 12: 48 };
  const BASS = { 0: 36, 6: 43, 8: 41, 12: 36 };
  const drums = (map) => (s) => map[s] || (s % 2 === 0 ? 'hat' : null);
  const TRACKS = [
    { zh: '鼓', en: 'DRUMS', hits: drums(DRUMS) },
    { zh: '钢琴', en: 'PIANO', hits: (s) => MELODY.find((m) => m[0] === s) },
    { zh: '和弦', en: 'CHORDS', hits: (s) => CHORDS[s] },
    { zh: '贝斯', en: 'BASS', hits: (s) => BASS[s] },
  ];
  function playHit(track, hit, s) {
    if (!hit) return;
    if (track === 0) { play(hit); if (hit !== 'hat' && s % 2 === 0) play('hat'); }
    if (track === 1) play('piano', SCALE[hit[1]]);
    if (track === 2) play('chord', hit + 12);
    if (track === 3) play('bass', hit);
  }
  const sound = (track, s) => playHit(track, TRACKS[track].hits(s), s);

  // A song: every track has patterns of its own (numbered as they were made) and each part picks one
  // per track, or none. A is the tune's first line; B, like it but its own, the second line (F F E E
  // D D C, then D back to A) with its own chords and bass and a drum fill; the intro has only a
  // kick, a long chord and long bass notes. The bass has four patterns: A's, B's, the intro's, and a
  // busier one jumping octaves for the second A.
  const MELODY_B = [[0, 3], [2, 3], [4, 2], [6, 2], [8, 1], [10, 1], [12, 0], [14, 1]];
  const PATTERNS = [
    { 1: drums({ 0: 'kick', 8: 'kick' }), 2: drums(DRUMS), 3: drums(FILL) },
    { 1: (s) => MELODY.find((m) => m[0] === s), 2: (s) => MELODY_B.find((m) => m[0] === s) },
    { 1: (s) => CHORDS[s], 2: (s) => ({ 0: 53, 4: 48, 8: 55, 12: 48 })[s], 3: (s) => ({ 0: 48, 8: 53 })[s] },
    {
      1: (s) => BASS[s],
      2: (s) => ({ 0: 41, 4: 36, 8: 43, 12: 36, 14: 43 })[s],
      3: (s) => ({ 0: 36, 8: 41 })[s],
      4: (s) => ({ 0: 36, 2: 48, 4: 36, 6: 43, 8: 41, 10: 53, 12: 36, 14: 43 })[s],
    },
  ];
  const SONG = [
    { zh: '前奏', en: 'INTRO', p: [1, 0, 3, 3] },
    { zh: 'A', en: 'A', p: [2, 1, 1, 1] },
    { zh: 'A', en: 'A', p: [2, 1, 1, 4] },
    { zh: 'B', en: 'B', p: [3, 2, 2, 2] },
    { zh: 'A', en: 'A', p: [2, 1, 1, 1] },
  ];
  const hitOf = (track, pattern, s) => (pattern ? PATTERNS[track][pattern](s) : null);

  // ---- Pieces ---------------------------------------------------------------------------------
  // A music box seen from the front. The pinned drum stands upright and turns towards the comb on its
  // left, so a row of the drum is a tooth of the comb (low at the bottom, as in the grid it unrolls
  // into), its pins come to the comb at an even speed, and the comb is where the music is now. The
  // teeth (all alike, kept simple) are cut from one steel plate, every tip on the drum; a plucked
  // tooth bends and rings. The drum's surface lines and the marks on its top turn with it; pins and
  // lines narrow and darken towards its sides, so it reads round while everything on it moves at one
  // speed.
  function musicBox(parent, svg, x, y, w, h, color, pins) {
    const sc = h / 150;
    const lanes = 8, laneH = h / lanes;
    const rowY = (r) => y + (lanes - r - 0.5) * laneH;
    const g = el(parent, 'g', {});
    const fx = x - 12 * sc - 4, fy = y - 14 * sc - 4, fw = w + 24 * sc + 8, fh = h + 28 * sc + 8;
    el(g, 'rect', { x: fx, y: fy, width: fw, height: fh, rx: 8, fill: '#0e0e0d', stroke: C.line2 });
    const combW = w * 0.24;
    const cylX = x + combW - 6 * sc, cylW = x + w - cylX;
    const contact = cylX + cylW * 0.1;
    const visibleSteps = 8;  // the front half of the drum
    const pxPerStep = (x + w - 4 * sc - contact) / visibleSteps;
    // The drum: spindle, round bottom, a body shaded round with dither columns, the top cap.
    const capRy = 8 * sc;
    for (const sy of [y - capRy - 7 * sc, y + h]) el(g, 'rect', { x: cylX + cylW / 2 - 2.5 * sc, y: sy, width: 5 * sc, height: capRy + 7 * sc, fill: C.faint });
    el(g, 'ellipse', { cx: cylX + cylW / 2, cy: y + h, rx: cylW / 2, ry: capRy, fill: svg.dither(color, 3) });
    const cols = 11;
    for (let b = 0; b < cols; b++) {
      const k = Math.sin(Math.PI * (b + 0.5) / cols);
      el(g, 'rect', { x: cylX + b * cylW / cols, y, width: cylW / cols + 0.5, height: h, fill: svg.dither(color, Math.max(1, Math.round(k * 6))) });
    }
    el(g, 'ellipse', { cx: cylX + cylW / 2, cy: y, rx: cylW / 2, ry: capRy, fill: '#121211', stroke: color, 'stroke-opacity': 0.7 });
    el(g, 'path', { d: `M ${cylX} ${y + h} A ${cylW / 2} ${capRy} 0 0 0 ${cylX + cylW} ${y + h}`, fill: 'none', stroke: color, 'stroke-opacity': 0.7 });
    for (const lx of [cylX, cylX + cylW]) el(g, 'line', { x1: lx, y1: y, x2: lx, y2: y + h, stroke: color, 'stroke-opacity': 0.55 });
    // A faint ring under each tooth: the pins of one note go round on it.
    for (let r = 0; r < lanes; r++) el(g, 'line', { x1: cylX + 1, y1: rowY(r), x2: cylX + cylW - 1, y2: rowY(r), stroke: '#000', 'stroke-opacity': 0.35, 'stroke-width': 1 });
    // Lines along the drum, one per step, that turn with it; marks round the top cap.
    const depth = (px) => Math.sin(Math.PI * Math.min(1, Math.max(0, (px - cylX) / cylW)));
    const ribs = [];
    for (let k = 0; k < 16; k++) ribs.push(el(g, 'line', { y1: y + 1, y2: y + h - 1, stroke: '#000' }));
    const capCx = cylX + cylW / 2, capRx = cylW / 2;
    const marks = [];
    const markCount = 12;
    for (let k = 0; k < markCount; k++) marks.push(el(g, 'line', { stroke: C.ink, 'stroke-width': 1.5 * sc, 'stroke-linecap': 'round' }));
    // The marks move like the surface at the front; a whole turn moves them a whole number of marks.
    const markStep = 2 * Math.PI / markCount;
    const markTurn = Math.max(markStep, Math.round(16 * pxPerStep / capRx / markStep) * markStep);
    // The pins, under the teeth.
    const pinLayer = el(g, 'g', {});
    const pinR = Math.max(2, 4.4 * sc);
    const pinNodes = pins.map(() => el(pinLayer, 'ellipse', { rx: pinR, ry: pinR, fill: C.ink, stroke: '#000', 'stroke-width': 0.8 }));
    // The comb: a plate with one tooth per row, all the same length.
    const baseX = () => x + combW * 0.45;
    el(g, 'rect', { x, y: y - 3, width: baseX() + 2 - x, height: h + 6, fill: svg.dither(C.dim, 5), stroke: C.faint });
    for (const yy of [y + h * 0.18, y + h * 0.82]) el(g, 'circle', { cx: x + combW * 0.1, cy: yy, r: 2.6 * sc, fill: '#0e0e0d', stroke: C.faint });
    const tipX = contact + 3 * sc;
    const teeth = [];
    for (let r = 0; r < lanes; r++) teeth.push(el(g, 'polygon', { fill: C.dim }));
    return {
      g,
      // phase: turns of the drum (0..1 per 16 steps).
      draw(phase) {
        const now = phase * 16;
        const until = (step) => ((step - now) % 16 + 16) % 16;  // steps until the pin reaches the comb
        const leftmost = (cylX - contact) / pxPerStep, rightmost = (cylX + cylW - contact) / pxPerStep;
        ribs.forEach((rib, k) => {
          let d = until(k + 0.5);
          if (d > rightmost) d -= 16;
          const rx = contact + d * pxPerStep, depthAt = depth(rx);
          const on = d >= leftmost && d <= rightmost;
          const u = Math.min(1, Math.abs(rx - capCx) / capRx);
          rib.setAttribute('x1', rx);
          rib.setAttribute('x2', rx);
          rib.setAttribute('y1', y + capRy * Math.sqrt(1 - u * u));
          rib.setAttribute('stroke-width', (0.8 + 2 * depthAt) * Math.max(0.6, sc));
          rib.setAttribute('stroke-opacity', on ? 0.25 + 0.5 * depthAt : 0);
        });
        const turn = -phase * markTurn;
        marks.forEach((mark, k) => {
          const a = k * markStep - turn;
          const c = Math.cos(a), s = Math.sin(a);
          mark.setAttribute('x1', capCx + capRx * 0.92 * c);
          mark.setAttribute('y1', y + capRy * 0.92 * s);
          mark.setAttribute('x2', capCx + capRx * 0.7 * c);
          mark.setAttribute('y2', y + capRy * 0.7 * s);
          mark.setAttribute('stroke-opacity', 0.25 + 0.45 * (s + 1) / 2);
        });
        pins.forEach(([step, row], i) => {
          let d = until(step);
          if (d > 15.4) d -= 16;  // just past: going under the tooth
          const node = pinNodes[i];
          const px = contact + d * pxPerStep, depthAt = depth(px);
          node.setAttribute('cx', px);
          node.setAttribute('cy', rowY(row));
          node.setAttribute('rx', pinR * (0.45 + 0.55 * depthAt));
          const fade = d < 0 ? 1 + d / 0.6 : d > visibleSteps - 0.7 ? (visibleSteps - d) / 0.7 : 1;
          node.setAttribute('opacity', Math.max(0, Math.min(1, fade)) * (0.5 + 0.5 * depthAt));
        });
        teeth.forEach((tooth, row) => {
          let next = 99, since = 99;
          pins.forEach(([step, r]) => {
            if (r !== row) return;
            const d = until(step);
            next = Math.min(next, d);
            since = Math.min(since, (16 - d) % 16);
          });
          // The pin lifts the tip a little, lets go, and the tooth rings down.
          let bend = 0;
          if (next < 0.3) bend = -(1 - next / 0.3) * 1.2 * sc;
          else if (since < 3) bend = 1 * sc * Math.exp(-since / 0.7) * Math.cos(since * Math.PI * 2.5);
          const cy = rowY(row), bx = baseX(row), hb = laneH * 0.2, ht = laneH * 0.1;
          tooth.setAttribute('points', `${bx},${cy - hb} ${tipX},${cy - ht + bend} ${tipX},${cy + ht + bend} ${bx},${cy + hb}`);
          tooth.setAttribute('fill', since < 1.6 ? color : C.dim);
        });
      },
    };
  }

  // A row of 16 steps (a track's pattern): cells lit where it has notes, the playhead green.
  function stepRow(parent, svg, x, y, w, h, color, has) {
    const g = el(parent, 'g', {});
    const cells = [];
    const cw = w / 16;
    for (let s = 0; s < 16; s++) {
      el(g, 'rect', { x: x + s * cw + 1, y, width: cw - 2, height: h, fill: s % 4 === 0 ? '#111110' : '#0b0b0a', stroke: C.line });
      cells.push(el(g, 'rect', { x: x + s * cw + 2, y: y + 1, width: cw - 4, height: h - 2, fill: has(s) ? svg.dither(color, 12) : 'none' }));
    }
    return {
      g,
      draw(step) {
        cells.forEach((cell, s) => {
          const on = has(s);
          cell.setAttribute('fill', on ? (s === step ? C.ink : svg.dither(color, 12)) : s === step ? svg.dither(C.play, 4) : 'none');
        });
      },
      refresh() { cells.forEach((cell, s) => cell.setAttribute('fill', has(s) ? svg.dither(color, 12) : 'none')); },
    };
  }

  // ---- Scenes -------------------------------------------------------------------------------------
  // Each scene builds its layers once; frame(t, step, from, to, k) blends from one lesson step to the next.
  const SCENES = {};

  SCENES['music-box'] = (svg, thumb) => {
    const pins = MELODY.map((m) => m.slice());
    const box = el(svg, 'g', {});
    const mb = musicBox(box, svg, 176, 92, 250, 170, C.t[1], pins);
    label(box, 320, 322, '梳齿碰到凸起就发出声音', 'A tooth meets a pin: it sounds', { fill: C.faint, 'font-size': 13, 'text-anchor': 'middle' });
    const flat = el(svg, 'g', {});
    const gx = 70, gy = 60, gw = 400, gh = 224, rows = 8;
    const has = (s, r) => pins.some((p) => p[0] === s && p[1] === r);
    const cellNodes = [];
    for (let r = 0; r < rows; r++) {
      el(flat, 'text', { x: gx - 12, y: gy + (rows - r - 0.5) * gh / rows + 4, fill: C.faint, 'font-size': 11, 'text-anchor': 'end' }, NAMES[r]);
      for (let s = 0; s < 16; s++) {
        const cell = el(flat, 'rect', { x: gx + s * gw / 16 + 1, y: gy + (rows - r - 1) * gh / rows + 1, width: gw / 16 - 2, height: gh / rows - 2,
          fill: s % 4 === 0 ? '#111110' : '#0b0b0a', stroke: C.line });
        cell.dataset.s = s;
        cell.dataset.r = r;
        cellNodes.push(cell);
      }
    }
    for (let s = 0; s < 16; s++) el(flat, 'text', { x: gx + (s + 0.5) * gw / 16, y: gy + gh + 16, fill: s % 4 === 0 ? C.dim : C.faint, 'font-size': 10, 'text-anchor': 'middle' }, s + 1);
    // One bar of 4/4: four beats of four steps.
    for (let b = 0; b < 4; b++) {
      el(flat, 'line', { x1: gx + b * gw / 4 + 3, y1: gy - 7, x2: gx + (b + 1) * gw / 4 - 3, y2: gy - 7, stroke: C.line2, 'stroke-width': 2 });
      label(flat, gx + (b + 0.5) * gw / 4, gy - 13, `第 ${b + 1} 拍`, `beat ${b + 1}`, { fill: C.dim, 'font-size': 11, 'text-anchor': 'middle' });
    }
    label(flat, gx - 12, gy - 13, '音高', 'pitch', { fill: C.faint, 'font-size': 11, 'text-anchor': 'end' });
    const head = el(flat, 'rect', { y: gy - 4, width: 2, height: gh + 8, fill: C.play });
    label(flat, gx + gw / 2, gy + gh + 36, '时间 →（16 格 = 一小节）', 'time → (16 steps = one bar)', { fill: C.faint, 'font-size': 12, 'text-anchor': 'middle' });
    // The device's GRID: the same 16 steps, four by four.
    const keys = el(svg, 'g', {});
    const kx = 500, ky = 92, ks = 30;
    label(keys, kx + 2 * ks + 3, ky - 14, '机器右边的 4×4 键', 'the 4×4 keys', { fill: C.dim, 'font-size': 12, 'text-anchor': 'middle' });
    const keyNodes = [];
    for (let i = 0; i < 16; i++) {
      const kxx = kx + (i % 4) * (ks + 4), kyy = ky + (i >> 2) * (ks + 4);
      el(keys, 'rect', { x: kxx, y: kyy, width: ks, height: ks, rx: 4, fill: '#171715', stroke: '#3e3e38', 'stroke-dasharray': '4 2.5' });
      keyNodes.push(el(keys, 'rect', { x: kxx + 2, y: kyy + 2, width: ks - 4, height: ks - 4, rx: 3, fill: 'rgb(255,246,226)', opacity: 0 }));
      el(keys, 'text', { x: kxx + 4, y: kyy + 11, fill: C.faint, 'font-size': 8 }, i + 1);
    }
    flat.addEventListener('click', (event) => {
      const cell = event.target.closest('rect[data-s]');
      if (!cell) return;
      const s = Number(cell.dataset.s), r = Number(cell.dataset.r);
      const at = pins.findIndex((p) => p[0] === s && p[1] === r);
      if (at >= 0) pins.splice(at, 1);
      else { pins.push([s, r]); play('bell', SCALE[r]); }
    });
    let last = -1;
    const frame = (t, step, from, to, k) => {
      const show = (phase) => (phase === to ? k : phase === from ? 1 - k : 0);
      const boxVis = show(0), flatVis = show(1) + show(2), keysVis = show(2);
      box.setAttribute('opacity', boxVis);
      box.setAttribute('transform', `translate(0 ${-(1 - boxVis) * 30}) `);
      flat.setAttribute('opacity', flatVis);
      // Unrolled from the drum: its rows are already the grid's rows, time opens out to the right.
      flat.setAttribute('transform', `translate(${(1 - keysVis) * 60 + gx} 0) scale(${0.3 + 0.7 * ease(flatVis)} 1) translate(${-gx} 0)`);
      flat.style.pointerEvents = to === 2 && !thumb ? 'auto' : 'none';
      flat.style.cursor = to === 2 ? 'pointer' : 'default';
      keys.setAttribute('opacity', keysVis);
      const phase = (t / STEP / 16) % 1;
      mb.draw(phase);
      const s = Math.floor(t / STEP) % 16;
      head.setAttribute('x', gx + (t / STEP % 16) * gw / 16 - 1);
      cellNodes.forEach((cell) => {
        const cs = Number(cell.dataset.s), cr = Number(cell.dataset.r);
        cell.setAttribute('fill', has(cs, cr) ? (cs === s ? C.ink : svg.dither(C.t[1], 12)) : cs % 4 === 0 ? '#111110' : '#0b0b0a');
      });
      keyNodes.forEach((node, i) => node.setAttribute('opacity', i === s ? 0.95 : pins.some((p) => p[0] === i) ? 0.35 : 0));
      if (s !== last) {
        last = s;
        // A music box sings an octave up.
        if (!thumb) pins.filter((p) => p[0] === s).forEach((p) => play(to === 0 ? 'bell' : 'piano', SCALE[p[1]] + (to === 0 ? 12 : 0)));
      }
    };
    return frame;
  };

  SCENES['four-boxes'] = (svg, thumb) => {
    const one = el(svg, 'g', {});
    const single = musicBox(one, svg, 210, 100, 220, 140, C.t[1], MELODY);
    label(one, 320, 300, '钢琴', 'PIANO', { fill: C.t[1], 'font-size': 14, 'text-anchor': 'middle', 'font-weight': 700 });
    const four = el(svg, 'g', {});
    const boxes = TRACKS.map((track, i) => {
      const x = 52 + (i % 2) * 300, y = 52 + (i >> 1) * 160;
      const pins = [];
      for (let s = 0; s < 16; s++) {
        const hit = track.hits(s);
        if (!hit) continue;
        if (i === 0) pins.push([s, hit === 'kick' ? 1 : hit === 'snare' ? 4 : 6]);
        else if (i === 1) pins.push([s, hit[1]]);
        else pins.push([s, i === 2 ? 2 : 0], [s, i === 2 ? 5 : 3]);
      }
      const b = musicBox(four, svg, x + 40, y + 10, 160, 86, C.t[i], pins);
      label(four, x + 120, y + 128, track.zh, track.en, { fill: C.t[i], 'font-size': 13, 'text-anchor': 'middle', 'font-weight': 700 });
      return b;
    });
    const lanes = el(svg, 'g', {});
    const rows = TRACKS.map((track, i) => {
      el(lanes, 'rect', { x: 40, y: 64 + i * 58, width: 44, height: 40, rx: 3, fill: C.t[i] });
      el(lanes, 'text', { x: 62, y: 89 + i * 58, fill: '#000', 'font-size': 14, 'font-weight': 700, 'text-anchor': 'middle' }, 'T' + (i + 1));
      label(lanes, 96, 81 + i * 58, track.zh, track.en, { fill: C.t[i], 'font-size': 11 });
      el(lanes, 'text', { x: 96, y: 97 + i * 58, fill: C.faint, 'font-size': 10 }, 'P01');
      return stepRow(lanes, svg, 150, 64 + i * 58, 450, 40, C.t[i], (s) => !!track.hits(s));
    });
    const head = el(lanes, 'rect', { y: 56, width: 2, height: 4 * 58 + 2, fill: C.play });
    label(lanes, 375, 312, '四轨一起播放 = 一段', 'Four tracks playing together = one part', { fill: C.dim, 'font-size': 13, 'text-anchor': 'middle' });
    el(lanes, 'rect', { x: 32, y: 54, width: 576, height: 4 * 58 + 4, rx: 6, fill: 'none', stroke: C.fn, 'stroke-dasharray': '5 3.5' });
    let last = -1;
    const frame = (t, step, from, to, k) => {
      const show = (phase) => (phase === to ? k : phase === from ? 1 - k : 0);
      one.setAttribute('opacity', show(0));
      four.setAttribute('opacity', show(1));
      four.setAttribute('transform', `translate(320 180) scale(${0.8 + 0.2 * ease(show(1))}) translate(-320 -180)`);
      lanes.setAttribute('opacity', show(2));
      const phase = (t / STEP / 16) % 1;
      single.draw(phase);
      boxes.forEach((b) => b.draw(phase));
      const s = Math.floor(t / STEP) % 16;
      rows.forEach((row) => row.draw(s));
      head.setAttribute('x', 150 + (t / STEP % 16) * 450 / 16 - 1);
      if (s !== last) {
        last = s;
        if (thumb) return;
        if (to === 0) { const m = MELODY.find((n) => n[0] === s); if (m) play('piano', SCALE[m[1]]); }
        else for (let i = 0; i < 4; i++) sound(i, s, false);
      }
    };
    return frame;
  };

  SCENES['boxes-in-a-row'] = (svg, thumb) => {
    // A part drawn small: four lanes of hits in the track colours, from the patterns it picks.
    const part = (parent, x, y, w, h, picks) => {
      const g = el(parent, 'g', {});
      el(g, 'rect', { x, y, width: w, height: h, rx: 6, fill: '#0e0e0d', stroke: C.line2 });
      const lh = (h - 16) / 4;
      for (let i = 0; i < 4; i++) {
        for (let s = 0; s < 16; s++) {
          if (!hitOf(i, picks[i], s)) continue;
          el(g, 'rect', { x: x + 8 + s * (w - 16) / 16, y: y + 8 + i * lh + 1, width: (w - 16) / 16 - 1.5, height: lh - 3, fill: svg.dither(C.t[i], 12) });
        }
      }
      const frame = el(g, 'rect', { x: x - 3, y: y - 3, width: w + 6, height: h + 6, rx: 8, fill: 'none', stroke: C.play, 'stroke-width': 2, opacity: 0 });
      return { g, frame };
    };
    const A = SONG[1].p;
    const single = el(svg, 'g', {});
    part(single, 220, 110, 200, 120, A);
    label(single, 320, 270, '≈ 2.4 秒', '≈ 2.4 seconds', { fill: C.dim, 'font-size': 14, 'text-anchor': 'middle' });
    const loneHead = el(single, 'rect', { y: 104, width: 2, height: 132, fill: C.play });
    // The parts in a row.
    const row = el(svg, 'g', {});
    const pw = 104, px = (i) => 28 + i * 120;
    const parts = SONG.map((section, i) => {
      const p = part(row, px(i), 110, pw, 100, section.p);
      label(row, px(i) + pw / 2, 236, section.zh, section.en,
        { fill: section.zh === 'B' ? C.fn : C.dim, 'font-size': 13, 'text-anchor': 'middle', 'font-weight': 700 });
      return p;
    });
    const tape = el(row, 'rect', { y: 100, width: 2, height: 120, fill: C.play });
    label(row, 320, 290, '时间 →', 'time →', { fill: C.faint, 'font-size': 13, 'text-anchor': 'middle' });
    // The SONG page: a column per part, a row per track, the pattern it plays there (-- none).
    const song = el(svg, 'g', {});
    const cells = [];
    const cx = (c) => 140 + c * 94;
    SONG.forEach((section, c) => {
      el(song, 'text', { x: cx(c), y: 62, fill: C.dim, 'font-size': 12 }, String(c + 1).padStart(2, '0'));
      for (let r = 0; r < 4; r++) {
        const x = cx(c), y = 74 + r * 54, pattern = section.p[r];
        cells.push(el(song, 'rect', { x: x - 4, y: y - 4, width: 86, height: 46, rx: 3, fill: 'none', stroke: C.play, 'stroke-width': 2, opacity: 0, 'data-c': c }));
        el(song, 'text', { x, y: y + 22, fill: pattern ? C.ink : C.faint, 'font-size': 20, 'font-weight': 700 }, pattern ? 'P0' + pattern : '--');
        el(song, 'rect', { x, y: y + 32, width: 76, height: 3, fill: C.t[r], opacity: pattern ? 1 : 0.35 });
      }
    });
    for (let r = 0; r < 4; r++) {
      el(song, 'rect', { x: 66, y: 74 + r * 54, width: 54, height: 30, rx: 3, fill: C.t[r] });
      el(song, 'text', { x: 93, y: 94 + r * 54, fill: '#000', 'font-size': 14, 'font-weight': 700, 'text-anchor': 'middle' }, 'T' + (r + 1));
    }
    el(song, 'rect', { x: 534, y: 300, width: 64, height: 22, rx: 2, fill: C.fn });
    el(song, 'text', { x: 566, y: 316, fill: '#000', 'font-size': 13, 'font-weight': 700, 'text-anchor': 'middle' }, 'SONG');
    label(song, 66, 318, '3 TAPE · SONG 页', '3 TAPE · the SONG page', { fill: C.faint, 'font-size': 12 });
    let last = -1;
    const frame = (t, step, from, to, k) => {
      const show = (phase) => (phase === to ? k : phase === from ? 1 - k : 0);
      single.setAttribute('opacity', show(0));
      row.setAttribute('opacity', show(1));
      song.setAttribute('opacity', show(2));
      const at = t / STEP;
      const s = Math.floor(at) % 16;
      const current = Math.floor(at / 16) % SONG.length;
      loneHead.setAttribute('x', 228 + (at % 16) * 184 / 16);
      tape.setAttribute('x', px(current) + 8 + (at % 16) * (pw - 16) / 16);
      parts.forEach((p, i) => p.frame.setAttribute('opacity', i === current ? 1 : 0));
      cells.forEach((cell) => cell.setAttribute('opacity', Number(cell.dataset.c) === current ? 1 : 0));
      if (s !== last) {
        last = s;
        if (thumb) return;
        const picks = to === 0 ? A : SONG[current].p;
        for (let i = 0; i < 4; i++) playHit(i, hitOf(i, picks[i], s), s);
      }
    };
    return frame;
  };

  // ---- The Launcher -------------------------------------------------------------------------------
  // The 203 Launcher (github.com/203-Systems/203-launcher), the system that comes up first and installs
  // firmware, drawn as Cardenza's 240 x 135 screen shows it (its src/display.cpp): a black 24-pixel
  // rail (203 Systems on the home cards; inside, the page's name over a back arrow), white pages, the
  // chosen row black, the next home cards waiting at the right edge in greys, its 5 x 7 font at 1 to 4
  // times. Under the screen, the keys as the Launcher reads them, by their prints (the yellow arrows of
  // ; , . / move, OK is Enter, ESC goes back): a step's keys framed, the one pressed now lit. The words
  // on the screen are the Launcher's own. Keep this in step with the Launcher when its flow changes.
  const LS = 1.6, LX = 128, LY = 14;  // the screen in the stage: 384 x 216 at (128, 14)
  const grey = (v) => `rgb(${v},${v},${v})`;
  const HOME = [
    ['Software', 'Launcher', 'Launch installed software'],
    ['Download', 'Softwares', 'Get software online'],
    ['File', 'Browser', 'Browse files on SD'],
    ['Remote', 'Access', 'Manage from browser'],
    ['USB', 'Mode', 'SD card over USB'],
    ['System', 'Settings', 'Display, Wi-Fi, power'],
  ];
  // The caps as the web simulator prints them; the Launcher's direction keys carry yellow arrows.
  const CAPS = [
    ['ESC', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '↑'],
    ['TAB', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '↓'],
    ['FN', '', 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', "'", '↵'],
    ['CTRL', '', '', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', '▶'],
  ];
  const ARROWS = { '2,11': '↑', '3,10': '←', '3,11': '↓', '3,12': '→' };
  const ROLE = { 0: '#ff5a36', 1: '#36d6e6', 2: C.fn, 3: '#a070ff' };
  const KEY_AT = { ESC: [0, 0], OK: [2, 13], ';': [2, 11], ',': [3, 10], '.': [3, 11], '/': [3, 12] };
  CAPS.forEach((row, r) => row.forEach((cap, c) => { if (/^[A-Z0-9]$/.test(cap)) KEY_AT[cap] = [r, c]; }));

  function launcher(svg) {
    const root = el(svg, 'g', {});
    el(root, 'rect', { x: 96, y: 4, width: 448, height: 342, rx: 14, fill: '#0e0e0d', stroke: C.line2 });
    el(root, 'rect', { x: LX - 6, y: LY - 6, width: 240 * LS + 12, height: 135 * LS + 12, rx: 6, fill: '#000', stroke: C.line });
    const clip = 'lc' + (++uid);
    el(el(svg.querySelector('defs'), 'clipPath', { id: clip }), 'rect', { width: 240, height: 135 });
    const screen = el(root, 'g', { transform: `translate(${LX} ${LY}) scale(${LS})`, 'clip-path': `url(#${clip})` });
    const pages = [];
    const page = (background) => {
      const g = el(screen, 'g', { display: 'none' });
      el(g, 'rect', { width: 240, height: 135, fill: background || '#fff' });
      pages.push(g);
      return g;
    };
    // Text with its top at y, as the Launcher's text() places its 5 x 7 glyphs (6 pixels apart).
    const text = (g, x, y, words, scale, v, extra) => el(g, 'text', Object.assign({ x, y: y + 7 * scale, fill: grey(v),
      'font-size': 9.6 * scale, 'letter-spacing': 0.24 * scale, 'font-weight': scale > 1 ? 700 : 500 }, extra || {}), words);
    const vertical = (g, words) => el(g, 'text', { transform: 'translate(8 8) rotate(90)', fill: grey(240), 'font-size': 9.6, 'letter-spacing': 0.24 }, words);
    const homeRail = (g) => {
      el(g, 'rect', { width: 24, height: 135, fill: '#000' });
      vertical(g, '203 Systems');
      for (let bar = 0; bar < 4; bar++) el(g, 'rect', { x: 5 + bar * 4, y: 127 - bar * 3, width: 2, height: 3 + bar * 3, fill: grey(235) });
    };
    const rail = (g, title) => {
      el(g, 'rect', { width: 24, height: 135, fill: '#000' });
      vertical(g, title.length > 16 ? title.slice(0, 15) + '.' : title);
      el(g, 'path', { d: 'M5 122.5h14M5 122.5l5.5-5.5M5 122.5l5.5 5.5', stroke: grey(235), fill: 'none' });
    };
    const button = (g, x, width, words, chosen) => {
      el(g, 'rect', { x, y: 96, width, height: 31, fill: grey(chosen ? 0 : 232) });
      text(g, x + width / 2, 103.5, words, 2, chosen ? 255 : 0, { 'text-anchor': 'middle' });
    };
    const L = {
      root,
      // Unplugged: a dark screen.
      off() {
        const g = page('#000');
        el(g, 'path', { d: 'M12 2v14M8.5 5.5L12 2l3.5 3.5M12 12l-5-3V7M12 14l5-3V9', transform: 'translate(108 50)', stroke: grey(60), fill: 'none', 'stroke-width': 1.5 });
        el(g, 'circle', { cx: 120, cy: 69, r: 2.5, stroke: grey(60), fill: 'none', 'stroke-width': 1.5 });
        return g;
      },
      // The start: the 203 logo, and a line under it (or none).
      splash(line) {
        const g = page('#000');
        text(g, 120, 34, '203', 6, 255, { 'text-anchor': 'middle', 'font-weight': 800 });
        if (line) text(g, 120, 104, line, 1, 180, { 'text-anchor': 'middle' });
        return g;
      },
      // Home card i: the first is black, the others white; the cards after it wait at the right edge.
      home(i) {
        const g = page(grey(i === 0 ? 0 : 255));
        [189, 202, 209.5].forEach((edge, n) => {
          if (i + n + 1 < HOME.length) el(g, 'rect', { x: 24 + edge, width: 216 - edge, height: 135, fill: grey(255 - (n + 1) * 42) });
        });
        const ink = i === 0 ? 255 : 0;
        text(g, 36, 59, HOME[i][0], 3, ink);
        text(g, 36, 83, HOME[i][1], 3, ink);
        text(g, 36, 118, HOME[i][2], 1, i === 0 ? 145 : 120);
        homeRail(g);
        return g;
      },
      // A menu: rows [label, value, bars] four at a time from `first`, the chosen one black.
      list(title, rows, chosen, first) {
        const g = page();
        first = first || 0;
        el(g, 'rect', { x: 30, y: 4 + (chosen - first) * 32, width: 200, height: 32, fill: grey(20) });
        rows.slice(first, first + 4).forEach(([label, value, bars], n) => {
          const y = 4 + n * 32, on = first + n === chosen;
          text(g, 36, y + 12, label, 1, on ? 255 : 0);
          if (value) text(g, 228, y + 12.5, value, 1, on ? 205 : 115, { 'text-anchor': 'end' });
          if (bars !== undefined) {
            for (let bar = 0; bar < 4; bar++) {
              el(g, 'rect', { x: 205 + bar * 5, y: y + 21 - bar * 3, width: 3, height: 3 + bar * 3,
                fill: grey(bar < bars ? (on ? 235 : 35) : (on ? 85 : 180)) });
            }
          }
        });
        rail(g, title);
        return g;
      },
      // Installed software: the chosen one opens into a light panel with Launch and Modify.
      software(rows, chosen) {
        const g = page();
        let y = 4;
        rows.forEach(([name, version], i) => {
          if (i === chosen) {
            el(g, 'rect', { x: 30, y, width: 200, height: 80, fill: grey(230) });
            text(g, 36, y + 9, name, 1, 0);
            text(g, 36, y + 21, version, 1, 95);
            el(g, 'rect', { x: 36, y: y + 40, width: 90, height: 30, fill: grey(0) });
            text(g, 81, y + 48, 'Launch', 2, 255, { 'text-anchor': 'middle' });
            el(g, 'rect', { x: 132, y: y + 40, width: 90, height: 30, fill: grey(255) });
            text(g, 177, y + 48, 'Modify', 2, 0, { 'text-anchor': 'middle' });
            y += 80;
          } else {
            text(g, 36, y + 9, name, 1, 0);
            text(g, 36, y + 19, version, 1, 110);
            y += 32;
          }
        });
        rail(g, 'Software Launcher');
        return g;
      },
      // A question with two buttons, `chosen` black (the Launcher starts on Back).
      confirm(title, heading, left, right, chosen) {
        const g = page();
        text(g, 36, 12, heading, 2, 0);
        button(g, 36, 92, left, chosen === 0);
        button(g, 136, 92, right, chosen === 1);
        rail(g, title);
        return g;
      },
      // Work going on: its name, and with a fraction how far it is.
      busy(title, words, fraction) {
        const g = page();
        el(g, 'path', { d: 'M40 8v8M36.5 12.5L40 16l3.5-3.5M35 19.5h10', stroke: grey(0), fill: 'none' });
        text(g, 36, 30, words, 2, 0);
        if (fraction !== undefined) {
          text(g, 36, 66, `${Math.round(fraction * 100)}%`, 3, 0);
          el(g, 'rect', { x: 36, y: 116, width: 192, height: 5, fill: grey(222) });
          el(g, 'rect', { x: 36, y: 116, width: 192 * fraction, height: 5, fill: grey(0) });
        }
        rail(g, title);
        return g;
      },
      notice(title, heading, detail) {
        const g = page();
        text(g, 36, 14, heading, 2, 0);
        if (detail) text(g, 36, 40, detail, 1, 95);
        rail(g, title);
        return g;
      },
      // Where to install: the flash as a bar (regions [size, free]), the chosen place black under its name.
      target(name, needs, regions, chosen, label, detail, action) {
        const g = page();
        text(g, 36, 8, name, 2, 0);
        text(g, 36, 27, `Needs ${needs}`, 1, 110);
        const total = regions.reduce((sum, region) => sum + region[0], 0);
        let x = 36;
        regions.forEach(([size, free], i) => {
          const width = 192 * size / total;
          el(g, 'rect', { x, y: 41, width: width - 1, height: 18, fill: grey(i === chosen ? 0 : free ? 248 : 195) });
          if (free && i !== chosen) {
            for (let y = 42; y < 59; y += 4) for (let px = x + 1; px < x + width - 1; px += 4) el(g, 'rect', { x: px, y, width: 1, height: 1, fill: grey(155) });
          }
          x += width;
        });
        text(g, 36, 66, label, 2, 0);
        text(g, 36, 85, detail, 1, 110);
        button(g, 36, 192, action, true);
        rail(g, 'Install');
        return g;
      },
      usb(connected) {
        const g = page();
        text(g, 36, 29, 'USB', 4, 0);
        text(g, 36, 75, connected ? 'CONNECTED' : 'WAITING', 2, 0);
        text(g, 36, 115, connected ? 'EJECT BEFORE EXIT' : 'CONNECT TO COMPUTER', 1, 110);
        rail(g, 'USB Mode');
        return g;
      },
      password(typed) {
        const g = page();
        text(g, 36, 10, 'Wi-Fi password', 2, 0);
        text(g, 36, 30, 'Home-WiFi', 1, 110);
        el(g, 'rect', { x: 36.5, y: 46.5, width: 191, height: 26, fill: 'none', stroke: grey(0) });
        text(g, 42, 52, '*'.repeat(typed), 2, 0);
        el(g, 'rect', { x: 42 + typed * 12, y: 51, width: 2, height: 16, fill: grey(0) });
        text(g, 228, 115, 'TAB SHOW', 1, 110, { 'text-anchor': 'end' });
        rail(g, 'Wi-Fi');
        return g;
      },
      // The Groovebox once started: its first screen (the manual's picture of it).
      groovebox() {
        const g = page('#000');
        el(g, 'image', { href: 'img/instr-boot.png', width: 240, height: 135 });
        return g;
      },
      show(node) { pages.forEach((g) => g.setAttribute('display', g === node ? 'inline' : 'none')); },
    };
    // The keys: the edge columns and the GRID set apart as on the device.
    const keyX = (c) => 109.5 + c * 29 + (c >= 1 ? 6 : 0) + (c >= 9 ? 6 : 0) + (c >= 13 ? 6 : 0);
    const keyY = (r) => 248 + r * 24;
    const keys = {};
    CAPS.forEach((row, r) => row.forEach((cap, c) => {
      const x = keyX(c), y = keyY(r), grid = c >= 9 && c <= 12;
      el(root, 'rect', { x, y, width: 26, height: 20, rx: 3, fill: '#171715', stroke: '#3e3e38', 'stroke-dasharray': grid ? '4 2.5' : 'none' });
      const lit = el(root, 'rect', { x: x + 1.5, y: y + 1.5, width: 23, height: 17, rx: 2, fill: 'rgb(255,246,226)', opacity: 0 });
      el(root, 'text', { x: x + 13, y: y + 14, fill: c === 0 ? ROLE[r] : C.dim, 'font-size': c === 0 ? 7.5 : 9, 'text-anchor': 'middle', 'font-weight': 600 }, cap);
      const arrow = ARROWS[`${r},${c}`];
      if (arrow) el(root, 'text', { x: x + 21, y: y + 8.5, fill: C.fn, 'font-size': 7.5, 'text-anchor': 'middle' }, arrow);
      const frame = el(root, 'rect', { x: x - 2.5, y: y - 2.5, width: 31, height: 25, rx: 4, fill: 'none', stroke: C.fn, 'stroke-width': 1.5, 'stroke-dasharray': '4 2.5', opacity: 0 });
      keys[`${r},${c}`] = { lit, frame };
    }));
    const at = (name) => keys[(KEY_AT[name] || [-1, -1]).join(',')];
    L.keys = (framed, pressed) => {
      Object.values(keys).forEach((key) => { key.frame.setAttribute('opacity', 0); key.lit.setAttribute('opacity', 0); });
      framed.forEach((name) => { const key = at(name); if (key) key.frame.setAttribute('opacity', 1); });
      const key = pressed && at(pressed);
      if (key) key.lit.setAttribute('opacity', 0.9);
    };
    return L;
  }

  // A Launcher lesson: per step, frames [screen, the key pressed as it ends (a word: its letters typed
  // one by one through it), seconds], played in a loop; the step's keys stay framed.
  function launcherScene(build) {
    const scene = (svg) => {
      const L = launcher(svg);
      const steps = build(L);
      return (t, step, from, to, k) => {
        const s = steps[Math.min(to, steps.length - 1)];
        const framed = s.keys || [...new Set(s.frames.map((f) => f[1]).filter((key) => key && key.length < 4))];
        const total = s.frames.reduce((sum, f) => sum + f[2], 0) + 1.4;
        let left = t % total, i = 0;
        while (i < s.frames.length - 1 && left >= s.frames[i][2]) { left -= s.frames[i][2]; i++; }
        const [screen, key, seconds] = s.frames[i];
        let pressed = null;
        if (key && key.length > 3) pressed = left < seconds - 0.2 && left % 0.2 < 0.12 ? key[Math.floor(left / 0.2) % key.length] : null;
        else if (key && left > seconds - 0.35 && left < seconds) pressed = key;
        L.show(screen);
        L.keys(framed, pressed);
        L.root.setAttribute('opacity', 0.35 + 0.65 * k);
      };
    };
    scene.silent = true;
    return scene;
  }

  // What the Launcher's lists hold (its catalog for Cardenza as of 2026-11).
  const CATALOG = [['Cardenza Synth'], ['203-Launcher'], ['GLIDE'], ['MiniAcid'], ['Microgroove'], ['MicroHydra'], ['M5Tuna'],
    ['M5Porkchop'], ['Guitar Audio Tools'], ['Game Station'], ['Next Page'], ['Refine Search'], ['Check for Updates']];
  const NETWORKS = [['Studio-5G', '', 3], ['Home-WiFi', '', 4], ['Cafe', '', 2]];
  const ACTIONS = [['Install'], ['Download to SD'], ['Add to favorites']];
  const FILE_ACTIONS = [['Install'], ['Bind to key'], ['New Folder'], ['Edit name']];
  const replug = (L) => ({ frames: [[L.off(), null, 1.2], [L.splash(), null, 1.6], [L.home(0), null, 1.8]] });

  SCENES['update-launcher'] = launcherScene((L) => {
    const home = HOME.map((_, i) => L.home(i));
    const catalog = (chosen) => L.list('Downloads', CATALOG, chosen);
    const [cat0, cat1] = [catalog(0), catalog(1)];
    const ask = (chosen) => L.confirm('Downloads', 'Update Launcher?', 'Update', 'Back', chosen);
    const [askBack, askUpdate] = [ask(1), ask(0)];
    const updating = (fraction) => L.busy('Downloads', 'Update Launcher', fraction);
    return [
      replug(L),
      { frames: [[L.off(), null, 0.8], [L.splash('Press Func to enter Launcher'), 'OK', 1.8], [home[0], null, 1.8]] },
      { keys: ['/', ',', ';', '.', 'OK', 'ESC'],
        frames: [[home[0], '/', 1.0], [home[1], '/', 1.0], [home[2], ',', 1.0], [home[1], 'OK', 1.1], [cat0, 'ESC', 1.5], [home[1], null, 1.0]] },
      { frames: [[home[0], '/', 1.0], [home[1], 'OK', 1.0], [L.list('Download Softwares', NETWORKS, 0), '.', 1.0],
        [L.list('Download Softwares', NETWORKS, 1), 'OK', 1.0], [L.password(0), 'HOMEWIFI', 1.8], [L.password(8), 'OK', 0.8],
        [L.busy('Download Softwares', 'Connecting'), null, 1.0], [cat0, null, 1.2]] },
      { frames: [[cat0, '.', 1.0], [cat1, 'OK', 1.0], [L.list('Downloads', [['0.1.1', 'Release']], 0), 'OK', 1.1],
        [L.list('Downloads', ACTIONS, 0), 'OK', 1.1], [askBack, null, 1.2]] },
      { frames: [[askBack, ',', 1.4], [askUpdate, 'OK', 1.1], [updating(0.04), null, 0.9]] },
      // Restarted twice: into the temporary copy, which moves itself in place, then into the new one.
      { frames: [[updating(0.15), null, 0.6], [updating(0.5), null, 0.6], [updating(0.85), null, 0.6], [updating(1), null, 0.4],
        [L.notice('Downloads', 'Restarting'), null, 0.9], [L.off(), null, 0.4], [L.splash(), null, 0.9], [L.off(), null, 0.4],
        [L.splash(), null, 0.9], [home[0], null, 1.8]] },
      { frames: [[home[1], '/', 0.8], [home[2], 'OK', 1.0], [L.list('File Browser', [['downloads'], ['Cardenza'], ['bkp']], 0), 'OK', 1.0],
        [L.list('File Browser', [['203-Launcher.0.1.1.bin']], 0), 'OK', 1.0], [L.list('File Browser', FILE_ACTIONS, 0), 'OK', 1.1], [askBack, null, 1.4]] },
    ];
  });

  SCENES['install-firmware'] = launcherScene((L) => {
    const home = HOME.map((_, i) => L.home(i));
    const regions = [[1.0, false], [1.8, false], [3.4, true]];
    const target = (old) => L.target('Cardenza Synth', '1.7 MB', regions, old ? 1 : 2, old ? 'Cardenza Synth' : 'Free space',
      old ? '1.8 MB / Will be replaced' : '3.4 MB / Available', old ? 'Replace' : 'Install');
    const [free, old] = [target(false), target(true)];
    const replace = (chosen) => L.confirm('Install', 'Replace?', 'Replace', 'Back', chosen);
    const actions = L.list('Downloads', ACTIONS, 0);
    const busy = (fraction) => L.busy('Downloads', 'Install Software', fraction);
    const files = (chosen) => L.list('File Browser', [['downloads'], ['CardenzaSynth-0.1.1.bin'], ['Cardenza']], chosen);
    return [
      replug(L),
      { frames: [[home[0], '/', 0.9], [home[1], 'OK', 1.0], [L.list('Downloads', CATALOG, 0), 'OK', 1.0],
        [L.list('Downloads', [['0.1.0', 'Release']], 0), 'OK', 1.0], [actions, 'OK', 1.0], [free, null, 1.4]] },
      { frames: [[free, '/', 1.5], [old, ',', 1.5], [free, '/', 1.2], [old, null, 1.5]] },
      { frames: [[old, 'OK', 1.0], [replace(1), ',', 1.3], [replace(0), 'OK', 1.0], [busy(0.04), null, 0.8]] },
      { frames: [[busy(0.15), null, 0.6], [busy(0.5), null, 0.6], [busy(0.85), null, 0.6], [busy(1), null, 0.3],
        [L.busy('Downloads', 'Checking download'), null, 0.8], [L.notice('Downloads', 'Restarting'), null, 0.8], [L.off(), null, 0.4],
        [L.splash(), null, 1.0], [home[0], null, 1.6]] },
      { frames: [[home[0], 'OK', 1.1], [L.software([['Cardenza Synth', '0.1.0'], ['GLIDE', '3.8']], 0), 'OK', 1.3],
        [L.splash('Starting Cardenza Synth'), null, 1.2], [L.groovebox(), null, 2.2]] },
      { frames: [[home[4], 'OK', 1.0], [L.usb(false), null, 1.2], [L.usb(true), 'ESC', 1.8], [home[4], ',', 0.7], [home[3], ',', 0.7],
        [home[2], 'OK', 1.0], [files(0), '.', 0.9], [files(1), 'OK', 1.0], [L.list('File Browser', FILE_ACTIONS, 0), 'OK', 1.0], [free, null, 1.5]] },
      { frames: [[L.list('Downloads', CATALOG, CATALOG.length - 1, CATALOG.length - 4), 'OK', 1.2],
        [L.list('Downloads', [['Update all'], ['Cardenza Synth']], 1), 'OK', 1.2], [L.list('Downloads', [['0.1.1', 'Release']], 0), 'OK', 1.0],
        [actions, null, 1.4]] },
    ];
  });

  // ---- Players ----------------------------------------------------------------------------------
  // A lesson's player loops the step it shows, from the top each time a step is shown; it plays only
  // while its lesson is on screen (hidden, or destroyed with the lesson, it draws and sounds nothing).
  function create(container, name, options) {
    options = options || {};
    const thumb = !!options.thumb;
    container.innerHTML = '';
    const svg = makeSvg(container);
    const scene = SCENES[name];
    if (!scene) return { setStep() {}, destroy() {} };
    // A scene without sound (the Launcher's) has no sound button.
    const soundButton = scene.silent && container.closest('.anim-wrap') ? container.closest('.anim-wrap').querySelector('.a-sound') : null;
    if (soundButton) soundButton.style.display = 'none';
    const frame = scene(svg, thumb);
    let from = 0, to = options.step || 0, changed = -10, raf = 0, alive = true;
    const t0 = performance.now();
    let stepStart = t0;
    const tick = (now) => {
      if (!alive) return;
      raf = requestAnimationFrame(tick);
      if (container.offsetParent === null) return;  // hidden: nothing to draw
      const t = (now - t0) / 1000;
      const k = ease((t - changed) / 0.8);
      frame((now - stepStart) / 1000, to, from, to, k);
    };
    raf = requestAnimationFrame(tick);
    return {
      setStep(index) {
        if (index === to) return;
        from = to;
        to = index;
        stepStart = performance.now();
        changed = (stepStart - t0) / 1000;
      },
      destroy() {
        alive = false;
        cancelAnimationFrame(raf);
        container.innerHTML = '';
      },
    };
  }

  const THUMB_STEP = { 'music-box': 1, 'four-boxes': 2, 'boxes-in-a-row': 2, 'update-launcher': 5, 'install-firmware': 2 };
  window.GuideAnim = {
    create: (container, name) => create(container, name, {}),
    thumb: (container, name) => create(container, name, { thumb: true, step: container.classList.contains('hero-art') ? 0 : THUMB_STEP[name] || 0 }),
    setSound(on) {
      soundOn = !!on;
      if (soundOn) ctx();
    },
  };
})();
