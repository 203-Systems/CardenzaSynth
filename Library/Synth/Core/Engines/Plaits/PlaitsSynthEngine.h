#pragma once

#include <array>
#include <cstddef>

#include "Library/Synth/Core/PagedSynthEngine.h"

namespace synth {

// The Plaits macro oscillator (Emilie Gillet's Eurorack module, Library/third_party/eurorack): one
// oscillator model per track, shaped by HARMONICS, TIMBRE and MORPH, through its low-pass gate.
// The models in the order plaits::Voice registers them, which is what "osc.model" counts in: the
// eight of firmware 1.2 first, then the sixteen original ones.
inline constexpr std::size_t kPlaitsModelCount = 24;
inline constexpr std::array<const char*, kPlaitsModelCount> kPlaitsModelNames{{
    "VA VCF", "PHASE", "6OP A", "6OP B", "6OP C", "TERRAIN", "ENSEMBLE", "CHIP",
    "ANALOG", "SHAPER", "FM", "GRAIN", "ADDITIVE", "WTABLE", "CHORD", "SPEECH",
    "SWARM", "NOISE", "PARTICLE", "STRING", "MODAL", "KICK", "SNARE", "HIHAT"}};
// What a model is, for the screen (the page's graphic).
inline constexpr std::array<const char*, kPlaitsModelCount> kPlaitsModelDetails{{
    "ANALOG WAVES + FILTER", "PHASE DISTORTION", "6-OP FM BANK A", "6-OP FM BANK B",
    "6-OP FM BANK C", "WAVE TERRAIN", "STRING MACHINE", "CHIPTUNE CHORDS",
    "PAIR OF ANALOG WAVES", "WAVESHAPER + FOLDER", "2-OPERATOR FM", "GRANULAR FORMANTS",
    "HARMONIC OSCILLATOR", "WAVETABLE", "CHORDS", "VOWELS AND SPEECH",
    "GRANULAR CLOUD", "FILTERED NOISE", "PARTICLE NOISE", "INHARMONIC STRING",
    "MODAL RESONATOR", "ANALOG BASS DRUM", "ANALOG SNARE DRUM", "ANALOG HI-HAT"}};

// "lpg.mode": what opens the low-pass gate.
inline constexpr int kPlaitsEnvelopeGate = 0;  // open while the key is down, DECAY is the release
inline constexpr int kPlaitsEnvelopePing = 1;  // struck at the note's start, DECAY is its length

class PlaitsSynthEngine final : public PagedSynthEngine {
public:
  PlaitsSynthEngine();

protected:
  std::string FormatParameterValue(const SynthParameterInfo& parameter) const override;
  // The audio runtime does not know the presets: every value is sent with one.
  bool ShouldSyncParameterOnPresetLoad(const SynthParameterInfo&) const override { return true; }
};

}  // namespace synth
