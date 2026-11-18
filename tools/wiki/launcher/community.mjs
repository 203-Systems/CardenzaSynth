#!/usr/bin/env node
// The 203 community's pages for the Learn site's update lessons, as the live site shows them to someone not
// signed in: wiki/static/img/community/<zh|en>/<name>.png (a part of the page, 2x) and
// wiki/src/guide/community-shots.json (each picture's size and the boxes to highlight on it, the button or
// the field the lesson points at). Run it again when the community's pages change, and check the lessons'
// words against the new pictures (docs/learn/guide.json, part "update").
//
//   PLAYWRIGHT_CORE=<path to playwright-core> CHROMIUM=<chromium or headless shell> node tools/wiki/launcher/community.mjs
//
// The site is https://cardenza.203.io/community (COMMUNITY_URL to change it).
import {createRequire} from 'node:module';
import {mkdirSync, writeFileSync} from 'node:fs';
import {dirname, join} from 'node:path';
import {fileURLToPath} from 'node:url';

const require = createRequire(import.meta.url);
const {chromium} = require(process.env.PLAYWRIGHT_CORE || 'playwright-core');
const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..', '..', '..');
const OUT = join(ROOT, 'wiki', 'static', 'img', 'community');
const MARKS = join(ROOT, 'wiki', 'src', 'guide', 'community-shots.json');
const SITE = (process.env.COMMUNITY_URL || 'https://cardenza.203.io/community').replace(/\/$/, '');
const SCALE = 2;

// [name, page, what to do first, the part of the page (a selector), what to highlight (selectors), at most this
// tall, from this element's left edge]. Only what is visible counts (the phone's bar has the same buttons, hidden).
const HERO = ['.fwp-hero', '.fwp-mid'];
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
];

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

const browser = await chromium.launch(process.env.CHROMIUM ? {executablePath: process.env.CHROMIUM} : {});
const marks = {};
for (const lang of ['zh', 'en']) {
  mkdirSync(join(OUT, lang), {recursive: true});
  const context = await browser.newContext({viewport: {width: 1280, height: 860}, deviceScaleFactor: SCALE,
    locale: lang === 'zh' ? 'zh-CN' : 'en-US', colorScheme: 'light'});
  for (const [name, path, actions, [part, from], highlights, height] of SHOTS) {
    const page = await context.newPage();
    await page.goto(SITE + path, {waitUntil: 'networkidle', timeout: 60000});
    await page.waitForTimeout(800);
    // The sign-in offer a first visit gets.
    await page.evaluate(() => document.querySelectorAll('dialog.signin-auto').forEach((d) => d.close()));
    for (const [kind, target] of actions) {
      if (kind === 'click') await (await visible(page, target)).locator.click();
      else await page.getByText(target).first().click();
      await page.waitForTimeout(900);
    }
    await page.mouse.move(0, 0);
    const {box} = await visible(page, part);
    const left = from ? (await visible(page, from)).box.x : box.x;
    const clip = {x: Math.floor(left), y: Math.floor(box.y), width: Math.ceil(box.x + box.width - left),
      height: Math.ceil(height ? Math.min(height, box.height) : box.height)};
    const boxes = [];
    for (const selector of highlights) {
      const {box: b} = await visible(page, selector);
      boxes.push([b.x - clip.x, b.y - clip.y, b.width, b.height].map((v) => Math.round(v * SCALE)));
    }
    await page.screenshot({path: join(OUT, lang, `${name}.png`), clip});
    marks[name] = marks[name] || {};
    marks[name][lang] = {w: clip.width * SCALE, h: clip.height * SCALE, marks: boxes};
    await page.close();
    console.log(`community shots: ${lang}/${name}.png`);
  }
  await context.close();
}
await browser.close();
writeFileSync(MARKS, JSON.stringify({site: SITE, taken: new Date().toISOString().slice(0, 10), shots: marks}, null, 1) + '\n');
