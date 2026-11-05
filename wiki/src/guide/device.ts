// The Cardenza on USB that follows the guide, through Web MIDI: one connection for the whole site,
// so it stays while the reader moves between lessons and the manual. A port of the device part of
// tools/learn/guide.js (same messages, timings and states). The guide pages ask for MIDI (SysEx) when
// they open (start()): the browser asks the user once, and from then on a Cardenza plugged in, now or
// later, is found by itself. A device lesson starts the guide on it (GE): the device sets the user's
// work aside in a practice project, goes where the lesson starts (GL), blinks the keys of the step
// until it is learned (GH, then GT steady) and reports its state (GS) and every key pressed (GK).
// The messages (Applications/Synth/GrooveAppGuide.cpp): SysEx F0 23 7F <text> F7 to the Cardenza,
// F0 24 7F <text> F7 back.
//   status: nomidi (no Web MIDI here) | denied | waiting (allowed, no Cardenza) | hello (asking it) |
//           old (its firmware has no guide) | on
import {useSyncExternalStore} from 'react';
import {toast} from './store';

export type Status = 'nomidi' | 'denied' | 'waiting' | 'hello' | 'old' | 'on';
export type DeviceState = Record<string, number | string>;

/** What a running device lesson hears (src/guide/run.ts). */
export interface LessonHooks {
  onGuideOn(): void;
  onState(state: DeviceState): void;
  onKey(key: number, down: boolean): void;
  onChange(): void;
}

export function parseState(text: string): DeviceState {
  const at = text.indexOf('n');
  const state: DeviceState = {n: at >= 0 ? text.slice(at + 1) : ''};
  (at >= 0 ? text.slice(0, at) : text).replace(/([a-z])(\d+)/g, (_, letter: string, value: string) => {
    state[letter] = Number(value);
    return '';
  });
  return state;
}

const isCardenza = (port: MIDIPort) => port.state === 'connected' && /cardenza/i.test(`${port.name || ''} ${port.manufacturer || ''}`);

class GuideDevice {
  status: Status = 'waiting';
  guide = false;
  entering = false;
  leaving = false;
  state: DeviceState | null = null;
  want = false;
  reason: string | null = null;
  lesson: LessonHooks | null = null;
  version = 0;

  private midi: MIDIAccess | null = null;
  private input: MIDIInput | null = null;
  private output: MIDIOutput | null = null;
  private helloTimer: ReturnType<typeof setTimeout> | undefined;
  private endTimer: ReturnType<typeof setTimeout> | undefined;
  private started = false;
  private listeners = new Set<() => void>();

  connected() {
    return this.status === 'on';
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
    this.lesson?.onChange();
  }

  /** The guide pages call this as they open: ask for MIDI once, keep the guide alive while it runs. */
  start() {
    if (this.started) return;
    this.started = true;
    this.askMidi();
    setInterval(() => {
      if (this.guide) this.send('GP');
    }, 4000);
    document.addEventListener('visibilitychange', () => {
      if (document.visibilityState === 'visible' && this.want && !this.guide) this.beginGuide();
    });
    window.addEventListener('pagehide', () => {
      if (this.guide) this.send('GX');
    });
  }

  private usbSend(text: string) {
    if (!this.output) return;
    const bytes = [0xf0, 0x23, 0x7f];
    for (let i = 0; i < text.length && i < 60; i++) bytes.push(text.charCodeAt(i) & 0x7f);
    bytes.push(0xf7);
    try {
      this.output.send(bytes);
    } catch (error) {
      this.detach();
    }
  }

  send(text: string) {
    if (this.connected()) this.usbSend(text);
  }

  private setStatus(status: Status) {
    this.status = status;
    this.paint();
  }

  // A Cardenza among the ports: take it and ask whether its firmware has the guide.
  private attach = () => {
    if (!this.midi || this.input) return;
    const inputs: MIDIInput[] = [];
    const outputs: MIDIOutput[] = [];
    this.midi.inputs.forEach((port) => inputs.push(port));
    this.midi.outputs.forEach((port) => outputs.push(port));
    const input = inputs.find(isCardenza);
    const output = outputs.find(isCardenza);
    if (!input || !output) {
      if (this.status !== 'old') this.setStatus('waiting');
      return;
    }
    this.input = input;
    this.output = output;
    input.onmidimessage = (event) => this.onMidi(event);
    this.setStatus('hello');
    this.usbSend('G?');
    clearTimeout(this.helloTimer);
    this.helloTimer = setTimeout(() => {
      if (this.status !== 'hello') return;
      this.release();
      this.setStatus('old');
    }, 2000);
  };

  private release() {
    clearTimeout(this.helloTimer);
    if (this.input) this.input.onmidimessage = null;
    this.input = null;
    this.output = null;
  }

  // The Cardenza went away: the lesson goes on as pictures.
  private detach() {
    const was = this.connected();
    this.release();
    this.guide = false;
    this.entering = false;
    this.state = null;
    this.reason = null;
    this.setStatus('waiting');
    if (was) toast(['鸣片断开了：接着看图学，插回去会自动连上。', 'Your Cardenza is gone: go on with the pictures; plug it back in and it reconnects.']);
  }

  private onMidi(event: MIDIMessageEvent) {
    const bytes = event.data;
    if (!bytes || bytes.length < 4 || bytes[0] !== 0xf0 || bytes[1] !== 0x24 || bytes[2] !== 0x7f) return;
    let text = '';
    for (let i = 3; i < bytes.length && bytes[i] !== 0xf7; i++) text += String.fromCharCode(bytes[i]);
    if (text.startsWith('G!')) {
      clearTimeout(this.helloTimer);
      if (this.connected()) return;
      this.setStatus('on');
      toast(['鸣片已连接：它会跟着课程走。', 'Your Cardenza is connected: it follows the lessons.']);
      if (this.want) this.beginGuide();
      return;
    }
    if (this.connected()) this.heard(text);
  }

  private async askMidi() {
    if (!('requestMIDIAccess' in navigator)) {
      this.setStatus('nomidi');
      return;
    }
    try {
      this.midi = await navigator.requestMIDIAccess({sysex: true});
    } catch (error) {
      this.setStatus('denied');
      return;
    }
    this.midi.onstatechange = (event) => {
      const port = (event as MIDIConnectionEvent).port;
      if (!port) return;
      if (this.input && port.state === 'disconnected' && (port === this.input || port === this.output)) this.detach();
      // Plugged in: both of its ports appear, one after the other.
      else if (!this.input && port.state === 'connected' && isCardenza(port)) setTimeout(this.attach, 300);
    };
    this.attach();
  }

  beginGuide() {
    this.want = true;
    clearTimeout(this.endTimer);
    if (!this.connected() || this.guide || this.entering) return;
    this.entering = true;
    this.leaving = false;
    this.reason = null;
    this.send('GE');
    setTimeout(() => {
      if (this.entering) {
        this.entering = false;
        this.paint();
      }
    }, 3000);
    this.paint();
  }

  endGuide() {
    this.want = false;
    clearTimeout(this.endTimer);
    if (!this.guide) return;
    this.leaving = true;
    this.send('GX');
    this.guide = false;
    this.paint();
  }

  /** A device lesson closed: end the guide on the device unless another lesson takes it on at once
   *  (going from lesson to lesson keeps the practice project, as on the one-page guide). */
  leaveSoon() {
    this.want = false;
    clearTimeout(this.endTimer);
    this.endTimer = setTimeout(() => {
      if (!this.want) this.endGuide();
    }, 600);
  }

  retry() {
    if (!this.reason) return;
    this.reason = null;
    this.beginGuide();
  }

  private heard(text: string) {
    if (text === 'GE1') {
      this.guide = true;
      this.entering = false;
      this.paint();
      this.lesson?.onGuideOn();
    } else if (text.startsWith('GE0')) {
      this.entering = false;
      this.reason = ['rec', 'save', 'mem'].includes(text.slice(4)) ? text.slice(4) : 'other';
      this.paint();
    } else if (text === 'GX') {
      const ours = this.leaving;
      this.guide = false;
      this.leaving = false;
      this.paint();
      // The device ended it by itself (the page was quiet): back in when someone is here.
      if (!ours && this.want && document.visibilityState === 'visible') setTimeout(() => this.beginGuide(), 400);
    } else if (text.startsWith('GS')) {
      this.state = parseState(text.slice(2));
      this.lesson?.onState(this.state);
    } else if (/^GK[0-9a-f]{2}[01]$/.test(text)) {
      this.lesson?.onKey(parseInt(text.slice(2, 4), 16), text[4] === '1');
    }
  }
}

export const device: GuideDevice | null = typeof window === 'undefined' ? null : new GuideDevice();

/** Re-renders when the device's connection or guide state changes. */
export function useDevice(): GuideDevice | null {
  useSyncExternalStore(
    (listener) => (device ? device.subscribe(listener) : () => {}),
    () => (device ? device.version : 0),
    () => 0,
  );
  return device;
}

export const REASONS: Record<string, [string, string]> = {
  rec: ['机器正在录音：先停下录音，再点这里重试。', 'The device is recording: stop it, then click here to try again.'],
  save: ['你现在的工程太长，教程没法替你暂存。先用 FN+S 存到卡上，在 SETUP 里新建一个空工程，再点这里重试。',
    'Your project is too long to set aside. Save it with FN+S, start a new project in SETUP, then click here to try again.'],
  mem: ['机器的内存不够：重启一下，再点这里重试。', 'The device is short of memory: restart it, then click here to try again.'],
  other: ['机器没能开始教程：点这里重试。', 'The device could not start the guide: click here to try again.'],
};

export const STATUS: Record<Status, [string, string]> = {
  nomidi: ['用电脑上的 Chrome 或 Edge 打开这一页，再接上鸣片：机器会跟着课程走。', 'Open this page in Chrome or Edge on a computer and plug in your Cardenza: it follows the lessons.'],
  denied: ['要让鸣片跟着课程走：在地址栏左边的网站设置里允许 MIDI 设备，再刷新这一页。', 'To have your Cardenza follow the lessons, allow MIDI devices in the site settings (left of the address bar) and reload.'],
  waiting: ['用 USB 线接上鸣片，它会跟着这一课走；没有机器也可以看图跟着学。', 'Plug in your Cardenza with a USB cable and it follows this lesson; no device, follow the pictures.'],
  hello: ['正在连接鸣片…', 'Connecting to your Cardenza…'],
  old: ['这台鸣片的固件还不支持教程：更新到最新固件，再接上。', "This Cardenza's firmware has no guide yet: update it, then plug it in again."],
  on: ['鸣片已连接：照着机器上闪的键按，学会了再点「下一步」。', 'Your Cardenza is connected: press the keys blinking on it; when you have got it, press Next.'],
};
