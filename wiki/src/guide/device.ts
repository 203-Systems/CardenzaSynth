// The Cardenza on USB that follows the guide, through Web MIDI: one connection for the whole site,
// so it stays while the reader moves between lessons and the manual (the device part of the one-page
// guide of before: same messages, timings and states). The guide pages ask for MIDI (SysEx) when
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
    if (was) toast(['鸣片已断开连接。你可以继续看图学习，或重新插线自动连接。', 'Cardenza disconnected. You can continue with the diagrams, or plug it back in to reconnect.']);
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
      toast(['鸣片已连接，正与课程同步联动。', 'Cardenza connected and syncing with the lessons.']);
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
      this.reason = text.slice(4) in REASONS ? text.slice(4) : 'other';
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
  rec: ['设备正在录音，请先停止录音后点击此处重试。', 'Recording is active on the device. Stop recording, then click here to retry.'],
  save: ['当前工程体积过大，无法临时暂存。请先按 FN+S 保存至 SD 卡，在 SETUP 新建空工程后再点击此处重试。',
    'Your current project is too large to stash temporarily. Save it with FN+S, start a new project in SETUP, then click here to retry.'],
  mem: ['设备内存不足，请重启设备后点击此处重试。', 'Device is low on memory. Restart it, then click here to retry.'],
  card: ['未检测到 SD 卡。交互教程需要 SD 卡来暂存你当前的工作会话，请插卡重启后再点击此处重试。',
    'No SD card detected. Interactive lessons need an SD card to safely stash your current session. Insert a card, restart the device, then click here to retry.'],
  other: ['设备未能进入教程模式，请点击此处重试。', 'Could not start guide mode on the device. Click here to retry.'],
};

export const STATUS: Record<Status | 'song', [string, string]> = {
  nomidi: ['请使用电脑端 Chrome 或 Edge 打开本页并连接鸣片，即可开启硬件联动教学。', 'Open this page in Chrome or Edge on a desktop browser and connect your Cardenza to follow along on hardware.'],
  denied: ['如需联动鸣片，请在地址栏左侧的网站设置中允许 MIDI 设备，然后刷新页面。', 'To connect your Cardenza, allow MIDI devices in site settings (left of the address bar) and reload the page.'],
  waiting: ['通过 USB 连接鸣片即可开启硬件联动，也可直接按下方图示学习。', 'Connect your Cardenza over USB to follow along interactively, or continue using the diagrams below.'],
  hello: ['正在连接鸣片…', 'Connecting to Cardenza...'],
  old: ['当前固件暂不支持交互教程，请更新至最新固件后重新连接。', 'Your firmware does not support interactive lessons yet. Update your firmware and reconnect.'],
  // A song lesson on firmware that cannot start from the song so far (its state has no song bars).
  song: ['本课需要更新版本的固件才能加载歌曲状态，请先按图示学习。', 'This lesson requires newer firmware to load the song state. Follow along using the diagrams for now.'],
  on: ['鸣片已连接。按设备上闪烁的引导键操作，完成后点击「下一步」。', 'Cardenza connected. Press the blinking keys on the device, then click Next when done.'],
};
