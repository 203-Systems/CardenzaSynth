#pragma once

#include <array>
#include <cstddef>

// The models of the Braids engine (Library/third_party/braids, Mutable Instruments' macro oscillator),
// in the module's order, and what its two macro controls do in each: for the engine's parameter text
// and the OSC page. No DSP here: this header is for the engine and the UI.
namespace synth::braids {

struct ModelInfo {
  const char* name;    // as the screen writes it (uppercase font, 6 characters at most)
  const char* timbre;  // what TIMBRE does
  const char* color;   // what COLOR does
};

inline constexpr std::size_t kModelCount = 47;

inline constexpr std::array<ModelInfo, kModelCount> kModels{{
    {"CSAW", "NOTCH", "DEPTH"},      // CS-80 style saw with a notch
    {"MORPH", "SHAPE", "TONE"},      // triangle > saw > square > pulse, low-pass
    {"SAW SQ", "WIDTH", "MIX"},      // saw and square, pulse width
    {"FOLD", "FOLD", "SIN/TRI"},     // sine / triangle into a wavefolder
    {"BUZZ", "COMB", "DETUNE"},      // sine to a comb of pulses, two of them
    {"SUB SQ", "WIDTH", "SUB"},      // square with a sub oscillator
    {"SUB SW", "SHAPE", "SUB"},      // saw with a sub oscillator
    {"SYN SQ", "RATIO", "MIX"},      // hard-synced square
    {"SYN SW", "RATIO", "MIX"},      // hard-synced saw
    {"SAW X3", "OSC 2", "OSC 3"},    // three saws
    {"SQR X3", "OSC 2", "OSC 3"},    // three squares
    {"TRI X3", "OSC 2", "OSC 3"},    // three triangles
    {"SIN X3", "OSC 2", "OSC 3"},    // three sines
    {"RING", "OSC 2", "OSC 3"},      // three sines ring-modulated
    {"SWARM", "DETUNE", "HIPASS"},   // seven saws
    {"COMB", "DELAY", "FEEDBK"},     // saw through a comb filter
    {"TOY", "RATE", "BITS"},         // circuit-bent toy
    {"ZLPF", "CUTOFF", "SHAPE"},     // waveform that sounds low-pass filtered
    {"ZPKF", "CUTOFF", "SHAPE"},     // peaking
    {"ZBPF", "CUTOFF", "SHAPE"},     // band-pass
    {"ZHPF", "CUTOFF", "SHAPE"},     // high-pass
    {"VOSIM", "FORM 1", "FORM 2"},   // VOSIM formants
    {"VOWEL", "VOWEL", "GENDER"},    // speech synthesis
    {"VFOF", "VOWEL", "GENDER"},     // FOF vowels
    {"HARM", "CENTER", "PEAK"},      // twelve additive harmonics
    {"FM", "INDEX", "RATIO"},        // two-operator FM
    {"FBFM", "INDEX", "RATIO"},      // with feedback
    {"WTFM", "INDEX", "RATIO"},      // chaotic feedback
    {"PLUCK", "DECAY", "POS"},       // plucked string
    {"BOWED", "FRICT", "POS"},       // bowed string
    {"BLOWN", "BREATH", "BORE"},     // reed
    {"FLUTE", "BREATH", "BORE"},     // flute
    {"BELL", "DECAY", "INHARM"},     // struck bell
    {"DRUM", "DECAY", "BRIGHT"},     // struck drum
    {"KICK", "DECAY", "TONE"},       // 808 kick
    {"CYMBAL", "CUTOFF", "NOISE"},   // 808 cymbal
    {"SNARE", "TONE", "SNAPPY"},     // 808 snare
    {"WTBL", "WAVE", "TABLE"},       // wavetables
    {"WMAP", "X", "Y"},              // a map of waves
    {"WLIN", "WAVE", "INTERP"},      // every wave in a line
    {"WT X4", "WAVE", "CHORD"},      // four wavetable voices, a chord
    {"NOISE", "RESO", "LP/HP"},      // filtered noise
    {"TWINQ", "RESO", "SPREAD"},     // noise through two peaks
    {"CLKN", "CYCLE", "QUANT"},      // clocked noise
    {"CLOUD", "DENSE", "SCATTR"},    // granular cloud
    {"PRTCL", "DENSE", "SCATTR"},    // particle noise
    {"QPSK", "RATE", "DATA"},        // digital modulation
}};

inline constexpr const ModelInfo& Model(int model)
{
  return kModels[model < 0 ? 0 : (static_cast<std::size_t>(model) < kModelCount ? static_cast<std::size_t>(model) : kModelCount - 1)];
}

}  // namespace synth::braids
