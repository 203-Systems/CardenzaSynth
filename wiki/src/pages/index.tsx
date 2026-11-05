// The site opens on the guide: the lessons as cards, part by part. Links of the one-page guide keep
// working: /learn/#/lesson goes to that lesson, /learn/#section to the manual's section.
import React, {useEffect} from 'react';
import Link from '@docusaurus/Link';
import {useHistory} from '@docusaurus/router';
import {useBaseUrlUtils} from '@docusaurus/useBaseUrl';
import sections from '@site/generated/data/sections.json';
import {useLang, useUrls} from '@site/src/components/cardenza/common';
import {GUIDE, lessonPath} from '@site/src/guide/data';
import {AnimThumb, GuideLayout} from '@site/src/guide/pieces';
import type {DeviceStep} from '@site/src/guide/run';
import {useProgress} from '@site/src/guide/store';
import {useSay} from '@site/src/guide/text';

function useOldLinks() {
  const history = useHistory();
  const urls = useUrls();
  const {withBaseUrl} = useBaseUrlUtils();
  useEffect(() => {
    const hash = decodeURIComponent(window.location.hash.slice(1));
    if (hash.startsWith('/') && GUIDE.lessons[hash.slice(1)]) history.replace(withBaseUrl(lessonPath(hash.slice(1))));
    else if (hash && hash in sections) history.replace(urls.section(hash));
  }, []);
}

export default function Home(): React.ReactElement {
  const lang = useLang();
  const say = useSay();
  const urls = useUrls();
  const done = useProgress();
  useOldLinks();
  return (
    <GuideLayout active="" title={say('入门', 'Guide')}>
      <section className="g-home">
        <div className="g-hero">
          <div className="g-hero-text">
            <h1>{lang === 'zh' ? <>十分钟，<br />做出第一首歌。</> : <>Ten minutes<br />to your first song.</>}</h1>
            <p>{say('先用三张图弄懂编曲，再一课一课学机器：每课几步，跟着按就会。',
              'Three pictures explain how music is built; then the device, lesson by lesson, a few steps each.')}</p>
            <Link className="cta" to={lessonPath(GUIDE.order[0])}>{say('从第一课开始', 'Start with lesson 1')} →</Link>
          </div>
          <AnimThumb className="hero-art" name="music-box" />
        </div>
        {GUIDE.parts.map((part) => (
          <section className="cards-part" key={part.id}>
            <h2>{part.title[lang]}</h2>
            <div className="g-cards">
              {part.lessons.map((id, i) => {
                const lesson = GUIDE.lessons[id];
                const lastStep = lesson.steps[lesson.steps.length - 1] as DeviceStep;
                return (
                  <Link key={id} className={'g-card' + (done[id] ? ' done' : '')} to={lessonPath(id)}>
                    {lesson.anim
                      ? <AnimThumb className="c-art" name={lesson.anim} />
                      : <div className="c-art"><img src={urls.shot(lastStep.shot)} alt="" loading="lazy" /></div>}
                    <div className="c-text">
                      <span className="c-num">{i + 1}</span>
                      <b>{lesson.title[lang]}</b>
                      <small>{lesson.lead[lang]}</small>
                    </div>
                    <span className="ok" aria-hidden="true" />
                  </Link>
                );
              })}
            </div>
          </section>
        ))}
        <section className="device-card">
          <h2>{say('有一台鸣片？', 'Got a Cardenza?')}</h2>
          <p>{say('用 USB 线接上电脑，浏览器问能不能使用 MIDI 设备时点「允许」：机器会跟着课程走，该按的键会闪，做对了会告诉你「学会了」。没有机器也没关系，看图就能跟着学。',
            'Plug it in with a USB cable and allow MIDI when the browser asks: the device follows the lessons, the keys to press blink, and the page tells you when you have got it. No device? The pictures show every step.')}</p>
          <p className="dc-small">{say('教程用一个练习工程，你自己的工程不会被改动；关掉这一页，机器就回到原来的样子。',
            'The lessons use a practice project: your own work is left alone, and it is back as soon as you close this page.')}</p>
        </section>
        <p className="g-ref">
          <Link to="/manual/start/">{say('要查某个键、某一页、某个设置？去说明书 →', 'Looking for one key, page or setting? The manual →')}</Link>
        </p>
      </section>
    </GuideLayout>
  );
}
