#include "Applications/Synth/UI/Groove/InstrumentPages.h"

namespace groove {

namespace {

constexpr Slot P(const char* id, const char* label) { return Slot{id, label, SlotAction::None}; }
constexpr Slot A(SlotAction action, const char* label) { return Slot{nullptr, label, action}; }
constexpr Slot E() { return Slot{}; }

constexpr InstrumentPage kVoicePage{
    "PRESET", "VOICE",
    {{A(SlotAction::Engine, "ENGINE"), A(SlotAction::Preset, "PRESET"), A(SlotAction::SavePreset, "SAVE"), A(SlotAction::SavePresetAs, "SAVE AS"),
      E(), E(), E(), E(), E(), E(), E(), E(), E(), E(), E(), E()}},
    PageGraphic::Voice, {1, 0, 3, 4}, 0};

constexpr InstrumentPage kKeysPage{
    "LAYOUT", "KEYS",
    {{P("keys.layout", "LAYOUT"), P("keys.scale", "SCALE"), P("keys.root", "ROOT"), P("keys.octave", "OCTAVE"),
      E(), E(), E(), E(), E(), E(), E(), E(), E(), E(), E(), E()}},
    PageGraphic::KeyMap, {1, 0, 3, 4}, 0};

// Signal order left to right: drive, crush, filter, then chorus and the two sends.
constexpr InstrumentPage kFxPage{
    "INSERTS", "FX",
    {{P("fx.drive", "DRIVE"), P("fx.crush", "CRUSH"), P("fx.filter", "FILTER"), P("fx.resonance", "RESO"),
      P("fx.chorus", "CHORUS"), P("fx.delay", "DELAY"), P("fx.reverb", "REVERB"), E(),
      E(), E(), E(), E(), E(), E(), E(), E()}},
    PageGraphic::FxChain, {2, 0, 2, 4}, 0};

// One page per DX7 operator: tuning on top, the envelope graph beside MODE / AMOD, then the four
// rates over the four levels.
#define CARDENZA_DX7_OP_PAGE(n)                                                                                      \
  InstrumentPage{"TUNE EG", "OP" #n,                                                                            \
                 {{P("op" #n ".output", "OUT"), P("op" #n ".coarse", "CRS"), P("op" #n ".fine", "FINE"),            \
                   P("op" #n ".detune", "DTN"), P("op" #n ".mode", "MODE"), P("op" #n ".amp_mod", "AMOD"), E(), E(), \
                   P("op" #n ".attack", "R1"), P("op" #n ".decay", "R2"), P("op" #n ".rate3", "R3"),                \
                   P("op" #n ".release", "R4"), P("op" #n ".level1", "L1"), P("op" #n ".level2", "L2"),             \
                   P("op" #n ".sustain", "L3"), P("op" #n ".level4", "L4")}},                                        \
                 PageGraphic::OperatorEnvelope, {1, 2, 1, 2}, n}

constexpr std::array<InstrumentPage, 12> kDx7Pages{{
    kVoicePage,
    InstrumentPage{"ROUTING", "ALGO",
                   {{P("global.algorithm", "ALG"), P("global.feedback", "FB"), P("global.transpose", "TRNS"), E(),
                     E(), E(), E(), E(), E(), E(), E(), E(), E(), E(), E(), E()}},
                   PageGraphic::Algorithm, {1, 0, 3, 4}, 0},
    InstrumentPage{"OP MIX", "LEVELS",
                   {{P("op1.output", "OP1"), P("op2.output", "OP2"), P("op3.output", "OP3"), P("op4.output", "OP4"),
                     P("op5.output", "OP5"), P("op6.output", "OP6"), E(), E(),
                     E(), E(), E(), E(), E(), E(), E(), E()}},
                   PageGraphic::OperatorLevels, {2, 0, 2, 4}, 0},
    CARDENZA_DX7_OP_PAGE(1),
    CARDENZA_DX7_OP_PAGE(2),
    CARDENZA_DX7_OP_PAGE(3),
    CARDENZA_DX7_OP_PAGE(4),
    CARDENZA_DX7_OP_PAGE(5),
    CARDENZA_DX7_OP_PAGE(6),
    InstrumentPage{"VIBRATO", "LFO",
                   {{P("global.lfo_speed", "SPEED"), P("global.lfo_wave", "WAVE"), P("global.lfo_pitch", "PMD"), P("global.lfo_amp", "AMD"),
                     P("global.lfo_pms", "P SENS"), E(), E(), E(),
                     E(), E(), E(), E(), E(), E(), E(), E()}},
                   PageGraphic::Lfo, {2, 0, 2, 4}, 0},
    kFxPage,
    kKeysPage,
}};

#undef CARDENZA_DX7_OP_PAGE

constexpr std::array<InstrumentPage, 7> kJunoPages{{
    kVoicePage,
    InstrumentPage{"WAVES", "DCO",
                   {{P("dco.range", "RANGE"), P("dco.saw", "SAW"), P("dco.pulse", "PULSE"), P("dco.pwm", "PWM"),
                     P("dco.pwm_mode", "PW SRC"), P("dco.sub", "SUB"), P("dco.noise", "NOISE"), P("dco.lfo", "LFO"),
                     E(), E(), E(), E(), E(), E(), E(), E()}},
                   PageGraphic::Oscillator, {2, 0, 2, 4}, 0},
    InstrumentPage{"FILTER", "VCF",
                   {{P("vcf.cutoff", "CUTOFF"), P("vcf.resonance", "RESO"), P("vcf.env", "ENV"), P("vcf.polarity", "POL"),
                     P("vcf.lfo", "LFO"), P("vcf.keyboard", "KBD"), E(), E(),
                     E(), E(), E(), E(), E(), E(), E(), E()}},
                   PageGraphic::Filter, {2, 0, 2, 4}, 0},
    InstrumentPage{"ADSR", "ENV",
                   {{P("env.attack", "ATTACK"), P("env.decay", "DECAY"), P("env.sustain", "SUSTAIN"), P("env.release", "RELEASE"),
                     P("vca.mode", "VCA"), P("vca.level", "LEVEL"), E(), E(),
                     E(), E(), E(), E(), E(), E(), E(), E()}},
                   PageGraphic::Adsr, {2, 0, 2, 4}, 0},
    InstrumentPage{"MOD", "LFO",
                   {{P("lfo.rate", "RATE"), P("lfo.delay", "DELAY"), E(), E(),
                     E(), E(), E(), E(), E(), E(), E(), E(), E(), E(), E(), E()}},
                   PageGraphic::Lfo, {1, 0, 3, 4}, 0},
    kFxPage,
    kKeysPage,
}};

constexpr std::array<InstrumentPage, 5> kSamplerPages{{
    kVoicePage,
    InstrumentPage{"WAV", "SAMPLE",
                   {{A(SlotAction::SampleFile, "FILE"), A(SlotAction::SampleRecord, "REC"), P("sample.start", "START"), P("sample.end", "END"),
                     P("sample.root", "ROOT"), P("sample.tune", "TUNE"), P("sample.loop", "LOOP"), P("sample.gain", "GAIN"),
                     E(), E(), E(), E(), E(), E(), E(), E()}},
                   PageGraphic::Waveform, {2, 0, 2, 4}, 0},
    InstrumentPage{"TONE", "TONE",
                   {{P("filter.cutoff", "CUTOFF"), P("filter.resonance", "RESO"), P("env.attack", "ATTACK"), P("env.release", "RELEASE"),
                     P("amp.level", "LEVEL"), E(), E(), E(), E(), E(), E(), E(), E(), E(), E(), E()}},
                   PageGraphic::Filter, {2, 0, 2, 4}, 0},
    kFxPage,
    kKeysPage,
}};

constexpr std::array<InstrumentPage, 3> kDrumPages{{
    kVoicePage,
    InstrumentPage{"LEVELS", "KIT",
                   {{P("kick.level", "BD"), P("snare.level", "SD"), P("closed_hat.level", "CH"), P("open_hat.level", "OH"),
                     P("clap.level", "CP"), P("tom.level", "TM"), P("cowbell.level", "CB"), P("maraca.level", "MA"),
                     P("master.level", "MASTER"), E(), E(), E(), E(), E(), E(), E()}},
                   PageGraphic::DrumPads, {2, 1, 2, 3}, 0},
    kFxPage,
}};

}  // namespace

InstrumentPageList InstrumentPagesFor(synth::SynthEngineKind kind)
{
  switch (kind)
  {
    case synth::SynthEngineKind::Dx7: return {kDx7Pages.data(), kDx7Pages.size()};
    case synth::SynthEngineKind::Juno: return {kJunoPages.data(), kJunoPages.size()};
    case synth::SynthEngineKind::Sampler: return {kSamplerPages.data(), kSamplerPages.size()};
    case synth::SynthEngineKind::Drums: return {kDrumPages.data(), kDrumPages.size()};
    case synth::SynthEngineKind::None:
    default: return {&kVoicePage, 1};
  }
}

}  // namespace groove
