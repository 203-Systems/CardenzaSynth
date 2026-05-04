<script>
  import {
    FN_ACTIONS,
    HOLD_DELAY_MS,
    closeFnPopup,
    getCurrentMatrix,
    getDisplaySubtitle,
    getDisplayTitle,
    getKeyboardKeys,
    getSelectionInfo,
    holdTrack,
    overlayLabels,
    pressKeyboardKey,
    releaseKeyboardKey,
    resetSimulator,
    selectMatrixCell,
    simulator,
    tapTrack,
    toggleFnPopup,
    adjustSelection,
  } from './lib/simulator.js'

  let trackGestures = {}
  let keyPointers = new Map()

  const trackButtonNames = ['Tone', 'Params', 'Layout', 'Other']

  function clearTrackGesture(index) {
    const gesture = trackGestures[index]
    if (gesture?.timer) {
      clearTimeout(gesture.timer)
    }

    trackGestures = Object.fromEntries(
      Object.entries(trackGestures).filter(([key]) => Number(key) !== index),
    )
  }

  function handleTrackPointerDown(index, event) {
    clearTrackGesture(index)
    event.currentTarget.setPointerCapture?.(event.pointerId)

    const timer = window.setTimeout(() => {
      holdTrack(index)
      trackGestures = {
        ...trackGestures,
        [index]: {
          pointerId: event.pointerId,
          held: true,
        },
      }
    }, HOLD_DELAY_MS)

    trackGestures = {
      ...trackGestures,
      [index]: {
        pointerId: event.pointerId,
        held: false,
        timer,
      },
    }
  }

  function handleTrackPointerUp(index, event) {
    const gesture = trackGestures[index]
    if (!gesture || gesture.pointerId !== event.pointerId) {
      return
    }

    if (gesture.timer) {
      clearTimeout(gesture.timer)
    }

    if (!gesture.held) {
      tapTrack(index)
    }

    clearTrackGesture(index)
  }

  function handleTrackPointerCancel(index) {
    clearTrackGesture(index)
  }

  function handleKeyPointerDown(key, event) {
    if (keyPointers.has(event.pointerId)) {
      return
    }

    event.currentTarget.setPointerCapture?.(event.pointerId)
    keyPointers = new Map(keyPointers).set(event.pointerId, key.id)
    pressKeyboardKey(key.id)
  }

  function endKeyPointer(event) {
    if (!keyPointers.has(event.pointerId)) {
      return
    }

    const keyIndex = keyPointers.get(event.pointerId)
    releaseKeyboardKey(keyIndex)
    const next = new Map(keyPointers)
    next.delete(event.pointerId)
    keyPointers = next
  }

  $: track = $simulator.tracks[$simulator.activeTrack]
  $: matrix = getCurrentMatrix($simulator)
  $: keyboardKeys = getKeyboardKeys($simulator)
  $: selection = getSelectionInfo($simulator)
  $: displayTitle = getDisplayTitle($simulator)
  $: displaySubtitle = getDisplaySubtitle($simulator)
  $: reversedLog = [...$simulator.inputLog].reverse()
</script>

<svelte:head>
  <title>ComputeCard Synth Web Simulator</title>
</svelte:head>

<div class="sim-shell">
  <header class="hero-bar">
    <div>
      <p class="eyebrow">ComputeCardOS / ComputeCardSim</p>
      <h1>Browser Synth Simulator</h1>
      <p class="hero-copy">
        A lightweight web simulator inspired by MatrixOS MystrixSim. It mirrors the current synth app interaction model:
        4 synth tracks, hold-to-open parameter matrices, a 3x4 selector rail, an FN popup, and a playable 4x14 keyboard.
      </p>
    </div>

    <div class="hero-meta">
      <span class="meta-pill">WebUI</span>
      <span class="meta-pill">Svelte + Vite</span>
      <span class="meta-pill">4 Synth Ports</span>
      <button class="reset-button" on:click={resetSimulator}>Reset State</button>
    </div>
  </header>

  <main class="sim-grid">
    <section class="device-panel">
      <div class="device-chassis">
        <div class="top-deck">
          <div class="track-rail">
            {#each $simulator.tracks as trackItem, index}
              <button
                class="track-key"
                class:track-key-active={$simulator.activeTrack === index}
                class:track-key-overlay={$simulator.overlay === (index === 0 ? 'tone' : index === 1 ? 'parameter' : index === 2 ? 'layout' : 'other')}
                on:pointerdown={(event) => handleTrackPointerDown(index, event)}
                on:pointerup={(event) => handleTrackPointerUp(index, event)}
                on:pointercancel={() => handleTrackPointerCancel(index)}
              >
                <span class="track-key-id">T{index + 1}</span>
                <span class="track-key-name">{trackItem.synth}</span>
                <span class="track-key-hint">Tap switch / Hold {trackButtonNames[index]}</span>
              </button>
            {/each}
          </div>

          <div class="display-stack">
            <div class="display-frame">
              <div class="display-scanlines"></div>
              <div class="display-content">
                <div class="display-header">
                  <div>
                    <p class="display-kicker">Synth App</p>
                    <h2>{displayTitle}</h2>
                  </div>
                  <span class="display-port">{track.port}</span>
                </div>

                <p class="display-subtitle">{displaySubtitle}</p>

                {#if $simulator.fnPopupOpen}
                  <div class="fn-popup">
                    {#each FN_ACTIONS as action}
                      <button class="fn-action" on:click={closeFnPopup}>
                        <span>{action.label}</span>
                        <small>{action.caption}</small>
                      </button>
                    {/each}
                  </div>
                {:else}
                  {#if $simulator.overlay !== 'none'}
                    <div class="matrix-preview">
                      {#each matrix as entry}
                        <div class="matrix-preview-cell" class:matrix-preview-selected={selection?.id === entry.id}>
                          <span>{entry.label}</span>
                        </div>
                      {/each}
                    </div>
                  {:else}
                    <div class="display-track-grid">
                      {#each $simulator.tracks as rackTrack}
                        <article class="display-track-card" class:display-track-card-active={rackTrack.id === track.id}>
                          <strong>{rackTrack.name}</strong>
                          <span>{rackTrack.synth}</span>
                          <small>{rackTrack.patch}</small>
                        </article>
                      {/each}
                    </div>
                  {/if}

                  {#if selection}
                    <div class="selection-dock">
                      <div>
                        <p class="selection-label">Selected</p>
                        <strong>{selection.label}</strong>
                        <span>{selection.value}</span>
                      </div>

                      <div class="selection-controls">
                        <button on:click={() => adjustSelection(-1)} disabled={!selection.adjustable}>-</button>
                        <button on:click={() => adjustSelection(1)} disabled={!selection.adjustable}>+</button>
                      </div>
                    </div>
                  {/if}
                {/if}
              </div>
            </div>

            <button class="fn-key" class:fn-key-open={$simulator.fnPopupOpen} on:click={toggleFnPopup}>
              <span>FN</span>
              <small>Popup</small>
            </button>
          </div>

          <div class="matrix-rail">
            {#each matrix as entry, index}
              <button
                class="matrix-key"
                class:matrix-key-selected={selection?.id === entry.id}
                disabled={!entry.id}
                on:click={() => selectMatrixCell(index)}
              >
                <span>{entry.label}</span>
              </button>
            {/each}
          </div>
        </div>

        <div class="keyboard-panel">
          <div class="keyboard-header">
            <div>
              <p class="eyebrow">4x14 Keyboard</p>
              <h3>{track.name} Keybed</h3>
            </div>
            <p class="keyboard-status">Layout: {$simulator.layoutLabel} / Active notes: {track.activeKeys.length}</p>
          </div>

          <div class="keyboard-grid">
            {#each keyboardKeys as key}
              <button
                class="note-key"
                class:note-key-active={track.activeKeys.includes(key.id)}
                on:pointerdown={(event) => handleKeyPointerDown(key, event)}
                on:pointerup={endKeyPointer}
                on:pointercancel={endKeyPointer}
              >
                <span class="note-led"></span>
                <span class="note-main">{key.label}</span>
                <span class="note-sub">{key.note}</span>
              </button>
            {/each}
          </div>
        </div>
      </div>
    </section>

    <aside class="inspector-panel">
      <section class="panel-card">
        <p class="eyebrow">Status</p>
        <h3>Runtime Model</h3>
        <div class="stat-grid">
          <article>
            <span>Active track</span>
            <strong>{track.name}</strong>
          </article>
          <article>
            <span>Overlay</span>
            <strong>{overlayLabels[$simulator.overlay]}</strong>
          </article>
          <article>
            <span>Patch</span>
            <strong>{track.patch}</strong>
          </article>
          <article>
            <span>Last event</span>
            <strong>{track.lastEvent}</strong>
          </article>
        </div>
      </section>

      <section class="panel-card">
        <p class="eyebrow">Interaction</p>
        <h3>Controls</h3>
        <ul class="guide-list">
          <li>Tap T1-T4 to switch tracks and leave edit mode.</li>
          <li>Hold T1-T4 for {HOLD_DELAY_MS}ms to open Tone, Params, Layout, or Utility matrices.</li>
          <li>Use the 3x4 matrix rail to choose a parameter or preset.</li>
          <li>Use the +/- buttons inside the display to change continuous parameters.</li>
          <li>Press FN to preview the system popup that will eventually host settings and launcher actions.</li>
        </ul>
      </section>

      <section class="panel-card">
        <p class="eyebrow">Synth Rack</p>
        <h3>Slots</h3>
        <div class="track-stack">
          {#each $simulator.tracks as rackTrack}
            <article class="track-card" class:track-card-current={rackTrack.id === track.id}>
              <div>
                <strong>{rackTrack.name}</strong>
                <span>{rackTrack.synth} / {rackTrack.port}</span>
              </div>
              <div>
                <small>{rackTrack.patch}</small>
                <small>{rackTrack.activeKeys.length} active</small>
              </div>
            </article>
          {/each}
        </div>
      </section>

      <section class="panel-card panel-log">
        <p class="eyebrow">Input Log</p>
        <h3>Recent Events</h3>
        <div class="event-list">
          {#if reversedLog.length === 0}
            <p class="empty-copy">No simulated events yet.</p>
          {:else}
            {#each reversedLog as entry}
              <article class="event-row">
                <div>
                  <strong>{entry.category}</strong>
                  <span>{entry.detail}</span>
                </div>
                <small>{entry.time}</small>
              </article>
            {/each}
          {/if}
        </div>
      </section>
    </aside>
  </main>
</div>