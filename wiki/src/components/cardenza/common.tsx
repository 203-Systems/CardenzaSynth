import React from 'react';
import useDocusaurusContext from '@docusaurus/useDocusaurusContext';
import {useBaseUrlUtils} from '@docusaurus/useBaseUrl';
import {ICONS} from './icons';
import sections from '@site/generated/data/sections.json';

export type Lang = 'zh' | 'en';

/** The reader's language as the old manual names it (zh / en). */
export function useLang(): Lang {
  const {i18n} = useDocusaurusContext();
  return i18n.currentLocale.startsWith('zh') ? 'zh' : 'en';
}

export const say = (lang: Lang, zh: string, en: string) => (lang === 'zh' ? zh : en);

/** URL of a section of the manual (its chapter's page), and of a screenshot. */
export function useUrls() {
  const {withBaseUrl} = useBaseUrlUtils();
  const path = (id: string): string => {
    const entry = (sections as Record<string, {chapter: string}>)[id];
    if (!entry) throw new Error(`cardenza: link to unknown section #${id}`);
    return `/manual/${entry.chapter}/` + (entry.chapter === id ? '' : `#${id}`);
  };
  return {
    /** For <Link to>: without the base URL (Link adds it, and checks the link). */
    sectionPath: path,
    section(id: string): string {
      return withBaseUrl(path(id));
    },
    shot(name: string): string {
      return withBaseUrl(`/img/${name}.png`);
    },
  };
}

/** An icon of ./icons.ts (24 x 24 strokes). */
export function Icon({name, className}: {name: string; className?: string}) {
  const body = ICONS[name];
  if (body === undefined) throw new Error(`cardenza: unknown icon ${name}`);
  return (
    <svg className={'cz-ico' + (className ? ' ' + className : '')} viewBox="0 0 24 24" aria-hidden="true"
      dangerouslySetInnerHTML={{__html: body}} />
  );
}

/** The plain text of some rendered children (for alt text). */
export function nodeText(node: React.ReactNode): string {
  if (node === null || node === undefined || typeof node === 'boolean') return '';
  if (typeof node === 'string' || typeof node === 'number') return String(node);
  if (Array.isArray(node)) return node.map(nodeText).join('');
  if (React.isValidElement(node)) {
    const props = node.props as {children?: React.ReactNode; k?: string; name?: string};
    return nodeText(props.children) || props.k || props.name || '';
  }
  return '';
}

/** A marker component's role, so a figure finds its parts among its children. */
export type Role = 'mark' | 'keymark' | 'caption' | 'step' | 'group';

export function parts<P>(children: React.ReactNode, role: Role): P[] {
  return React.Children.toArray(children)
    .filter((child): child is React.ReactElement<P> =>
      React.isValidElement(child) && (child.type as {czRole?: Role}).czRole === role)
    .map((child) => child.props);
}

export function marker<P>(role: Role): React.FC<P> & {czRole: Role} {
  const component = (() => null) as unknown as React.FC<P> & {czRole: Role};
  component.czRole = role;
  return component;
}
