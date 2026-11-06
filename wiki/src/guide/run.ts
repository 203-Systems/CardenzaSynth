// A device lesson as it runs: which step is shown, what is learned, the demo on the key picture and
// on the device (as the one-page guide of before ran them: same conditions, demo pace and
// messages); the page (LessonPage) draws what it holds and calls next / back / dot / show.
import {device, parseState, type DeviceState, type LessonHooks} from './device';
import {markDone, toast} from './store';

export type Words = {zh: string; en: string};
export type Condition = (string | number | number[])[];
export type DemoAction = [string, number];
export type DeviceStep = {
  say: Words;
  keys: number[];
  keySpec: string;
  done: Condition[];
  demo: DemoAction[];
  shot: string;
  marks: string[];
  lights: boolean;
  keymarks: [string, Words][];
};
export type Lesson = {
  id: string;
  kind: 'anim' | 'device';
  part: string;
  number: number;
  of: number;
  next: string | null;
  title: Words;
  lead: Words;
  more: string | null;
  anim?: string;
  start?: string;
  steps: (DeviceStep | {say: Words})[];
};

// The demo's pace: slow enough to follow with the eye. A pressed key stays lit, the next waits, and
// the moment the step wants you to see (its "shot") holds still a while.
const DEMO_PACE = {start: 800, tap: 900, tapLit: 420, down: 900, up: 650, shot: 900, loop: 2200, waitScale: 1.6, waitMin: 350};

const hex = (key: number) => key.toString(16).padStart(2, '0');

export class LessonRun implements LessonHooks {
  index = 0;
  learned: boolean[];
  done: boolean[];
  complete = false;
  /** Keys down on the step's key picture: the demo's, or the ones pressed on the Cardenza. */
  pressedKeys = new Set<number>();
  /** "Show me" is playing on the device. */
  demo = false;
  catchingUp = false;
  waitingStart = true;
  /** When the step was learned on the device (the page lets "learned" and Next pop). */
  learnedAt = 0;
  /** A song lesson (GL d<n>) waits for the device's state to know whether it can start there. */
  pendingStart = false;
  /** The device's firmware cannot start from the song so far: the lesson goes by the pictures. */
  oldFirmware = false;
  version = 0;

  private steps: DeviceStep[];
  private base: DeviceState | null = null;
  private startFields: DeviceState = {};
  private startTimer: ReturnType<typeof setTimeout> | undefined;
  private latched: boolean[] = [];
  private pressed: Set<number>[] = [];
  private demoTimer: ReturnType<typeof setTimeout> | undefined;
  private demoToken = 0;
  private injected = new Set<number>();
  private disposed = false;
  private listeners = new Set<() => void>();

  constructor(private lesson: Lesson, private goNext: () => void) {
    this.steps = lesson.steps as DeviceStep[];
    this.learned = this.steps.map(() => false);
    this.done = this.steps.map(() => false);
  }

  subscribe = (listener: () => void) => {
    this.listeners.add(listener);
    return () => {
      this.listeners.delete(listener);
    };
  };

  private paint() {
    this.version++;
    this.listeners.forEach((l) => l());
  }

  onChange() {
    this.paint();
  }

  /** The lesson opens (or starts over): first step, the device into the guide and to the start. */
  start() {
    this.stopDemo();
    clearTimeout(this.startTimer);
    this.learned = this.steps.map(() => false);
    this.done = this.steps.map(() => false);
    this.complete = false;
    this.waitingStart = true;
    this.catchingUp = false;
    this.base = null;
    this.pendingStart = false;
    this.oldFirmware = false;
    if (device) device.lesson = this;
    this.activateStep(0);
    device?.beginGuide();
    this.applyStart();
  }

  dispose() {
    this.disposed = true;
    this.stopDemo();
    clearTimeout(this.startTimer);
    if (device && device.lesson === this) device.lesson = null;
    device?.leaveSoon();
  }

  onGuideOn() {
    this.applyStart();
  }

  private applyStart() {
    if (!device || !device.guide || this.disposed) return;
    const start = this.lesson.start || '';
    this.waitingStart = true;
    // The song lessons start from the song so far (GL d<n>): firmware without it would load its old
    // demo and could not finish the steps. Its state has no song bars (b): the lesson goes by the pictures.
    this.pendingStart = false;
    this.oldFirmware = false;
    this.paint();
    if (/d\d/.test(start)) {
      if (!device.state) {
        this.pendingStart = true;
        device.send('GQ');
        return;
      }
      if (device.state.b === undefined) {
        this.oldFirmware = true;
        device.send('GH');
        this.paint();
        return;
      }
    }
    this.startFields = parseState(start.replace(/[rd]\d/g, ''));
    if (this.startFields.t !== undefined) this.startFields.t = (this.startFields.t as number) - 1;
    delete this.startFields.n;
    device.send('GL' + start);
    // Plugged in during the lesson: the device first does the steps already done, then the step's
    // state is taken from where that leaves it.
    const earlier = this.steps.slice(0, this.index).flatMap((s) => s.demo);
    if (earlier.length) {
      this.cancelDemo();
      const token = this.demoToken;
      this.catchingUp = true;
      this.paint();
      this.replay(earlier, token, () => {
        if (this.disposed || token !== this.demoToken) return;
        this.catchingUp = false;
        this.waitingStart = false;
        this.base = null;
        this.sendHint();
        device?.send('GQ');
        this.paint();
      });
      return;
    }
    this.sendHint();
    clearTimeout(this.startTimer);
    // The state the lesson starts from: the first one that matches the start, or the latest.
    this.startTimer = setTimeout(() => {
      if (!this.disposed && this.waitingStart && device?.state) this.takeBase(device.state);
    }, 900);
  }

  private takeBase(state: DeviceState) {
    this.waitingStart = false;
    this.base = {...state};
    this.evaluate(state);
  }

  /** With a Cardenza following, the step waits to be learned on it before Next lights up. */
  follows() {
    return !!device && device.connected() && device.guide && !this.oldFirmware;
  }

  private activateStep(index: number) {
    this.index = index;
    this.resetConditions();
    this.base = this.waitingStart ? null : device?.state ? {...device.state} : null;
    this.paint();
    this.sendHint();
    this.playDemo(index, 1);
    if (this.base) this.evaluate(this.base);
  }

  private sendHint() {
    if (this.oldFirmware) return;
    const step = this.steps[this.index];
    const keys = step ? step.keys : [];
    // Blinking while the step waits for the user, steady once it is learned (they try more keys).
    if (device?.guide) device.send((this.learned[this.index] ? 'GT' : 'GH') + keys.map(hex).join(''));
  }

  onState(state: DeviceState) {
    if (this.disposed) return;
    if (this.pendingStart) this.applyStart();
    if (this.demo || this.catchingUp || this.oldFirmware || this.pendingStart) return;
    if (this.waitingStart) {
      const matches = Object.keys(this.startFields).every((f) => state[f] === this.startFields[f]);
      if (matches) this.takeBase(state);
      return;
    }
    if (!this.base) this.base = {...state};
    this.evaluate(state);
  }

  // Nothing met yet; ["press", keys, n] counts the different keys of its list pressed.
  private resetConditions() {
    const conditions = this.steps[this.index].done;
    this.latched = conditions.map(() => false);
    this.pressed = conditions.map(() => new Set<number>());
  }

  onKey(key: number, down: boolean) {
    if (this.disposed) return;
    if (!this.waitingStart && !this.demo && !this.catchingUp && down) {
      this.stopDemo();
      this.steps[this.index].done.forEach((condition, i) => {
        if (condition[0] !== 'press' || !(condition[1] as number[]).includes(key)) return;
        this.pressed[i].add(key);
        if (this.pressed[i].size >= ((condition[2] as number) || 1)) this.latched[i] = true;
      });
      this.finishIfDone();
    }
    // A key pressed on the Cardenza goes down on the step's key picture.
    if (down) this.pressedKeys.add(key);
    else this.pressedKeys.delete(key);
    this.paint();
  }

  private evaluate(state: DeviceState) {
    if (this.waitingStart || this.demo || this.catchingUp || !this.base) return;
    if (this.learned[this.index]) return;
    const base = this.base;
    this.steps[this.index].done.forEach((condition, i) => {
      const [kind, field, value] = condition as [string, string, number];
      if ((kind === 'state' || kind === 'seen') && state[field] === value) this.latched[i] = true;
      if (kind === 'min' && (state[field] as number) >= value) this.latched[i] = true;
      if (kind === 'change' && state[field] !== base[field]) this.latched[i] = true;
    });
    this.finishIfDone();
  }

  private finishIfDone() {
    if (!this.latched.length || !this.latched.every(Boolean)) return;
    this.completeStep(this.index, 'device');
  }

  // by: 'device' (done on the Cardenza: learned, the page stays) or 'next' (the Next button: on to
  // the next step, or the lesson is done).
  private completeStep(index: number, by: 'device' | 'next') {
    if (by === 'device') {
      if (this.learned[index]) return;
      this.stopDemo();
      this.learned[index] = true;
      this.done[index] = true;
      this.learnedAt = Date.now();
      this.sendHint();
      this.paint();
      return;
    }
    this.stopDemo();
    this.done[index] = true;
    if (index + 1 < this.steps.length) this.activateStep(index + 1);
    else {
      if (!this.complete) this.lessonComplete();
      this.goNext();
    }
  }

  private lessonComplete() {
    this.complete = true;
    markDone(this.lesson.id);
    if (device?.guide) device.send('GH');
    this.paint();
    toast(['完成！这一课学会了。', 'Done! You have got this one.']);
  }

  // The buttons. While the device shows or catches up, it finishes first.
  private busy() {
    return this.demo || this.catchingUp;
  }

  next() {
    if (!this.busy()) this.completeStep(this.index, 'next');
  }

  back() {
    if (!this.busy() && this.index > 0) this.activateStep(this.index - 1);
  }

  dot(index: number) {
    if (!this.busy()) this.activateStep(index);
  }

  show() {
    if (!this.busy()) this.showOnDevice(this.index);
  }

  // The step's demo on its key picture: keys go down and light as a person would press them. "Show me"
  // has the device do it as well (keys the page presses, GK, which never count for the step).
  private inject(key: number, down: boolean) {
    if (down) this.injected.add(key);
    else this.injected.delete(key);
    device?.send('GK' + hex(key) + (down ? '1' : '0'));
  }

  private releaseInjected() {
    [...this.injected].forEach((key) => this.inject(key, false));
  }

  private cancelDemo() {
    this.demoToken++;
    clearTimeout(this.demoTimer);
    this.releaseInjected();
    if (this.pressedKeys.size) {
      this.pressedKeys.clear();
      this.paint();
    }
  }

  private stopDemo() {
    this.cancelDemo();
    if (this.demo) this.endDeviceDemo();
  }

  private press(key: number, down: boolean) {
    if (down) this.pressedKeys.add(key);
    else this.pressedKeys.delete(key);
    this.paint();
  }

  private playDemo(index: number, loops: number, onDevice = false, after?: (token: number) => void) {
    this.cancelDemo();
    const token = this.demoToken;
    const actions = this.steps[index].demo;
    let at = 0;
    const step = () => {
      if (token !== this.demoToken) return;
      if (at >= actions.length) {
        this.pressedKeys.clear();
        this.paint();
        if (onDevice) this.releaseInjected();
        if (--loops > 0) this.demoTimer = setTimeout(() => {
          at = 0;
          step();
        }, DEMO_PACE.loop);
        else if (after) after(token);
        return;
      }
      const [verb, value] = actions[at++];
      let wait = DEMO_PACE.up;
      if (verb === 'tap') {
        this.press(value, true);
        // On the device a tap is short: held 200 ms a GRID cell opens its value picker instead.
        if (onDevice) {
          this.inject(value, true);
          setTimeout(() => {
            if (token === this.demoToken) this.inject(value, false);
          }, 70);
        }
        setTimeout(() => {
          if (token === this.demoToken) this.press(value, false);
        }, DEMO_PACE.tapLit);
        wait = DEMO_PACE.tap;
      } else if (verb === 'down') {
        this.press(value, true);
        if (onDevice) this.inject(value, true);
        wait = DEMO_PACE.down;
      } else if (verb === 'up') {
        this.press(value, false);
        if (onDevice) this.inject(value, false);
      } else if (verb === 'wait') {
        // On the device a wait is the time the demo needs (a held key, a song playing).
        const ms = value * 16 * DEMO_PACE.waitScale;
        wait = Math.max(DEMO_PACE.waitMin, Math.min(onDevice ? 5000 : 2200, ms));
      } else if (verb === 'shot') {
        wait = DEMO_PACE.shot;
      }
      this.demoTimer = setTimeout(step, wait);
    };
    this.demoTimer = setTimeout(step, DEMO_PACE.start);
  }

  // "Show me" with the device following: it goes to where the step begins (a fresh practice project
  // at the lesson's start, and the earlier steps' demos again), does the step so it is seen and heard,
  // and goes back there. The step is then the user's to do.
  private showOnDevice(index: number) {
    if (!device?.guide || this.waitingStart || this.oldFirmware) {
      this.playDemo(index, 1);
      return;
    }
    this.demo = true;
    this.paint();
    const start = this.lesson.start || '';
    const toStepStart = (token: number, then: () => void) => {
      device?.send('GL' + (/[rd]\d/.test(start) ? start : 'r1' + start));
      this.replay(this.steps.slice(0, index).flatMap((s) => s.demo), token, then);
    };
    this.cancelDemo();
    const first = this.demoToken;
    toStepStart(first, () => {
      if (first !== this.demoToken || this.disposed) return;
      this.playDemo(index, 1, true, (token) => {
        // The result stays a while to be seen, then the device goes back.
        this.demoTimer = setTimeout(() => {
          if (token !== this.demoToken || this.disposed) return;
          toStepStart(token, () => {
            if (token !== this.demoToken || this.disposed) return;
            this.endDeviceDemo();
            this.base = null;
            this.resetConditions();
            this.sendHint();
            device?.send('GQ');
            toast(['轮到你了：在机器上照着做一遍。', 'Your turn: now do it on the device.']);
          });
        }, 2200);
      });
    });
  }

  private endDeviceDemo() {
    this.demo = false;
    this.paint();
  }

  // The earlier steps' demos, quickly, after the lesson's start (GL) has taken effect. Held keys keep
  // their waits, so a key map still opens.
  private replay(actions: DemoAction[], token: number, done: () => void) {
    let at = 0;
    const next = () => {
      if (token !== this.demoToken) return;
      if (at >= actions.length) {
        this.releaseInjected();
        this.demoTimer = setTimeout(done, 300);
        return;
      }
      const [verb, value] = actions[at++];
      let wait = 90;
      if (verb === 'tap') {
        this.inject(value, true);
        setTimeout(() => {
          if (token === this.demoToken) this.inject(value, false);
        }, 60);
        wait = 130;
      } else if (verb === 'down') {
        this.inject(value, true);
        wait = 130;
      } else if (verb === 'up') {
        this.inject(value, false);
      } else if (verb === 'wait') {
        wait = Math.max(60, Math.min(600, value * 16));
      } else {
        wait = 0;
      }
      this.demoTimer = setTimeout(next, wait);
    };
    this.demoTimer = setTimeout(next, 400);
  }
}
