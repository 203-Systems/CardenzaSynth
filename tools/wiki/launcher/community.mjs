#!/usr/bin/env node
// The 203 community's pages for the Learn site's update lessons, as the live site shows them to someone not
// signed in: wiki/static/img/community/<zh|en>/<name>.png (a part of the page, 2x) and
// wiki/src/guide/community-shots.json (each picture's size and the boxes to highlight on it, the button or
// the field the lesson points at). Run it again when the community's pages change, and check the lessons'
// words against the new pictures (docs/learn/guide.json, part "update").
//
//   PLAYWRIGHT_CORE=<path to playwright-core> CHROMIUM=<chromium or headless shell> node tools/wiki/launcher/community.mjs
//
// The site is https://stage.203.io (COMMUNITY_URL to change it). ONLY=rom-prompt,rom-done takes those
// pictures alone and keeps the others' marks. The 刷入最新启动器 pictures use the community's demo chip (?rom=demo…: a
// Cardenza in download mode on a fake port, a fake esptool; nothing is downloaded or written).
import {createRequire} from 'node:module';
import {mkdirSync, readFileSync, writeFileSync} from 'node:fs';
import {dirname, join} from 'node:path';
import {fileURLToPath} from 'node:url';

const require = createRequire(import.meta.url);
const {chromium} = require(process.env.PLAYWRIGHT_CORE || 'playwright-core');
const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..', '..', '..');
const OUT = join(ROOT, 'wiki', 'static', 'img', 'community');
const MARKS = join(ROOT, 'wiki', 'src', 'guide', 'community-shots.json');
const SITE = (process.env.COMMUNITY_URL || 'https://stage.203.io').replace(/\/$/, '');
const SCALE = 2;

// [name, page, what to do first, the part of the page (a selector, or several: the box around them all), what to
// highlight (selectors), at most this tall, from this element's left edge, {width: the window's, above: px of the page
// over the part}]. Only what is visible counts (the phone's bar has the same buttons, hidden). What to do: ['click',
// selector], ['click-text', text], ['wait', selector] (until it shows), ['pause', ms], ['bottom'] (scrolled to the end).
const HERO = ['.fwp-hero', '.fwp-mid'];
// The prompt under the header at the right, with the header's right end above it.
const PROMPT = [['.lfl-pop', '#nav .acct']];
const SHOTS = [
  ['official-usb', '/V6QK28', [], HERO, ['.fwp-hero .fwa-go']],
  ['official-code', '/V6QK28', [], HERO, ['.fwp-hero .fwp-coderow']],
  ['official-usb-dialog', '/V6QK28', [['click', '.fwp-hero .fwa-go']], ['dialog[open]:not(.signin-auto)'], ['dialog[open] button.cta']],
  ['official-bin', '/V6QK28', [['click', '.fwp-hero .fwa-caret']], HERO, ['.item-menu [role=menuitem]:nth-child(2)']],
  ['official-wifi', '/V6QK28', [['click', '.fwp-hero .fwa-caret']], HERO, ['.item-menu [role=menuitem]:nth-child(1)']],
  ['firmware-list', '/firmware', [], ['.cards-firmware'], ['.fw-card:has(a.fw-name[href$="B6S4QV"])'], 400],
  ['glide-usb', '/B6S4QV', [], HERO, ['.fwp-hero .fwa-go']],
  ['glide-code', '/B6S4QV', [], HERO, ['.fwp-hero .fwp-coderow']],
  ['glide-usb-dialog', '/B6S4QV', [['click', '.fwp-hero .fwa-go']], ['dialog[open]:not(.signin-auto)'], ['dialog[open] button.cta']],
  ['glide-bin', '/B6S4QV', [['click', '.fwp-hero .fwa-caret']], HERO, ['.item-menu [role=menuitem]:nth-child(2)']],
  ['recover', '/recover', [['click-text', 'Cardenza']], ['.rcv-body'], ['.rcv-step.on .rcv-way:last-child'], 640],
  // 刷入最新启动器 (the lesson flash-launcher): the footer's way the first time, the prompt, the dialog writing and done, and the
  // prompt for a Launcher too old.
  ['rom-entry', '/?rom=demo-new', [], [['.f-rom', '.f-row']], ['.f-rom button'], 0, {width: 860, above: 150}],
  ['rom-prompt', '/?rom=demo', [['wait', '.lfl-pop'], ['pause', 700]], PROMPT, ['.lfl-pop .cta']],
  ['rom-writing', '/?rom=demo', [['wait', '.lfl-pop'], ['click', '.lfl-pop .cta'], ['wait', 'dialog[open] .lfl .flash-steps'], ['pause', 3200]],
    ['dialog[open]'], ['dialog[open] .rcv-meter']],
  ['rom-done', '/?rom=demo', [['wait', '.lfl-pop'], ['click', '.lfl-pop .cta'], ['wait', 'dialog[open] .lfl-done'], ['pause', 600]],
    ['dialog[open]'], ['dialog[open] .lfl-done .rcv-ok']],
  ['old-prompt', '/?rom=demo-launcher', [['wait', '.lfl-pop'], ['pause', 700]], PROMPT, ['.lfl-pop .cta']],
];
const ONLY = (process.env.ONLY || '').split(',').filter(Boolean);

const visible = async (page, selector) => {
  for (const one of selector.split(/,\s*/)) {
    const all = page.locator(one);
    for (let i = 0; i < (await all.count()); i++) {
      const box = await all.nth(i).boundingBox();
      if (box && box.width > 0 && box.height > 0) return {box, locator: all.nth(i)};
    }
  }
  throw new Error(`community shots: nothing visible for ${selector}`);
};

/** The box around every visible element of `selectors`. */
const around = async (page, selectors) => {
  const boxes = [];
  for (const one of selectors) boxes.push((await visible(page, one)).box);
  const x = Math.min(...boxes.map((b) => b.x)), y = Math.min(...boxes.map((b) => b.y));
  return {x, y, width: Math.max(...boxes.map((b) => b.x + b.width)) - x, height: Math.max(...boxes.map((b) => b.y + b.height)) - y};
};

const browser = await chromium.launch(process.env.CHROMIUM ? {executablePath: process.env.CHROMIUM} : {});
// With ONLY, the other pictures keep their marks.
let kept = {};
try { kept = JSON.parse(readFileSync(MARKS, 'utf8')).shots || {}; } catch { kept = {}; }
const marks = ONLY.length ? kept : {};
for (const lang of ['zh', 'en']) {
  mkdirSync(join(OUT, lang), {recursive: true});
  const context = await browser.newContext({viewport: {width: 1280, height: 860}, deviceScaleFactor: SCALE,
    locale: lang === 'zh' ? 'zh-CN' : 'en-US', colorScheme: 'light'});
  for (const [name, path, actions, [part, from], highlights, height, {width = 0, above = 0} = {}] of SHOTS) {
    if (ONLY.length && !ONLY.includes(name)) continue;
    const page = await context.newPage();
    if (width) await page.setViewportSize({width, height: 860});
    // The sign-in offer a first visit gets stays away (it was just closed, as a visitor would).
    await page.addInitScript(() => { try { localStorage.setItem('signin-prompt:v1', String(Date.now())); } catch { /* none */ } });
    await page.goto(SITE + path, {waitUntil: 'networkidle', timeout: 60000});
    await page.waitForTimeout(800);
    await page.evaluate(() => document.querySelectorAll('dialog.signin-auto').forEach((d) => d.close()));
    for (const [kind, target] of actions) {
      if (kind === 'click') await (await visible(page, target)).locator.click();
      else if (kind === 'wait') { await page.waitForSelector(target, {state: 'visible', timeout: 30000}); continue; }
      else if (kind === 'pause') { await page.waitForTimeout(target); continue; }
      else if (kind === 'bottom') { await page.evaluate(() => window.scrollTo(0, document.body.scrollHeight)); }
      else await page.getByText(target).first().click();
      await page.waitForTimeout(900);
    }
    await page.mouse.move(0, 0);
    const found = Array.isArray(part) ? await around(page, part) : (await visible(page, part)).box;
    const box = above ? {...found, y: Math.max(0, found.y - above), height: found.height + Math.min(above, found.y)} : found;
    const left = from ? (await visible(page, from)).box.x : box.x;
    const clip = {x: Math.floor(left), y: Math.floor(box.y), width: Math.ceil(box.x + box.width - left),
      height: Math.ceil(height ? Math.min(height, box.height) : box.height)};
    const boxes = [];
    for (const selector of highlights) {
      const {box: b} = await visible(page, selector);
      boxes.push([b.x - clip.x, b.y - clip.y, b.width, b.height].map((v) => Math.round(v * SCALE)));
    }
    // A part below the window (the footer): the whole page is drawn and the part cut from it (boxes are measured at the
    // top of the page, so they are the page's own).
    const below = clip.y + clip.height > page.viewportSize().height;
    await page.screenshot({path: join(OUT, lang, `${name}.png`), clip, fullPage: below});
    marks[name] = marks[name] || {};
    marks[name][lang] = {w: clip.width * SCALE, h: clip.height * SCALE, marks: boxes};
    await page.close();
    console.log(`community shots: ${lang}/${name}.png`);
  }
  await context.close();
}
await browser.close();
writeFileSync(MARKS, JSON.stringify({site: SITE, taken: new Date().toISOString().slice(0, 10), shots: marks}, null, 1) + '\n');
