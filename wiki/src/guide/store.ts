// Small shared stores of the guide: lessons done (localStorage "guide.done", as the one-page guide
// kept it, so progress made there carries over) and the toast line.
import {useEffect, useRef, useSyncExternalStore} from 'react';

type Listener = () => void;

function createStore<T>(initial: T) {
  let value = initial;
  const listeners = new Set<Listener>();
  return {
    get: () => value,
    set(next: T) {
      value = next;
      listeners.forEach((l) => l());
    },
    subscribe(listener: Listener) {
      listeners.add(listener);
      return () => listeners.delete(listener);
    },
  };
}

// ---- Progress ---------------------------------------------------------------------------------------
const KEY = 'guide.done';

function load(): Record<string, boolean> {
  try {
    return JSON.parse(localStorage.getItem(KEY) || '{}') || {};
  } catch (error) {
    return {};
  }
}

const progress = createStore<Record<string, boolean>>({});
let loaded = false;

function ensureLoaded() {
  if (!loaded && typeof window !== 'undefined') {
    loaded = true;
    progress.set(load());
  }
}

function save(done: Record<string, boolean>) {
  try {
    localStorage.setItem(KEY, JSON.stringify(done));
  } catch (error) { /* private mode */ }
  progress.set(done);
}

export function markDone(id: string) {
  ensureLoaded();
  save({...progress.get(), [id]: true});
}

/** Clears the ticks of these lessons; returns how many there were. */
export function clearDone(ids: string[]): number {
  ensureLoaded();
  const done = {...progress.get()};
  const cleared = ids.filter((id) => done[id]).length;
  ids.forEach((id) => delete done[id]);
  save(done);
  return cleared;
}

const EMPTY = {};
export function useProgress(): Record<string, boolean> {
  return useSyncExternalStore(
    (listener) => {
      ensureLoaded();
      return progress.subscribe(listener);
    },
    () => progress.get(),
    () => EMPTY,
  );
}

// ---- Learning again ---------------------------------------------------------------------------------
// The rail's "start all over" and a lesson's "learn again" restart the lesson shown when it is the one.
const restart = createStore<{id: string; at: number} | null>(null);

export function requestRestart(id: string) {
  restart.set({id, at: Date.now()});
}

export function useRestart(id: string, onRestart: () => void) {
  const request = useSyncExternalStore(restart.subscribe, restart.get, () => null);
  const seen = useRef(request?.at ?? 0);
  useEffect(() => {
    if (request && request.id === id && request.at !== seen.current) {
      seen.current = request.at;
      onRestart();
    }
  });
}

// ---- Concept lessons' sound (off until a click turns it on: browsers start audio on a gesture) ------
const sound = createStore(false);
export const setSound = (on: boolean) => sound.set(on);
export const useSound = () => useSyncExternalStore(sound.subscribe, sound.get, () => false);

// ---- Toast --------------------------------------------------------------------------------------
export type Words = [string, string]; // [zh, en]

const toastStore = createStore<{words: Words; at: number} | null>(null);
let toastTimer: ReturnType<typeof setTimeout> | undefined;

export function toast(words: Words, ms = 3600) {
  toastStore.set({words, at: Date.now()});
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => toastStore.set(null), ms);
}

export function useToast() {
  return useSyncExternalStore(toastStore.subscribe, toastStore.get, () => null);
}
