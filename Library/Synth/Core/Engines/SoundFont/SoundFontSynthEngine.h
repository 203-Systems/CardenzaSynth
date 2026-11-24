#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "Library/Synth/Audio/SoundFont/Sf2Program.h"
#include "Library/Synth/Core/PagedSynthEngine.h"

namespace synth {

namespace soundfont {

// One preset of a SoundFont file, as the engine lists it.
struct Patch {
  std::uint16_t bank = 0;
  std::uint16_t program = 0;
  char name[21] = {};
};

// A SoundFont file the app opened for a track (Applications/Synth/Runtime/SoundFont): its patches in
// bank and program order, and the way to read one into memory. The engine never touches the card
// itself.
class Font {
public:
  virtual ~Font() = default;
  virtual std::size_t PatchCount() const = 0;
  virtual const Patch& PatchAt(std::size_t index) const = 0;
  // Reads a patch's regions and samples, at most `budgetBytes` of them. Null with what to tell the
  // user in `message` (TOO BIG, SD CARD ERROR, ...) when it cannot.
  virtual std::shared_ptr<const audio::sf2::Program> Load(std::size_t index, std::size_t budgetBytes, std::string& message) = 0;

  // The same away from the caller's thread, where the device has a task for it: for `owner` (the
  // track), a request of the same owner not started yet is replaced. The ticket to take the result
  // with (TakeLoaded); 0 where nothing reads in the background (the simulator): the caller reads the
  // patch with Load.
  virtual std::uint32_t LoadLater(std::size_t owner, std::size_t index, std::size_t budgetBytes)
  {
    (void)owner;
    (void)index;
    (void)budgetBytes;
    return 0;
  }
  // What was read for `ticket`, once it is there: true, with the patch, or null and why in `message`.
  virtual bool TakeLoaded(std::size_t owner, std::uint32_t ticket, std::shared_ptr<const audio::sf2::Program>& program, std::string& message)
  {
    (void)owner;
    (void)ticket;
    (void)program;
    (void)message;
    return false;
  }
  // Drop just this request, including a result already read. Its track may have changed hands.
  virtual void CancelLoad(std::uint32_t ticket) { (void)ticket; }
};

// What a track's patch may take of the PSRAM, and what it leaves there for patterns and the other
// engines (docs/sf2-engine-design.md, decision 2).
inline constexpr std::size_t kTrackBudgetBytes = 512 * 1024;
inline constexpr std::size_t kSlowReserveBytes = 512 * 1024;

}  // namespace soundfont

// The SoundFont engine: a track plays one patch of an .sf2 file on the card. The file's patches are
// the engine's presets ("sf.<bank>.<program>", listed with their numbers so they sort as the file
// has them); the parameters are what the track adds to the patch (the TONE page: filter, envelope,
// tuning, level) and the track effects. Without a file the track is silent on the preset "sf.none".
// A patch is read from the card when it is chosen: where the device can, away from the UI (the patch
// before plays on until the new one is there, and comes back when it cannot be read), else on the
// thread that chose it; a restored state (a project, the autosave) is read there and then. The engine
// lets go of the patch it replaces at once: the audio keeps its own reference while its notes fade,
// and hands it back to the UI thread to be freed (audio::CollectSoundFontPrograms).
class SoundFontSynthEngine final : public PagedSynthEngine {
public:
  SoundFontSynthEngine();
  ~SoundFontSynthEngine() override;

  static SoundFontSynthEngine* From(ISynthEngine* engine)
  {
    return engine != nullptr && engine->Descriptor().kind == SynthEngineKind::SoundFont ? static_cast<SoundFontSynthEngine*>(engine) : nullptr;
  }
  static const SoundFontSynthEngine* From(const ISynthEngine* engine)
  {
    return engine != nullptr && engine->Descriptor().kind == SynthEngineKind::SoundFont ? static_cast<const SoundFontSynthEngine*>(engine) : nullptr;
  }

  // The file (null: none). `fileName` is how a project names it. Its patches become the presets; none
  // is loaded yet: the track is silent until a patch is chosen (LoadPatch, LoadFirstPatch, a restored
  // state, RetryPatch).
  void SetFont(std::shared_ptr<soundfont::Font> font, std::string fileName);
  const std::shared_ptr<soundfont::Font>& SharedFont() const { return font_; }
  const std::string& FontFileName() const { return fontFileName_; }
  bool HasFont() const { return font_ != nullptr; }

  // The patch at bank / program of the file, or its first one, with the TONE settings kept (the app
  // choosing a patch for the user: another file, a new track; LoadPreset is the user choosing one,
  // which sets them back like any preset). False when the file has none such or it could not be
  // loaded (LoadMessage says why); the track then stays as it was. True as well when the patch is
  // being read (Loading): the preset is the new one, and should it not load after all, the state
  // before comes back and TakeLoadFailure says why.
  bool LoadPatch(unsigned bank, unsigned program);
  bool LoadFirstPatch();
  // The patch a restored state asked for while its file was not there: tried again now that it is.
  // False when nothing waits or it still cannot be loaded.
  bool RetryPatch();
  bool PatchWaits() const { return !wantedId_.empty(); }
  // Bank and program of the patch the state is based on; false on "sf.none" or a waiting patch.
  bool CurrentPatch(unsigned& bank, unsigned& program) const;

  // Whether the patch is in memory and playing, and when it is not, why ("" before any was chosen).
  bool Loaded() const { return program_ != nullptr; }
  // A patch chosen is being read in the background; the one before plays until it is there.
  bool Loading() const { return loading_; }
  // Why a patch read in the background did not load, once ("" for nothing new): the preset went back.
  std::string TakeLoadFailure();
  const std::string& LoadMessage() const { return loadMessage_; }
  std::size_t LoadedBytes() const { return program_ ? program_->Bytes() : 0; }

  bool LoadPreset(const std::string& presetId) override;
  // The patch's name without the numbers its list entry starts with.
  std::string CurrentPresetName() const override;
  SynthEngineState CaptureState() const override;
  SynthEngineState CaptureChanges() const override;
  bool CanRestore(const SynthEngineState& state) const override;
  bool RestoreState(const SynthEngineState& state) override;
  void DispatchPendingMessages() override;

protected:
  std::size_t FactoryPresetCount() const override;
  std::string_view FactoryPresetId(std::size_t index) const override;
  std::string FactoryPresetName(std::size_t index) const override;
  std::string FormatParameterValue(const SynthParameterInfo& parameter) const override;
  bool ShouldSyncParameterOnPresetLoad(const SynthParameterInfo& parameter) const override;
  void OnRuntimeSynced() override;

private:
  struct PatchId {
    char text[16];  // "sf.<bank>.<program>"
  };

  int PatchOfId(std::string_view presetId) const;
  bool Select(const std::string& presetId, bool keepTone);
  std::shared_ptr<const audio::sf2::Program> LoadProgram(std::size_t patch, std::string& message);
  bool ApplyPresetState(const std::string& presetId, bool keepTone, int patch);
  void CancelLoading();

  std::shared_ptr<soundfont::Font> font_;
  std::string fontFileName_;
  std::unique_ptr<PatchId[]> ids_;
  std::size_t idCount_ = 0;

  // The patch in memory and the preset it is ("" for none).
  std::shared_ptr<const audio::sf2::Program> program_;
  std::string programId_;
  std::string loadMessage_;
  // A restored state's patch that could not be found: kept so that saving the project keeps it.
  std::string wantedId_;
  std::string wantedName_;
  // A patch read in the background (soundfont::Font::LoadLater): its preset, the ticket to take it
  // with, and the state to go back to when it cannot be read.
  bool loading_ = false;
  std::uint32_t ticket_ = 0;
  std::string loadingId_;
  SynthEngineState fallback_;
  std::string failure_;
};

}  // namespace synth
