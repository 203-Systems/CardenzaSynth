// The site opens on the guide: the lessons as cards, part by part. Links of the one-page guide keep
// working: /learn/#/lesson goes to that lesson, /learn/#section to the manual's section.
import React, {useEffect} from 'react';
import Link from '@docusaurus/Link';
import {useHistory} from '@docusaurus/router';
import {useBaseUrlUtils} from '@docusaurus/useBaseUrl';
import sections from '@site/generated/data/sections.json';
import {useLang, useUrls} from '@site/src/components/cardenza/common';
import {GUIDE, lessonPath} from '@site/src/guide/data';
import {LauncherThumb} from '@site/src/guide/launcher';
import {AnimThumb, GuideLayout} from '@site/src/guide/pieces';
import {FirmwareTutorials} from '@site/src/guide/tutorials';
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
            <p>{say('先通过三组动图理解编曲原理，再分课掌握硬件操作，每课只需几步。',
              'Start with three quick visual lessons on how music is built, then learn the hardware step by step.')}</p>
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
                    {lesson.anim === 'launcher'
                      ? <LauncherThumb className="c-art" name={lesson.thumb || 'home-0'} />
                      : lesson.anim
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
        <FirmwareTutorials />
        <section className="device-card">
          <h2>{say('手边有鸣片？', 'Got a Cardenza?')}</h2>
          <p>{say('通过 USB 连接电脑并在浏览器提示时允许使用 MIDI 设备，鸣片就会随课程自动联动、闪烁引导键，并在操作正确后确认完成。没有设备也可以直接看图学习。',
            'Connect over USB and allow MIDI access when prompted. Your Cardenza follows each lesson, blinks the keys to press, and confirms when you complete each step. No hardware? You can still follow along with the diagrams.')}</p>
          <p className="dc-small">{say('交互教程运行在临时练习工程中，不会改动你的个人工程；关闭本页后设备会自动恢复原状。',
            'Lessons run in a temporary practice project, leaving your own work untouched and restoring it automatically when you close this page.')}</p>
          <p className="dc-small">
            {say('设备固件版本过旧、无法联动教程？请先查看', 'Firmware too old for the interactive lessons? See')}{' '}
            <Link to={lessonPath('update-firmware')}>{say('更新官方固件', 'Updating the official firmware')}</Link>
            {say('，启动器更新与社区固件安装也在「更新与安装」分区中。', '. Launcher updates and community firmware are covered under Update and install.')}
          </p>
        </section>
        <p className="g-ref">
          <Link to="/manual/start/">{say('需要查阅具体按键、页面或参数？查看说明书 →', 'Looking for a specific key, page, or setting? Open the manual →')}</Link>
        </p>
      </section>
    </GuideLayout>
  );
}
