// The scene map: each scene's pages with the screenshot that shows them and the section that tells them.
// [title, section, [label, section, screenshot][]]
export const SCENE_MAP: [string, string, [string, string, string][]][] = [
  ["0 SETUP", "setup", [
    ["PROJECT", "setup-project", "setup-project"],
    ["FILES", "setup-files", "setup-files"],
    ["KEYS", "setup-keys", "setup-keys"],
    ["SYSTEM", "setup-system", "setup-system"],
  ]],
  ["1 INSTR", "instr", [
    ["VOICE", "instr-voice", "instr-voice"],
    ["CHORD", "play-chords", "instr-chord"],
    ["ARP", "play-arp", "instr-arp"],
    ["DRUMS · KIT", "engine-drums", "instr-drums-kit"],
    ["HERA · DCO VCF ENV LFO", "engine-hera", "instr-hera-dco"],
    ["FM7 · ALGO LEVELS OP1–6 LFO", "engine-fm7", "instr-fm7-algo"],
    ["SAMPLE · SAMPLE TONE", "engine-sampler", "instr-sampler-sample"],
    ["FX", "instr-fx", "instr-hera-fx"],
  ]],
  ["2 SEQ", "seq", [
    ["ROLL", "seq-roll", "seq-notes"],
    ["STEP", "seq-step-rec", "seq-step"],
    ["RECORD", "record-panel", "record"],
    ["PATTERN", "seq-patterns", "seq-pattern"],
    ["TRACK", "seq-track", "seq-track"],
  ]],
  ["3 TAPE", "tape", [
    ["SESSION", "tape-session", "tape-session"],
    ["SONG", "tape-song", "tape-song"],
    ["MIX", "tape-mix", "tape-mix"],
    ["SENDS", "tape-sends", "tape-sends"],
    ["FX", "tape-fx", "tape-fx"],
  ]],
  ["HOLD", "nav-maps", [
    ["ESC", "nav-scenes", "map-esc"],
    ["TAB", "nav-tracks", "map-tab"],
    ["CTRL", "nav-pages", "map-ctrl"],
    ["FN", "fn-menu", "fn-held"],
  ]],
];
