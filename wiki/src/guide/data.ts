// The guide's lessons (generated/data/guide.json, from docs/learn/guide.json by tools/wiki/generate.py)
// and the key lights of their pictures.
import guideJson from '@site/generated/data/guide.json';
import guideLeds from '@site/generated/data/guide-leds.json';
import type {Lesson, Words} from './run';

export type Part = {id: string; title: Words; lessons: string[]};
export type Guide = {title: Words; order: string[]; parts: Part[]; lessons: Record<string, Lesson>};

export const GUIDE = guideJson as unknown as Guide;
export const GUIDE_LEDS = guideLeds as unknown as Record<string, number[][]>;

export const partOf = (lesson: Lesson) => GUIDE.parts.find((p) => p.id === lesson.part)!;
export const lessonPath = (id: string) => `/guide/${id}/`;

/** The guide's animations (tools/learn/guide-anim.js): a browser-only script that sets window.GuideAnim. */
export type AnimPlayer = {setStep(index: number): void; destroy(): void};
export type GuideAnim = {
  create(container: HTMLElement, name: string): AnimPlayer;
  thumb(container: HTMLElement, name: string): AnimPlayer;
  setSound(on: boolean): void;
};

let loading: Promise<GuideAnim> | null = null;
export function loadAnim(): Promise<GuideAnim> {
  // @ts-expect-error: a plain script (no exports), run for what it sets on window
  loading = loading || import('@site/../tools/learn/guide-anim.js').then(
    () => (window as unknown as {GuideAnim: GuideAnim}).GuideAnim);
  return loading;
}
