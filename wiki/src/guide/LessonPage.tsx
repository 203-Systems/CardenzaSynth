// A guide lesson (/guide/<lesson>/, plugins/guide-routes.js). Concept lessons play an animation step
// by step; device lessons show the device step by step, and a Cardenza on USB follows them (run.ts).
import React, {useEffect, useRef, useState, useSyncExternalStore} from 'react';
import Link from '@docusaurus/Link';
import {useHistory} from '@docusaurus/router';
import {useBaseUrlUtils} from '@docusaurus/useBaseUrl';
import {useLang, useUrls} from '@site/src/components/cardenza/common';
import {GUIDE, lessonPath, loadAnim, partOf} from './data';
import {useDevice} from './device';
import {AnimStage, DeviceLine, GuideLayout, StepPicture, learnAgain} from './pieces';
import {LessonRun, layoutVariant, type DeviceStep, type Lesson} from './run';
import {markDone, setSound, toast, useRestart, useSound} from './store';
import {GuideText, useSay} from './text';

const noSubscribe = () => () => {};

/** Where the lesson's last Next goes: the next lesson, or back to all lessons. */
function useNextUrl(lesson: Lesson) {
  const {withBaseUrl} = useBaseUrlUtils();
  return withBaseUrl(lesson.next ? lessonPath(lesson.next) : '/');
}

function Header({lesson, device, song = false}: {lesson: Lesson; device: boolean; song?: boolean}) {
  const lang = useLang();
  return (
    <header className="l-head">
      <div className="l-kicker">{partOf(lesson).title[lang]} · {lesson.number} / {lesson.of}</div>
      <h1>{lesson.title[lang]}</h1>
      <p className="l-lead">{lesson.lead[lang]}</p>
      {device && <DeviceLine song={song} />}
    </header>
  );
}

function Footer({lesson, last, complete}: {lesson: Lesson; last: boolean; complete: boolean}) {
  const say = useSay();
  const urls = useUrls();
  return (
    <>
      {complete && (
        <div className="l-done"><b>{say('完成！', 'Done!')}</b><span>{say('这一课学会了。', 'You have got this one.')}</span></div>
      )}
      <nav className="l-pager">
        {last && <button type="button" className="redo" onClick={() => learnAgain(lesson.id)}>↺ {say('重新学习', 'Learn again')}</button>}
      </nav>
      {lesson.more && (
        <p className="l-more"><Link to={urls.sectionPath(lesson.more)}>{say('字典里的详细说明', 'In the reference')} →</Link></p>
      )}
    </>
  );
}

function nextLabel(say: (zh: string, en: string) => string, lesson: Lesson, last: boolean) {
  if (!last) return say('下一步 →', 'Next →');
  return lesson.next ? say('学习下一节 →', 'Next lesson →') : say('回到目录 →', 'All lessons →');
}

function DeviceLesson({lesson}: {lesson: Lesson}) {
  const say = useSay();
  const history = useHistory();
  const nextUrl = useNextUrl(lesson);
  const goNext = useRef(() => {});
  goNext.current = () => history.push(nextUrl);
  const [run, setRun] = useState<LessonRun | null>(null);
  useEffect(() => {
    const lessonRun = new LessonRun(lesson, () => goNext.current());
    setRun(lessonRun);
    lessonRun.start();
    return () => lessonRun.dispose();
  }, [lesson]);
  useSyncExternalStore(run ? run.subscribe : noSubscribe, () => (run ? run.version : -1), () => -1);
  const d = useDevice();
  useRestart(lesson.id, () => run?.start());

  const steps = lesson.steps as DeviceStep[];
  const index = run?.index ?? 0;
  const step = steps[index];
  const last = index === steps.length - 1;
  const learned = !!run?.learned[index];
  const idle = !!run?.follows() && !learned;
  const press = last ? (lesson.next ? say('学习下一节', 'Next lesson') : say('回到目录', 'All lessons')) : say('下一步', 'Next');
  return (
    <section className="lesson" data-kind="device">
      <Header lesson={lesson} device song={!!run?.oldFirmware} />
      <div className="anim-wrap">
        <div className="anim-stage st-stage">
          <StepPicture key={index} step={step} pressed={run?.pressedKeys} />
        </div>
        <button type="button" className="a-show" disabled={!!run?.demo} onClick={() => run?.show()}>
          {run?.demo ? say('演示中…', 'Showing…') : say('▶ 演示', '▶ Show me')}
        </button>
      </div>
      <div className="anim-bar">
        <button type="button" className="a-back" aria-label="Back" disabled={index === 0} onClick={() => run?.back()}>←</button>
        <div className="anim-say">
          <div className="anim-dots">
            {steps.map((_, i) => (
              <button key={i} type="button" aria-label={String(i + 1)}
                className={'a-dot' + (i === index ? ' on' : '') + (run?.done[i] ? ' done' : '')} onClick={() => run?.dot(i)} />
            ))}
          </div>
          <p className="anim-text"><GuideText words={layoutVariant(step, d?.connected() && d.guide ? d.state : null)?.say ?? step.say} /></p>
          {learned && (
            <p key={run?.learnedAt} className="anim-pass pop" role="status">
              <span className="pass-badge" aria-hidden="true" />
              <span className="pass-text">{say(`学会了！再多按几下试试，然后点「${press}」。`, `You have got it! Try a few more keys, then press ${press}.`)}</span>
            </p>
          )}
        </div>
        <button key={learned ? run?.learnedAt : 'wait'} type="button" className={'a-next' + (idle ? ' idle' : '') + (learned ? ' pop' : '')}
          onClick={() => run?.next()}>
          {nextLabel(say, lesson, last)}
        </button>
      </div>
      <Footer lesson={lesson} last={last} complete={!!run?.complete} />
    </section>
  );
}

function AnimLesson({lesson}: {lesson: Lesson}) {
  const say = useSay();
  const history = useHistory();
  const nextUrl = useNextUrl(lesson);
  const sound = useSound();
  const [index, setIndex] = useState(0);
  const [complete, setComplete] = useState(false);
  useRestart(lesson.id, () => {
    setIndex(0);
    setComplete(false);
  });
  const last = index === lesson.steps.length - 1;
  const next = () => {
    if (!last) {
      setIndex(index + 1);
      return;
    }
    setComplete(true);
    markDone(lesson.id);
    toast(['完成！这一课学会了。', 'Done! You have got this one.']);
    history.push(nextUrl);
  };
  const toggleSound = () => {
    setSound(!sound);
    loadAnim().then((anim) => anim.setSound(!sound));
  };
  return (
    <section className="lesson" data-kind="anim">
      <Header lesson={lesson} device={false} />
      <div className="anim-wrap">
        <AnimStage name={lesson.anim || ''} step={index} />
        <button type="button" className="a-sound" aria-pressed={sound} onClick={toggleSound}>♪ {say('声音', 'Sound')}</button>
      </div>
      <div className="anim-bar">
        <button type="button" className="a-back" aria-label="Back" disabled={index === 0} onClick={() => setIndex(Math.max(0, index - 1))}>←</button>
        <div className="anim-say">
          <div className="anim-dots">
            {lesson.steps.map((_, i) => (
              <button key={i} type="button" aria-label={String(i + 1)}
                className={'a-dot' + (i === index ? ' on' : '') + (i < index ? ' seen' : '')} onClick={() => setIndex(i)} />
            ))}
          </div>
          <p className="anim-text"><GuideText words={lesson.steps[index].say} /></p>
        </div>
        <button type="button" className="a-next" onClick={next}>{nextLabel(say, lesson, last)}</button>
      </div>
      <Footer lesson={lesson} last={last} complete={complete} />
    </section>
  );
}

export default function LessonPage({lessonId}: {lessonId: string}) {
  const lang = useLang();
  const lesson = GUIDE.lessons[lessonId];
  return (
    <GuideLayout active={lessonId} title={lesson.title[lang]}>
      {lesson.kind === 'device' ? <DeviceLesson key={lessonId} lesson={lesson} /> : <AnimLesson key={lessonId} lesson={lesson} />}
    </GuideLayout>
  );
}
