#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "Library/Synth/Audio/SynthVocal.h"
#include "Library/Synth/Core/Engines/Vocal/VocalPack.h"
#include "Library/Synth/Core/PagedSynthEngine.h"

namespace synth {

// The Vocal engine: a singer. Its parameters are how the voice sounds (level, envelope, glide,
// vibrato, tuning, MONO / POLY: what presets hold), the track's own word, "voice.word": what a note
// sings when its step names none (Library/Synth/Audio/SynthVocal.h), and the track's lyric: up to
// kLyricLength words that keys played by hand sing in turn (the controller sends each one's word key;
// a performance, not a pattern). The lyric starts empty and is kept packed: its words are the places
// "lyric.01".. up to the first empty one (-1), and only the lyric methods below change them. Words and
// lyric belong to the track (saved with the project), not to presets. The voice pack is the singer: the built-in one
// until the app hands it another (a .vocal file).
inline constexpr std::size_t kLyricLength = 16;

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

  // The lyric: its places' parameter ids, and which place an id is (-1: not one).
  static std::string LyricSlotId(std::size_t place);
  static int LyricSlotOf(std::string_view parameterId);
  // How many words it has (0: none, the keys sing the track's word).
  std::size_t LyricLength() const;
  // The word at a place, -1 past its end.
  int LyricWord(std::size_t place) const;
  // A word added at its end (false: it is full), one changed, one taken out (the later ones move up),
  // all taken out. A word is a place in the pack's list.
  bool AppendLyricWord(int word);
  bool SetLyricWord(std::size_t place, int word);
  bool RemoveLyricWord(std::size_t place);
  bool ClearLyric();

  // The word steps through the pack's pronunciations and stops at its last.
  bool AdjustParameterValue(const std::string& parameterId, int delta) override;

protected:
  std::string FormatParameterValue(const SynthParameterInfo& parameter) const override;
  bool ShouldSyncParameterToRuntime(const SynthParameterInfo& parameter) const override;
  bool ShouldSyncParameterOnPresetLoad(const SynthParameterInfo& parameter) const override;
  bool PresetCarries(const SynthParameterInfo& parameter) const override;
  void OnRuntimeSynced() override;

private:
  vocal::PackRef pack_;
  std::string packFileName_;
};

}  // namespace synth
