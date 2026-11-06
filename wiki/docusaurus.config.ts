// The Learn site (the guide and the manual) on Docusaurus, set up like 203 Systems' Matrix wiki
// (https://github.com/203-Systems/Matrix-Wiki): one sidebar per area, English with a zh-Hans
// translation, the 203 navbar and footer. What is drawn from the firmware (screenshots, key lights,
// reference tables, the guide's lessons) comes from tools/wiki/generate.py into generated/ (not in git).
// The site opens on the guide (src/pages/index.tsx, a page per lesson from plugins/guide-routes.js).
import {themes as prismThemes} from 'prism-react-renderer';
import type {Config} from '@docusaurus/types';
import type * as Preset from '@docusaurus/preset-classic';

// Keep the Cardenza brand in the browser title in every language.
const baseUrl = process.env.CARDENZA_LEARN_BASE_URL || '/learn/';
const simulatorUrl = process.env.CARDENZA_SIMULATOR_URL || 'pathname:///';

const config: Config = {
  title: 'Cardenza',
  tagline: 'Cardenza Groovebox',
  favicon: 'img/favicon.png',

  // Served next to the web simulator, where the Learn site is today (tools/web-deploy.sh).
  url: process.env.CARDENZA_LEARN_URL || 'https://cardenza-synth.203.io',
  baseUrl,
  // Pages as folders (manual/seq/index.html): the Vercel project serves files as they are (no
  // cleanUrls), so /learn/manual/seq/ works there without a rewrite.
  trailingSlash: true,

  onBrokenLinks: 'throw',
  onBrokenAnchors: 'throw',
  markdown: {
    format: 'mdx',
    hooks: {
      onBrokenMarkdownLinks: 'throw',
    },
  },

  i18n: {
    defaultLocale: 'en',
    locales: ['en', 'zh-Hans'],
    localeConfigs: {
      en: {label: 'English'},
      'zh-Hans': {label: '中文'},
    },
  },

  // generated/static holds the screenshots (img/) and reference.html (links of the one-page manual).
  staticDirectories: ['static', 'generated/static'],

  presets: [
    [
      'classic',
      {
        docs: {
          path: 'docs',
          routeBasePath: 'manual',
          sidebarPath: './sidebars.ts',
          breadcrumbs: true,
        },
        blog: false,
        theme: {
          customCss: ['./src/css/custom.css', './src/css/cardenza.css'],
        },
      } satisfies Preset.Options,
    ],
  ],

  plugins: ['./plugins/search-index.js', './plugins/guide-routes.js'],


  themeConfig: {
    // The device and the old Learn site are dark: follow the reader's system.
    colorMode: {
      respectPrefersColorScheme: true,
    },
    docs: {
      sidebar: {
        hideable: true,
        autoCollapseCategories: false,
      },
    },
    navbar: {
      title: 'Cardenza',
      logo: {
        alt: '203 Systems',
        src: 'img/203.svg',
        srcDark: 'img/203dark.svg',
      },
      items: [
        {
          to: '/',
          label: 'Guide',
          position: 'left',
          activeBaseRegex: `^${baseUrl}(zh-Hans/)?(guide/.*)?$`,
        },
        {
          type: 'docSidebar',
          sidebarId: 'manualSidebar',
          position: 'left',
          label: 'Manual',
        },
        {
          href: simulatorUrl,
          target: '_self',
          label: 'Simulator',
          position: 'left',
        },
        {
          type: 'localeDropdown',
          position: 'right',
        },
        {
          href: 'https://github.com/203-Systems',
          position: 'right',
          className: 'header-github-link',
          'aria-label': 'GitHub',
        },
      ],
    },
    footer: {
      style: 'dark',
      links: [
        {
          title: 'Cardenza',
          items: [
            {label: 'Guide', to: '/'},
            {label: 'Manual', to: '/manual/start'},
            {label: 'Simulator', href: simulatorUrl, target: '_self'},
          ],
        },
        {
          title: 'Community',
          items: [
            {label: 'Discord', href: 'https://discord.gg/rRVCBHHPfw'},
            {label: 'Twitter / X', href: 'https://twitter.com/203Systems'},
            {label: 'Youtube', href: 'https://www.youtube.com/channel/UCl9tL-r4NzvGl-EIeVYZZ-g'},
          ],
        },
        {
          title: 'More',
          items: [
            {label: '203 Systems', href: 'https://203.io'},
            {label: 'Matrix wiki', href: 'https://matrix.203.io'},
            {label: 'GitHub', href: 'https://github.com/203-Systems'},
          ],
        },
      ],
      logo: {
        alt: '203 Systems',
        src: 'img/203 systems long dark.svg',
        href: 'https://203.io',
        style: {maxWidth: 300},
      },
      copyright: `Copyright © ${new Date().getFullYear()} 203 Systems`,
    },
    prism: {
      theme: prismThemes.github,
      darkTheme: prismThemes.vsDark,
    },
  } satisfies Preset.ThemeConfig,
};

export default config;
