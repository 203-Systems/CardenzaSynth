#pragma once

#include <string>

#include "Library/Synth/Audio/SynthVocal.h"
#include "Library/Synth/Core/Engines/Vocal/VocalPack.h"
#include "Library/Synth/Core/PagedSynthEngine.h"

namespace synth {

// The Vocal engine: a singer. Its parameters are how the voice sounds (level, envelope, glide,
// vibrato, tuning, MONO / POLY: what presets hold) and the track's own word, "voice.word": what a
// note sings when its step names none (Library/Synth/Audio/SynthVocal.h). The voice pack is the
// singer: the built-in one until the app hands it another (a .vocal file).
class VocalSynthEngine final : public PagedSynthEngine {
public:
  VocalSynthEngine();

  // The engine of a vocal track as what it is (the firmware is built without RTTI).
  static VocalSynthEngine* From(ISynthEngine* engine)
  {
    return engine != nullptr && engine->Descriptor().kind == SynthEngineKind::Vocal ? static_cast<VocalSynthEngine*>(engine) : nullptr;
  }
  static const VocalSynthEngine* From(const ISynthEngine* engine)
  {
    return engine != nullptr && engine->Descriptor().kind == SynthEngineKind::Vocal ? static_cast<const VocalSynthEngine*>(engine) : nullptr;
  }

  // The singer. `fileName` is how a project names the pack ("" for the built-in one). A word is a
  // place in the pack's list: the track's own word stays where it is when the new pack has that many
  // (a project's word is its pack's), and goes to the new pack's default when it has not.
  void SetPack(vocal::PackRef pack, std::string fileName);
  const vocal::Pack& Pack() const { return *pack_; }
  // The same pack for another track that sings from the same file.
  const vocal::PackRef& SharedPack() const { return pack_; }
  const std::string& PackFileName() const { return packFileName_; }
  bool BuiltinPack() const { return packFileName_.empty(); }

  // The track's own word (a place in the pack's list), and any word's name ("?" for one the pack does
  // not have).
  int Word() const;
  void SetWord(int word);
  std::string WordName(int word) const;

  // The word steps through the pack's pronunciations and stops at its last.
  bool AdjustParameterValue(const std::string& parameterId, int delta) override;

protected:
  std::string FormatParameterValue(const SynthParameterInfo& parameter) const override;
  bool ShouldSyncParameterOnPresetLoad(const SynthParameterInfo& parameter) const override;
  bool PresetCarries(const SynthParameterInfo& parameter) const override;
  void OnRuntimeSynced() override;

private:
  vocal::PackRef pack_;
  std::string packFileName_;
};

}  // namespace synth
