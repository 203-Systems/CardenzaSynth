// Every page can use the Cardenza components without importing them (the manual's MDX writes them in
// place: <Key k="FN+6" />, <Shot name="...">, ...).
import MDXComponents from '@theme-original/MDXComponents';
import {Icon} from '@site/src/components/cardenza/common';
import {Key, Keywords, Led, Page, Scene, Track} from '@site/src/components/cardenza/chips';
import {Caption, Group, KeyMark, Keymap, Mark, Shot, Step, Strip} from '@site/src/components/cardenza/figures';
import {Ref} from '@site/src/components/cardenza/reference';
import {DeviceMap, SceneMap} from '@site/src/components/cardenza/maps';

export default {
  ...MDXComponents,
  Icon, Key, Keywords, Led, Page, Scene, Track,
  Shot, Mark, KeyMark, Caption, Strip, Step, Keymap, Group,
  Ref, DeviceMap, SceneMap,
};
