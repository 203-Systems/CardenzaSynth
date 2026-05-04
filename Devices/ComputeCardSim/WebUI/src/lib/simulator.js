import { writable } from 'svelte/store'

const HOLD_DELAY_MS = 360
const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
const EMPTY_MATRIX = Array.from({ length: 12 }, () => ({ label: '--', id: '', kind: 'empty' }))
const FN_ACTIONS = [
  { label: 'Settings', caption: 'Global settings and shortcuts' },
  { label: 'Launcher', caption: 'Switch apps and sessions' },
  { label: 'Audio', caption: 'Mix, monitoring, and output' },
  { label: 'Routing', caption: 'USB, wireless, and synth ports' },
]

function cell(label, id, kind = 'continuous') {
  return { label, id, kind }
}

const toneMatrices = {
  DX7: [
    cell('EPiano', 'preset.epiano', 'choice'),
    cell('Clavi', 'preset.clavi', 'choice'),
    cell('Bass1', 'preset.bass1', 'choice'),
    cell('Brass', 'preset.brass', 'choice'),
    cell('Bell', 'preset.bell', 'choice'),
    cell('Pad', 'preset.pad', 'choice'),
    cell('Keys', 'preset.keys', 'choice'),
    cell('Lead', 'preset.lead', 'choice'),
    cell('Organ', 'preset.organ', 'choice'),
    cell('Marimba', 'preset.marimba', 'choice'),
    cell('Vox', 'preset.vox', 'choice'),
    cell('Init', 'preset.init', 'choice'),
  ],
  Juno: [
    cell('Saw', 'preset.saw', 'choice'),
    cell('PWM', 'preset.pwm', 'choice'),
    cell('Sub', 'preset.sub', 'choice'),
    cell('Hoover', 'preset.hoover', 'choice'),
    cell('Strings', 'preset.strings', 'choice'),
    cell('Brass', 'preset.brass', 'choice'),
    cell('Bass', 'preset.bass', 'choice'),
    cell('Pad', 'preset.pad', 'choice'),
    cell('Pluck', 'preset.pluck', 'choice'),
    cell('Noise', 'preset.noise', 'choice'),
    cell('Arp', 'preset.arp', 'choice'),
    cell('Init', 'preset.init', 'choice'),
  ],
  Sampler: [
    cell('Kick', 'preset.kick', 'choice'),
    cell('Snare', 'preset.snare', 'choice'),
    cell('Hat', 'preset.hat', 'choice'),
    cell('Chord', 'preset.chord', 'choice'),
    cell('Bass', 'preset.bass', 'choice'),
    cell('Vox', 'preset.vox', 'choice'),
    cell('OneShot', 'preset.oneshot', 'choice'),
    cell('Pad', 'preset.pad', 'choice'),
    cell('LoopA', 'preset.loopA', 'choice'),
    cell('LoopB', 'preset.loopB', 'choice'),
    cell('Slice', 'preset.slice', 'choice'),
    cell('Init', 'preset.init', 'choice'),
  ],
}

const parameterMatrices = {
  DX7: [
    cell('Algo', 'algo'),
    cell('FB', 'feedback'),
    cell('Op1Atk', 'op1.attack'),
    cell('Op1Rel', 'op1.release'),
    cell('Op2Atk', 'op2.attack'),
    cell('Op2Rel', 'op2.release'),
    cell('Cutoff', 'filter.cutoff'),
    cell('Res', 'filter.resonance'),
    cell('LfoRt', 'lfo.rate'),
    cell('LfoDp', 'lfo.depth'),
    cell('Level', 'amp.level'),
    cell('Pan', 'amp.pan'),
  ],
  Juno: [
    cell('Dco', 'dco.range'),
    cell('PWM', 'dco.pwm'),
    cell('Sub', 'dco.sub'),
    cell('Noise', 'dco.noise'),
    cell('Cutoff', 'vcf.cutoff'),
    cell('Res', 'vcf.resonance'),
    cell('Env', 'vcf.env'),
    cell('Lfo', 'vcf.lfo'),
    cell('Atk', 'env.attack'),
    cell('Rel', 'env.release'),
    cell('Chorus', 'fx.chorus'),
    cell('Level', 'amp.level'),
  ],
  Sampler: [
    cell('Start', 'sample.start'),
    cell('End', 'sample.end'),
    cell('Loop', 'sample.loop'),
    cell('Tune', 'sample.tune'),
    cell('Atk', 'env.attack'),
    cell('Rel', 'env.release'),
    cell('Cutoff', 'filter.cutoff'),
    cell('Res', 'filter.resonance'),
    cell('Drive', 'fx.drive'),
    cell('Send', 'fx.send'),
    cell('Level', 'amp.level'),
    cell('Pan', 'amp.pan'),
  ],
}

const layoutMatrix = [
  cell('Piano', 'layout.piano', 'choice'),
  cell('Chrom', 'layout.chromatic', 'choice'),
  cell('Drum', 'layout.drum', 'choice'),
  cell('Scale', 'layout.scale', 'choice'),
  cell('Split', 'layout.split', 'choice'),
  cell('Latch', 'layout.latch', 'choice'),
  cell('Mono', 'layout.mono', 'choice'),
  cell('Arp', 'layout.arp', 'choice'),
  cell('Oct-', 'layout.octaveDown', 'choice'),
  cell('Oct+', 'layout.octaveUp', 'choice'),
  cell('Vel', 'layout.velocity', 'choice'),
  cell('After', 'layout.aftertouch', 'choice'),
]

const utilityMatrix = [
  cell('Level', 'mix.level'),
  cell('Pan', 'mix.pan'),
  cell('Send', 'mix.send'),
  cell('Porta', 'play.portamento'),
  cell('Chorus', 'fx.chorus'),
  cell('Delay', 'fx.delay'),
  cell('Reverb', 'fx.reverb'),
  cell('Clock', 'sync.clock'),
  cell('USB', 'route.usb', 'toggle'),
  cell('BT', 'route.bt', 'toggle'),
  cell('WiFi', 'route.wifi', 'toggle'),
  cell('Save', 'preset.save', 'action'),
]

const overlayLabels = {
  none: 'Track View',
  tone: 'Tone Matrix',
  parameter: 'Parameter Matrix',
  layout: 'Keyboard Layout Matrix',
  other: 'Utility Matrix',
}

function overlayForButton(index) {
  if (index === 0) return 'tone'
  if (index === 1) return 'parameter'
  if (index === 2) return 'layout'
  if (index === 3) return 'other'
  return 'none'
}

function formatTimestamp() {
  const now = new Date()
  return now.toLocaleTimeString('en-US', {
    hour12: false,
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  })
}

function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value))
}

function createParameterValues(synth) {
  const values = {}
  for (const entry of [...parameterMatrices[synth], ...utilityMatrix]) {
    if (!entry.id) {
      continue
    }
    if (entry.kind === 'toggle') {
      values[entry.id] = entry.id === 'route.usb' ? 1 : 0
    } else if (entry.kind === 'action') {
      values[entry.id] = 0
    } else if (entry.id === 'mix.level' || entry.id === 'amp.level') {
      values[entry.id] = 96
    } else if (entry.id === 'mix.pan' || entry.id === 'amp.pan') {
      values[entry.id] = 64
    } else {
      values[entry.id] = 64
    }
  }
  return values
}

function createTrack(index, synth, patch) {
  return {
    id: index + 1,
    name: `Track ${index + 1}`,
    synth,
    port: `0x${(0x8000 + index).toString(16).toUpperCase()}`,
    patch,
    lastEvent: 'idle',
    activeKeys: [],
    parameterValues: createParameterValues(synth),
  }
}

function createInitialState() {
  return {
    holdDelayMs: HOLD_DELAY_MS,
    fnPopupOpen: false,
    activeTrack: 0,
    overlay: 'none',
    editMode: false,
    selectedCellId: '',
    selectedCellLabel: '',
    keyboardLayout: 'layout.piano',
    layoutLabel: 'Piano',
    eventCounter: 0,
    inputLog: [],
    tracks: [
      createTrack(0, 'DX7', 'EPiano'),
      createTrack(1, 'Juno', 'Saw'),
      createTrack(2, 'Sampler', 'Kick'),
      createTrack(3, 'DX7', 'Bell'),
    ],
  }
}

function cloneState(state) {
  return {
    ...state,
    tracks: state.tracks.map((track) => ({
      ...track,
      activeKeys: [...track.activeKeys],
      parameterValues: { ...track.parameterValues },
    })),
    inputLog: [...state.inputLog],
  }
}

function updateState(mutator) {
  simulator.update((state) => {
    const next = cloneState(state)
    mutator(next)
    return next
  })
}

function pushEvent(state, category, detail) {
  state.eventCounter += 1
  state.inputLog = [
    ...state.inputLog.slice(-79),
    {
      id: state.eventCounter,
      time: formatTimestamp(),
      category,
      detail,
    },
  ]
}

function noteName(midi) {
  const octave = Math.floor(midi / 12) - 1
  return `${NOTE_NAMES[midi % 12]}${octave}`
}

function buildKeyboardLabel(layoutId, index, midi) {
  if (layoutId === 'layout.chromatic') {
    return { label: NOTE_NAMES[midi % 12], caption: `Step ${index + 1}` }
  }

  if (layoutId === 'layout.drum') {
    const drumNames = ['Kick', 'Snr', 'Hat', 'Tom', 'Clp', 'Rim', 'Shk', 'Perc', 'Bass', 'Vox', 'FX', 'Loop', 'Hit', 'Sub']
    return { label: drumNames[index % drumNames.length], caption: `Pad ${index + 1}` }
  }

  if (layoutId === 'layout.scale') {
    const degrees = ['1', '2', '3', '4', '5', '6', '7']
    return { label: `Deg ${degrees[index % degrees.length]}`, caption: noteName(midi) }
  }

  if (layoutId === 'layout.split') {
    return { label: index < 28 ? 'Bass' : 'Lead', caption: noteName(midi) }
  }

  if (layoutId === 'layout.mono') {
    return { label: 'Mono', caption: noteName(midi) }
  }

  if (layoutId === 'layout.arp') {
    return { label: 'Arp', caption: noteName(midi) }
  }

  return { label: noteName(midi), caption: `Key ${index + 1}` }
}

function resolveCellForSelection(state) {
  const matrix = getCurrentMatrix(state)
  return matrix.find((entry) => entry.id === state.selectedCellId) || null
}

function formatParameterValue(id, value) {
  if (id.startsWith('route.')) {
    return value ? 'On' : 'Off'
  }

  if (id === 'mix.pan' || id === 'amp.pan') {
    if (value === 64) {
      return 'Center'
    }
    if (value < 64) {
      return `L${64 - value}`
    }
    return `R${value - 64}`
  }

  if (id === 'sync.clock') {
    return `${40 + value} BPM`
  }

  return `${value}`
}

function selectionInfo(state) {
  if (!state.editMode || !state.selectedCellId) {
    return null
  }

  const track = state.tracks[state.activeTrack]
  const cellInfo = resolveCellForSelection(state)
  if (!cellInfo) {
    return null
  }

  if (state.overlay === 'tone') {
    return {
      id: cellInfo.id,
      label: cellInfo.label,
      value: track.patch,
      helper: `Applied to ${track.name}`,
      adjustable: false,
    }
  }

  if (state.overlay === 'layout') {
    return {
      id: cellInfo.id,
      label: cellInfo.label,
      value: state.layoutLabel,
      helper: 'Keyboard mapping switched for the browser simulator',
      adjustable: false,
    }
  }

  if (cellInfo.kind === 'action') {
    return {
      id: cellInfo.id,
      label: cellInfo.label,
      value: 'Triggered',
      helper: 'Snapshot action fired for the active track',
      adjustable: false,
    }
  }

  const value = track.parameterValues[cellInfo.id] ?? 64
  return {
    id: cellInfo.id,
    label: cellInfo.label,
    value: formatParameterValue(cellInfo.id, value),
    helper: cellInfo.kind === 'toggle' ? 'Tap +/- to toggle routing' : 'Tap +/- to edit the current parameter',
    adjustable: cellInfo.kind !== 'choice',
  }
}

export const simulator = writable(createInitialState())
export { FN_ACTIONS, overlayLabels, HOLD_DELAY_MS }

export function getCurrentMatrix(state) {
  const track = state.tracks[state.activeTrack]
  if (!track) {
    return EMPTY_MATRIX
  }

  if (state.overlay === 'tone') {
    return toneMatrices[track.synth] || EMPTY_MATRIX
  }

  if (state.overlay === 'parameter') {
    return parameterMatrices[track.synth] || EMPTY_MATRIX
  }

  if (state.overlay === 'layout') {
    return layoutMatrix
  }

  if (state.overlay === 'other') {
    return utilityMatrix
  }

  return EMPTY_MATRIX
}

export function getDisplayTitle(state) {
  const track = state.tracks[state.activeTrack]
  if (!track) {
    return 'No Track'
  }

  if (state.fnPopupOpen) {
    return 'FN Popup'
  }

  if (state.editMode && state.selectedCellLabel) {
    return 'Parameter Edit'
  }

  if (state.overlay === 'none') {
    return `${track.name} / ${track.synth}`
  }

  return `${track.synth} ${overlayLabels[state.overlay]}`
}

export function getDisplaySubtitle(state) {
  const track = state.tracks[state.activeTrack]
  if (!track) {
    return ''
  }

  if (state.fnPopupOpen) {
    return 'Settings, launcher, audio, and routing shortcuts.'
  }

  const selection = selectionInfo(state)
  if (selection) {
    return `${selection.value}  |  ${selection.helper}`
  }

  if (state.overlay === 'none') {
    return `${track.patch}  |  ${track.port}  |  ${track.activeKeys.length} active note${track.activeKeys.length === 1 ? '' : 's'}`
  }

  if (state.overlay === 'layout') {
    return `${state.layoutLabel} layout active. Hold another track key to switch matrices.`
  }

  return 'Hold a track key to change matrix. Tap a track key to return to track view.'
}

export function getSelectionInfo(state) {
  return selectionInfo(state)
}

export function getKeyboardKeys(state) {
  return Array.from({ length: 56 }, (_, index) => {
    const midi = 48 + index
    const row = Math.floor(index / 14)
    const column = index % 14
    const label = buildKeyboardLabel(state.keyboardLayout, index, midi)
    return {
      id: index,
      row,
      column,
      midi,
      note: noteName(midi),
      label: label.label,
      caption: label.caption,
    }
  })
}

export function tapTrack(trackIndex) {
  updateState((state) => {
    if (trackIndex < 0 || trackIndex >= state.tracks.length) {
      return
    }

    state.activeTrack = trackIndex
    state.overlay = 'none'
    state.editMode = false
    state.selectedCellId = ''
    state.selectedCellLabel = ''
    state.fnPopupOpen = false
    pushEvent(state, 'Track', `Tap T${trackIndex + 1} -> ${state.tracks[trackIndex].synth}`)
  })
}

export function holdTrack(trackIndex) {
  updateState((state) => {
    if (trackIndex < 0 || trackIndex >= state.tracks.length) {
      return
    }

    state.activeTrack = trackIndex
    state.overlay = overlayForButton(trackIndex)
    state.editMode = false
    state.selectedCellId = ''
    state.selectedCellLabel = ''
    state.fnPopupOpen = false
    pushEvent(state, 'Overlay', `Hold T${trackIndex + 1} -> ${overlayLabels[state.overlay]}`)
  })
}

export function toggleFnPopup() {
  updateState((state) => {
    state.fnPopupOpen = !state.fnPopupOpen
    pushEvent(state, 'FN', state.fnPopupOpen ? 'Opened system popup' : 'Closed system popup')
  })
}

export function closeFnPopup() {
  updateState((state) => {
    if (!state.fnPopupOpen) {
      return
    }
    state.fnPopupOpen = false
    pushEvent(state, 'FN', 'Closed system popup')
  })
}

export function selectMatrixCell(cellIndex) {
  updateState((state) => {
    const matrix = getCurrentMatrix(state)
    const cellInfo = matrix[cellIndex]
    if (!cellInfo || !cellInfo.id) {
      return
    }

    const track = state.tracks[state.activeTrack]
    state.editMode = true
    state.selectedCellId = cellInfo.id
    state.selectedCellLabel = cellInfo.label
    state.fnPopupOpen = false

    if (state.overlay === 'tone') {
      track.patch = cellInfo.label
      track.lastEvent = `Tone -> ${cellInfo.label}`
    } else if (state.overlay === 'layout') {
      state.keyboardLayout = cellInfo.id
      state.layoutLabel = cellInfo.label
      track.lastEvent = `Layout -> ${cellInfo.label}`
    } else if (cellInfo.kind === 'toggle') {
      track.parameterValues[cellInfo.id] = track.parameterValues[cellInfo.id] ? 0 : 1
      track.lastEvent = `${cellInfo.label} -> ${track.parameterValues[cellInfo.id] ? 'On' : 'Off'}`
    } else if (cellInfo.kind === 'action') {
      track.lastEvent = `Saved ${track.patch}`
    } else {
      track.lastEvent = `Edit ${cellInfo.label}`
    }

    pushEvent(state, 'Matrix', `Select ${cellInfo.label} on ${track.name}`)
  })
}

export function adjustSelection(delta) {
  updateState((state) => {
    if (!state.editMode || !state.selectedCellId) {
      return
    }

    const track = state.tracks[state.activeTrack]
    const cellInfo = resolveCellForSelection(state)
    if (!cellInfo || state.overlay === 'tone' || state.overlay === 'layout' || cellInfo.kind === 'action') {
      return
    }

    const current = track.parameterValues[cellInfo.id] ?? 64
    const next = cellInfo.kind === 'toggle'
      ? (current ? 0 : 1)
      : clamp(current + (delta > 0 ? 4 : -4), 0, 127)

    track.parameterValues[cellInfo.id] = next
    track.lastEvent = `${cellInfo.label} -> ${formatParameterValue(cellInfo.id, next)}`
    pushEvent(state, 'Adjust', `${cellInfo.label} -> ${formatParameterValue(cellInfo.id, next)}`)
  })
}

export function pressKeyboardKey(keyIndex) {
  updateState((state) => {
    const track = state.tracks[state.activeTrack]
    if (track.activeKeys.includes(keyIndex)) {
      return
    }

    track.activeKeys.push(keyIndex)
    const keyInfo = getKeyboardKeys(state)[keyIndex]
    track.lastEvent = `Note on ${keyInfo.note}`
    pushEvent(state, 'Key', `Note on ${keyInfo.note} -> ${track.name}`)
  })
}

export function releaseKeyboardKey(keyIndex) {
  updateState((state) => {
    const track = state.tracks[state.activeTrack]
    const nextKeys = track.activeKeys.filter((entry) => entry !== keyIndex)
    if (nextKeys.length === track.activeKeys.length) {
      return
    }

    track.activeKeys = nextKeys
    const keyInfo = getKeyboardKeys(state)[keyIndex]
    track.lastEvent = `Note off ${keyInfo.note}`
    pushEvent(state, 'Key', `Note off ${keyInfo.note} -> ${track.name}`)
  })
}

export function resetSimulator() {
  simulator.set(createInitialState())
}