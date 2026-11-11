// The guide's frame and parts: the layout with the rail of lessons, the toast, the line that says
// whether a Cardenza follows, the animations and a device step's picture.
import React, {useEffect, useRef, useState} from 'react';
import Layout from '@theme/Layout';
import Link from '@docusaurus/Link';
import {useHistory, useLocation} from '@docusaurus/router';
import {useBaseUrlUtils} from '@docusaurus/useBaseUrl';
import {useLang} from '@site/src/components/cardenza/common';
import {Legend, useScreen} from '@site/src/components/cardenza/figures';
import {usePcbKeys} from '@site/src/components/cardenza/pcb';
import {GUIDE, GUIDE_LEDS, lessonPath, loadAnim, type AnimPlayer} from './data';
import {REASONS, STATUS, device, useDevice} from './device';
import {layoutVariant, silentPads, type DeviceStep} from './run';
import {clearDone, requestRestart, toast, useProgress, useToast} from './store';
import {useSay} from './text';
import './guide.css';

/** Learning a lesson again clears its tick and those of every lesson after it (they build on it). */
export function learnAgain(id: string) {
  const from = GUIDE.order.indexOf(id);
  const cleared = clearDone(from >= 0 ? GUIDE.order.slice(from) : [id]);
  if (cleared > 1) {
    toast([`这一课和之后的课一起重新开始：清掉了 ${cleared} 课的完成记录。`,
      `This lesson and the ones after it start over: ${cleared} ticks cleared.`]);
  }
  requestRestart(id);
}

function Rail({active, open}: {active: string; open: boolean}) {
  const lang = useLang();
  const say = useSay();
  const done = useProgress();
  const history = useHistory();
  const {withBaseUrl} = useBaseUrlUtils();
  const count = GUIDE.order.filter((id) => done[id]).length;
  const allAgain = () => {
    if (!count || !window.confirm(say(`清除全部 ${count} 课的完成记录，从第一课重新学习？`,
      `Clear the ticks of all ${count} finished lessons and start again from lesson 1?`))) return;
    clearDone(GUIDE.order);
    const first = GUIDE.order[0];
    if (active === first) requestRestart(first);
    else history.push(withBaseUrl(lessonPath(first)));
  };
  return (
    <nav className={'g-rail' + (open ? ' open' : '')} aria-label={say('课程', 'Lessons')}>
      <Link className={'g-home-link' + (active === '' ? ' active' : '')} to="/">{say('全部课程', 'All lessons')}</Link>
      {GUIDE.parts.map((part) => (
        <div className="g-part" key={part.id}>
          <h3>{part.title[lang]}</h3>
          <ol>
            {part.lessons.map((id, i) => (
              <li key={id}>
                <Link to={lessonPath(id)} className={(active === id ? 'active' : '') + (done[id] ? ' done' : '')}>
                  <span className="n">{i + 1}</span>
                  <span className="t">{GUIDE.lessons[id].title[lang]}</span>
                  <span className="ok" aria-hidden="true" />
                </Link>
              </li>
            ))}
          </ol>
        </div>
      ))}
      <div className="g-progress">
        <span className="bar"><i style={{width: `${(100 * count) / Math.max(1, GUIDE.order.length)}%`}} /></span>
        <span>{say(`完成 ${count} / ${GUIDE.order.length} 课`, `${count} of ${GUIDE.order.length} lessons done`)}</span>
        {count > 0 && (
          <button type="button" className="redo-all" onClick={allAgain}>↺ {say('全部重新学习', 'Start all over')}</button>
        )}
      </div>
    </nav>
  );
}

function Toast() {
  const current = useToast();
  const say = useSay();
  return current ? <div className="g-toast" role="status">{say(current.words[0], current.words[1])}</div> : null;
}

/** A guide page: the wiki's navbar and footer, the rail of lessons beside the page. The guide pages
 *  ask for MIDI as they open (a Cardenza plugged in follows the lessons). */
export function GuideLayout({active, title, children}: {active: string; title: string; children: React.ReactNode}) {
  const lang = useLang();
  const say = useSay();
  const [open, setOpen] = useState(false);
  const location = useLocation();
  useEffect(() => {
    device?.start();
  }, []);
  useEffect(() => setOpen(false), [location.pathname]);
  return (
    <Layout title={title} description={say('鸣片入门：一课几步，接上机器跟着按。', 'The Cardenza guide: a few steps a lesson, with your device following.')}>
      <div className="cz-guide" data-lang={lang}>
        <div className="g-layout">
          <Rail active={active} open={open} />
          <main className="g-main">
            <button type="button" className="g-menu" aria-expanded={open} onClick={() => setOpen(!open)}>
              ☰ {say('全部课程', 'All lessons')}
            </button>
            {children}
          </main>
        </div>
        <Toast />
      </div>
    </Layout>
  );
}

/** Whether a Cardenza follows the lesson; after a refusal it is the button to try again. song: the
 *  lesson needs newer firmware than the device's (it goes by the pictures). */
export function DeviceLine({song = false}: {song?: boolean}) {
  const d = useDevice();
  const say = useSay();
  const status = d?.status ?? 'waiting';
  const connected = status === 'on';
  const guide = !!d?.guide;
  const reason = d?.reason ?? null;
  const words = reason ? REASONS[reason] || REASONS.other : connected && !guide ? STATUS.hello
    : song && guide ? STATUS.song : STATUS[status];
  const cls = ['l-device',
    connected && guide ? 'on' : '',
    status === 'hello' || (connected && !guide && !reason) ? 'wait' : '',
    reason || status === 'old' || status === 'denied' ? 'warn' : '',
    reason ? 'retry' : ''].filter(Boolean).join(' ');
  return <p className={cls} role="status" onClick={() => d?.retry()}>{say(words[0], words[1])}</p>;
}

/** A concept lesson's animation (./guide-anim.js), showing one step. */
export function AnimStage({name, step}: {name: string; step: number}) {
  const box = useRef<HTMLDivElement>(null);
  const player = useRef<AnimPlayer | null>(null);
  const shown = useRef(step);
  shown.current = step;
  useEffect(() => {
    let alive = true;
    loadAnim().then((anim) => {
      if (!alive || !box.current) return;
      player.current = anim.create(box.current, name);
      player.current.setStep(shown.current);
    });
    return () => {
      alive = false;
      player.current?.destroy();
      player.current = null;
    };
  }, [name]);
  useEffect(() => {
    player.current?.setStep(step);
  }, [step]);
  return <div className="anim-stage" ref={box} />;
}

/** A small looping animation (the home page's hero and cards), without sound. */
export function AnimThumb({name, className}: {name: string; className: string}) {
  const box = useRef<HTMLDivElement>(null);
  useEffect(() => {
    let alive = true;
    let player: AnimPlayer | null = null;
    loadAnim().then((anim) => {
      if (alive && box.current) player = anim.thumb(box.current, name);
    });
    return () => {
      alive = false;
      player?.destroy();
    };
  }, [name]);
  return <div className={className} ref={box} />;
}

/** A device step as the device looks: its screen in its display (enlarged, marks framed), the board's
 *  keys under it (the keys to press framed and the others faded, or lit as the device lit them), the keys
 *  of the demo going down. */
export function StepPicture({step, pressed}: {step: DeviceStep; pressed?: Set<number>}) {
  const lang = useLang();
  // The pads the device's keyboard leaves silent (as it lights them), or a new project's PIANO.
  const d = useDevice();
  const live = d?.connected() && d.guide ? d.state : null;
  const silent = silentPads(live);
  // On the user's SCALE or CHROMA keyboard every pad plays: the picture lights them all, and the PIANO
  // keys' frames (black, white) are not shown.
  const variant = layoutVariant(step, live);
  const screen = useScreen({shot: step.shot, marks: step.marks.map((at) => ({at})), light: true, alt: step.say.en});
  const shotLit = step.lights ? GUIDE_LEDS[step.shot] ?? null : null;
  const lit = shotLit && variant
    ? shotLit.map((row) => row.map((level, column) => (column >= 1 && column <= 8 ? Math.max(level, 6) : level)))
    : shotLit;
  const keymarks = variant ? [] : step.keymarks;
  const plain = !lit && !keymarks.length;
  const keys = usePcbKeys(plain
    ? {levels: null, focusOnly: true, pressed, label: step.say.en, screen: screen.inner, silent,
      marks: step.keySpec.split(/\s+/).filter(Boolean).map((token) => ({keys: token}))}
    : {levels: lit, focusOnly: !lit, badgeAll: true, pressed, label: step.say.en, screen: screen.inner,
      marks: keymarks.length ? keymarks.map(([spec, label]) => ({keys: spec, label: label[lang]})) : [{keys: step.keySpec}]});
  return (
    <div className="st-page">
      <div className="st-keys">
        <div className="cz-device">{keys.svg}</div>
        {keymarks.length > 0 && <Legend items={keys.items} rows={Math.ceil(keymarks.length / 2)} />}
      </div>
    </div>
  );
}
