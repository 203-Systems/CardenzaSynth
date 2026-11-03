// The guide's concept animations (tools/learn/guide.js plays them): a music box that becomes a
// sequencer, four boxes that make a part, parts in a row that make a song. Drawn in the product's
// look: black, the screen's colours and track colours, 4x4 Bayer dither, a green playhead.
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
  const TRACKS = [
    { zh: '鼓', en: 'DRUMS', hits: (s, fill) => (fill ? FILL : DRUMS)[s] || (s % 2 === 0 ? 'hat' : null) },
    { zh: '钢琴', en: 'PIANO', hits: (s) => MELODY.find((m) => m[0] === s) },
    { zh: '和弦', en: 'CHORDS', hits: (s) => CHORDS[s] },
    { zh: '贝斯', en: 'BASS', hits: (s) => BASS[s] },
  ];
  function sound(track, s, fill) {
    const hit = TRACKS[track].hits(s, fill);
    if (!hit) return;
    if (track === 0) { play(hit); if (hit !== 'hat' && s % 2 === 0) play('hat'); }
    if (track === 1) play('piano', SCALE[hit[1]]);
    if (track === 2) play('chord', hit + 12);
    if (track === 3) play('bass', hit);
  }

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
    const combW = w * 0.34;
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
    const baseX = () => x + combW * 0.3;
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
          if (next < 0.3) bend = -(1 - next / 0.3) * 1.6 * sc;
          else if (since < 3) bend = 1.4 * sc * Math.exp(-since / 0.7) * Math.cos(since * Math.PI * 2.5);
          const cy = rowY(row), bx = baseX(row), hb = laneH * 0.32, ht = laneH * 0.17;
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
    return (t, step, from, to, k) => {
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
    label(lanes, 375, 312, '四轨一起 = 一段', 'Four tracks together = one part', { fill: C.dim, 'font-size': 13, 'text-anchor': 'middle' });
    el(lanes, 'rect', { x: 32, y: 54, width: 576, height: 4 * 58 + 4, rx: 6, fill: 'none', stroke: C.fn, 'stroke-dasharray': '5 3.5' });
    let last = -1;
    return (t, step, from, to, k) => {
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
  };

  SCENES['boxes-in-a-row'] = (svg, thumb) => {
    // A part drawn small: four lanes of hits in the track colours.
    const PARTS = [false, false, true, false];  // A A B(fill) A
    const part = (parent, x, y, w, h, fill) => {
      const g = el(parent, 'g', {});
      el(g, 'rect', { x, y, width: w, height: h, rx: 6, fill: '#0e0e0d', stroke: C.line2 });
      const lh = (h - 16) / 4;
      for (let i = 0; i < 4; i++) {
        for (let s = 0; s < 16; s++) {
          if (!TRACKS[i].hits(s, fill)) continue;
          el(g, 'rect', { x: x + 8 + s * (w - 16) / 16, y: y + 8 + i * lh + 1, width: (w - 16) / 16 - 1.5, height: lh - 3, fill: svg.dither(C.t[i], 12) });
        }
      }
      const frame = el(g, 'rect', { x: x - 3, y: y - 3, width: w + 6, height: h + 6, rx: 8, fill: 'none', stroke: C.play, 'stroke-width': 2, opacity: 0 });
      return { g, frame };
    };
    const single = el(svg, 'g', {});
    const lone = part(single, 220, 110, 200, 120, false);
    const clock = label(single, 320, 270, '≈ 2.4 秒', '≈ 2.4 seconds', { fill: C.dim, 'font-size': 14, 'text-anchor': 'middle' });
    void clock;
    const lone_head = el(single, 'rect', { y: 104, width: 2, height: 132, fill: C.play });
    const row = el(svg, 'g', {});
    const parts = PARTS.map((fill, i) => {
      const p = part(row, 34 + i * 148, 110, 132, 100, fill);
      el(row, 'text', { x: 100 + i * 148, y: 236, fill: fill ? C.fn : C.dim, 'font-size': 13, 'text-anchor': 'middle', 'font-weight': 700 }, fill ? 'B' : 'A');
      return p;
    });
    const tape = el(row, 'rect', { y: 100, width: 2, height: 120, fill: C.play });
    label(row, 320, 290, '时间 →', 'time →', { fill: C.faint, 'font-size': 13, 'text-anchor': 'middle' });
    // The SONG page: a column per part, a row per track, the pattern in each cell.
    const song = el(svg, 'g', {});
    const cells = [];
    PARTS.forEach((fill, c) => {
      el(song, 'text', { x: 150 + c * 112, y: 62, fill: C.dim, 'font-size': 12 }, String(c + 1).padStart(2, '0'));
      for (let r = 0; r < 4; r++) {
        const x = 150 + c * 112, y = 74 + r * 54;
        cells.push(el(song, 'rect', { x: x - 4, y: y - 4, width: 104, height: 46, rx: 3, fill: 'none', stroke: C.play, 'stroke-width': 2, opacity: 0, 'data-c': c }));
        el(song, 'text', { x, y: y + 22, fill: C.ink, 'font-size': 20, 'font-weight': 700 }, r === 0 && fill ? 'P02' : 'P01');
        el(song, 'rect', { x, y: y + 32, width: 92, height: 3, fill: C.t[r] });
      }
    });
    for (let r = 0; r < 4; r++) {
      el(song, 'rect', { x: 66, y: 74 + r * 54, width: 54, height: 30, rx: 3, fill: C.t[r] });
      el(song, 'text', { x: 93, y: 94 + r * 54, fill: '#000', 'font-size': 14, 'font-weight': 700, 'text-anchor': 'middle' }, 'T' + (r + 1));
    }
    el(song, 'rect', { x: 520, y: 300, width: 64, height: 22, rx: 2, fill: C.fn });
    el(song, 'text', { x: 552, y: 316, fill: '#000', 'font-size': 13, 'font-weight': 700, 'text-anchor': 'middle' }, 'SONG');
    label(song, 66, 318, '3 TAPE · SONG 页', '3 TAPE · the SONG page', { fill: C.faint, 'font-size': 12 });
    let last = -1;
    return (t, step, from, to, k) => {
      const show = (phase) => (phase === to ? k : phase === from ? 1 - k : 0);
      single.setAttribute('opacity', show(0));
      row.setAttribute('opacity', show(1));
      song.setAttribute('opacity', show(2));
      const s = Math.floor(t / STEP) % 16;
      const total = Math.floor(t / STEP) % 64;
      const current = total >> 4;
      lone_head.setAttribute('x', 228 + (t / STEP % 16) * 184 / 16);
      tape.setAttribute('x', 42 + current * 148 + ((t / STEP) % 16) * 116 / 16);
      parts.forEach((p, i) => p.frame.setAttribute('opacity', i === current ? 1 : 0));
      lone.frame.setAttribute('opacity', 0);
      cells.forEach((cell) => cell.setAttribute('opacity', Number(cell.dataset.c) === current ? 1 : 0));
      if (s !== last) {
        last = s;
        if (thumb) return;
        const fill = to === 0 ? false : PARTS[current];
        for (let i = 0; i < 4; i++) sound(i, s, fill);
      }
    };
  };

  // ---- Players ----------------------------------------------------------------------------------
  function create(container, name, options) {
    const thumb = !!(options && options.thumb);
    container.innerHTML = '';
    const svg = makeSvg(container);
    const scene = SCENES[name];
    if (!scene) return { setStep() {}, destroy() {} };
    const frame = scene(svg, thumb);
    let from = 0, to = (options && options.step) || 0, changed = -10, raf = 0, alive = true;
    const t0 = performance.now();
    const tick = (now) => {
      if (!alive) return;
      raf = requestAnimationFrame(tick);
      if (container.offsetParent === null) return;  // hidden: nothing to draw
      const t = (now - t0) / 1000;
      const k = ease((t - changed) / 0.8);
      frame(t, to, from, to, k);
    };
    raf = requestAnimationFrame(tick);
    return {
      setStep(index) {
        if (index === to) return;
        from = to;
        to = index;
        changed = (performance.now() - t0) / 1000;
      },
      destroy() {
        alive = false;
        cancelAnimationFrame(raf);
        container.innerHTML = '';
      },
    };
  }

  const THUMB_STEP = { 'music-box': 1, 'four-boxes': 2, 'boxes-in-a-row': 2 };
  window.GuideAnim = {
    create: (container, name) => create(container, name, {}),
    thumb: (container, name) => create(container, name, { thumb: true, step: container.classList.contains('hero-art') ? 0 : THUMB_STEP[name] || 0 }),
    setSound(on) {
      soundOn = !!on;
      if (soundOn) ctx();
    },
  };
})();
