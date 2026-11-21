import React, {createContext, useContext, useEffect, useRef, useState, type PropsWithChildren} from 'react';
import useBaseUrl from '@docusaurus/useBaseUrl';
import useDocusaurusContext from '@docusaurus/useDocusaurusContext';
import {chinaContacts, discordInvite} from '../../../community';
import {BOARD, DISPLAY, useBoardImage} from '../cardenza/pcb';
import {useUrls} from '../cardenza/common';
import styles from './styles.module.css';

type Region = 'china' | 'international';

const OpenCommunity = createContext<() => void>(() => {});
const dismissalKey = 'cardenza.learn.community.dismissed';

async function getRegion(signal: AbortSignal): Promise<Region> {
  try {
    const response = await fetch('/api/learn-region', {signal});
    if (response.ok) {
      const {region} = await response.json();
      if (region === 'china' || region === 'international') return region;
    }
  } catch {
    // Static previews and temporary network failures can use the device time zone.
  }

  const zone = Intl.DateTimeFormat().resolvedOptions().timeZone;
  return zone === 'Asia/Shanghai' || zone === 'Asia/Urumqi' ? 'china' : 'international';
}

export function CommunityNavbarItem({mobile}: {mobile?: boolean}) {
  const open = useContext(OpenCommunity);
  const {i18n} = useDocusaurusContext();
  if (mobile) return null; // The button stays in the top bar on narrow screens.
  return (
    <button type="button" className={`clean-btn navbar__link ${styles.navTrigger}`}
      aria-haspopup="dialog" onClick={open}>
      {i18n.currentLocale === 'zh-Hans' ? '加群' : 'Discord'}
    </button>
  );
}

export default function CommunityPopupProvider({children}: PropsWithChildren) {
  const {i18n} = useDocusaurusContext();
  const isChineseLocale = i18n.currentLocale === 'zh-Hans';
  const [region, setRegion] = useState<Region | null>(null);
  const openedFromNavbar = useRef(false);

  function openFromNavbar() {
    openedFromNavbar.current = true;
    setRegion(isChineseLocale ? 'china' : 'international');
  }

  useEffect(() => {
    try {
      if (localStorage.getItem(dismissalKey) === '1') return;
    } catch {
      // The dialog can still be dismissed when browser storage is blocked.
    }

    if (isChineseLocale) {
      if (!openedFromNavbar.current) setRegion('china');
      return;
    }

    const request = new AbortController();
    const timeout = window.setTimeout(() => request.abort(), 2000);
    let mounted = true;
    void getRegion(request.signal).then(result => {
      window.clearTimeout(timeout);
      if (mounted && !openedFromNavbar.current) setRegion(result);
    });
    return () => {
      mounted = false;
      window.clearTimeout(timeout);
      request.abort();
    };
  }, [isChineseLocale]);

  return (
    <OpenCommunity.Provider value={openFromNavbar}>
      {children}
      {region && <CommunityDialog region={region} onClose={() => setRegion(null)} />}
    </OpenCommunity.Provider>
  );
}

function CommunityDialog({region, onClose}: {region: Region; onClose: () => void}) {
  const {i18n} = useDocusaurusContext();
  const wechatQr = useBaseUrl(chinaContacts.wechat.qrImage);
  const wechatLogo = useBaseUrl('img/community/wechat.svg');
  const qqLogo = useBaseUrl('img/community/qq.svg');
  const qqQr = useBaseUrl(chinaContacts.qq.qrImage);
  const boardArtwork = useBoardImage();
  const screen = useUrls().shot('instr-dx7-voice');
  const [copyStatus, setCopyStatus] = useState<'ready' | 'done' | 'manual'>('ready');
  const dialog = useRef<HTMLDialogElement>(null);
  const okButton = useRef<HTMLButtonElement>(null);

  useEffect(() => {
    if (!dialog.current || dialog.current.open) return;
    dialog.current.showModal();
    okButton.current?.focus();
  }, []);

  const {wechat, qq} = chinaContacts;
  const chinese = region === 'china' || i18n.currentLocale === 'zh-Hans';

  function rememberDismissal(event: React.FormEvent<HTMLFormElement>) {
    if (new FormData(event.currentTarget).has('dismiss')) {
      try {
        localStorage.setItem(dismissalKey, '1');
      } catch {
        // The form still closes the dialog for this visit.
      }
    }
  }

  async function copyWechat() {
    try {
      await navigator.clipboard.writeText(wechat.id);
      setCopyStatus('done');
    } catch {
      setCopyStatus('manual');
    }
  }

  return (
    <dialog ref={dialog} onClose={onClose} className={styles.dialog} aria-labelledby="learn-community-title"
      aria-describedby="learn-community-description" lang={chinese ? 'zh-Hans' : 'en'}>
      <header className={styles.inviteHeader}>
        <h2 id="learn-community-title">
          {region === 'china' ? <><span className={styles.lead}>加入 </span>Cardenza <span className={styles.groupName}>交流群</span></>
            : chinese ? '加入 203 Systems Discord' : <><span className={styles.lead}>Join the </span>203 Systems Discord.</>}
        </h2>
        <p id="learn-community-description">
          {chinese ? '交流 Cardenza 的用法、作品和灵感。' : 'Share your music and chat with other Cardenza users.'}
        </p>
        <div className={styles.instrument} aria-hidden="true">
          <svg viewBox={`0 0 ${BOARD.w} ${BOARD.h}`} focusable="false">
            <image href={boardArtwork} width={BOARD.w} height={BOARD.h} />
            <image href={screen} x={DISPLAY.x} y={DISPLAY.y} width={DISPLAY.w} height={DISPLAY.h} preserveAspectRatio="none" />
          </svg>
        </div>
      </header>
      {region === 'china' ? (
        <div className={styles.channels}>
          <section className={styles.channel} aria-label="微信">
            <h3>微信</h3>
            <img className={styles.code} src={wechatQr} alt="添加 Null0xcb 的微信二维码" />
            <p className={styles.account}>{wechat.id}</p>
            <p className={styles.hint}>加微信好友，私聊申请进群。</p>
            <button type="button" className={styles.secondary} onClick={copyWechat}>
              <img src={wechatLogo} alt="" />
              {copyStatus === 'done' ? '已复制微信号' : '复制微信号'}
            </button>
            {copyStatus === 'manual' && <p role="status" className={styles.hint}>请选中微信号手动复制。</p>}
          </section>
          <section className={styles.channel} aria-label="QQ 群">
            <h3>QQ 群</h3>
            <img className={styles.code} src={qqQr} alt="加入 Cardenza QQ 群的二维码" />
            <p className={styles.account}>{qq.number}</p>
            <p className={styles.hint}>用 QQ 扫码，或点击加入。</p>
            <a className={styles.primary} href={qq.openUrl} target="_blank" rel="noopener noreferrer">
              <img src={qqLogo} alt="" />
              加入 QQ 群 <span aria-hidden="true">↗</span>
            </a>
          </section>
        </div>
      ) : (
        <div className={styles.discord}>
          <a href={discordInvite} target="_blank" rel="noopener noreferrer">
            <svg className={styles.discordIcon} viewBox="0 0 24 24" fill="currentColor" aria-hidden="true">
              <path d="M20.317 4.37a19.8 19.8 0 0 0-4.927-1.528c-.213.38-.46.893-.63 1.3a18.3 18.3 0 0 0-5.52 0c-.17-.407-.42-.92-.63-1.3A19.7 19.7 0 0 0 3.68 4.37C.56 9.06-.29 13.636.13 18.148a19.9 19.9 0 0 0 6.043 3.01c.488-.66.923-1.36 1.296-2.1a13 13 0 0 1-2.04-.978c.172-.125.34-.255.503-.388 3.935 1.85 8.208 1.85 12.095 0 .165.133.333.263.504.388-.652.385-1.335.713-2.044.98.374.738.81 1.438 1.296 2.098a19.9 19.9 0 0 0 6.046-3.01c.492-5.23-.84-9.765-3.512-13.777ZM8.02 15.33c-1.18 0-2.145-1.08-2.145-2.4 0-1.32.943-2.4 2.145-2.4 1.192 0 2.164 1.09 2.145 2.4 0 1.32-.953 2.4-2.145 2.4Zm7.96 0c-1.18 0-2.145-1.08-2.145-2.4 0-1.32.943-2.4 2.145-2.4 1.192 0 2.164 1.09 2.145 2.4 0 1.32-.943 2.4-2.145 2.4Z" />
            </svg>
            <span>{chinese ? '加入 Discord' : 'Join Discord'}</span>
            <svg className={styles.inviteArrow} viewBox="0 0 24 24" fill="none" aria-hidden="true">
              <path d="M5 19 19 5M6 5h13v13" stroke="currentColor" strokeWidth="1.8" />
            </svg>
          </a>
        </div>
      )}
      <form method="dialog" className={styles.actions} onSubmit={rememberDismissal}>
        <label><input type="checkbox" name="dismiss" />{chinese ? '下次不要提醒我' : "Don't show this again"}</label>
        <button ref={okButton} type="submit" className={styles.ok}>OK</button>
      </form>
    </dialog>
  );
}
