#include "Library/Synth/Core/Engines/SoundFont/SoundFontSynthEngine.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>

#include "Library/Memory/Memory.h"
#include "Library/Synth/Audio/SynthAudioRuntime.h"
#include "Library/Synth/Core/Engines/SoundFont/SoundFontParameters.h"

namespace synth {

namespace {

// The preset of a track without a patch: silent.
constexpr const char* kNoPatchId = "sf.none";
constexpr const char* kNoPatchName = "No Sound";

bool Digits(std::string_view text) { return !text.empty() && text.size() <= 5 && std::all_of(text.begin(), text.end(), [](unsigned char c) { return c >= '0' && c <= '9'; }); }

// "sf.<bank>.<program>".
bool ParsePatchId(std::string_view id, unsigned& bank, unsigned& program)
{
  if (id.size() < 6 || id.compare(0, 3, "sf.") != 0) return false;
  const std::size_t dot = id.find('.', 3);
  if (dot == std::string_view::npos) return false;
  const std::string_view bankText = id.substr(3, dot - 3);
  const std::string_view programText = id.substr(dot + 1);
  if (!Digits(bankText) || !Digits(programText)) return false;
  bank = static_cast<unsigned>(std::atoi(std::string(bankText).c_str()));
  program = static_cast<unsigned>(std::atoi(std::string(programText).c_str()));
  return true;
}

// A list entry is "000:000 Name" (so the list sorts by bank and program); the name alone.
std::string WithoutNumbers(const std::string& name)
{
  std::size_t at = 0;
  while (at < name.size() && name[at] >= '0' && name[at] <= '9') ++at;
  if (at == 0 || at >= name.size() || name[at] != ':') return name;
  std::size_t after = at + 1;
  while (after < name.size() && name[after] >= '0' && name[after] <= '9') ++after;
  if (after == at + 1 || after >= name.size() || name[after] != ' ') return name;
  return name.substr(after + 1);
}

std::string Signed(int value) { return value > 0 ? "+" + std::to_string(value) : std::to_string(value); }

}  // namespace

SoundFontSynthEngine::SoundFontSynthEngine() : PagedSynthEngine({SynthEngineKind::SoundFont, "soundfont", "SoundFont"})
{
  ReserveParameters(soundfont::kParameterCount);
  RegisterParameter({soundfont::kCutoffId, "Cutoff", "Tone", SynthParameterKind::Continuous, 0, 127, 1, 127, 127});
  RegisterParameter({soundfont::kResonanceId, "Resonance", "Tone", SynthParameterKind::Continuous, 0, 127, 1, 0, 0});
  RegisterParameter({soundfont::kAttackId, "Attack", "Tone", SynthParameterKind::Continuous, -63, 63, 1, 0, 0});
  RegisterParameter({soundfont::kReleaseId, "Release", "Tone", SynthParameterKind::Continuous, -63, 63, 1, 0, 0});
  RegisterParameter({soundfont::kTuneId, "Tune", "Tone", SynthParameterKind::Continuous, -24, 24, 1, 0, 0});
  RegisterParameter({soundfont::kFineId, "Fine", "Tone", SynthParameterKind::Continuous, -50, 50, 1, 0, 0});
  RegisterParameter({soundfont::kLevelId, "Level", "Tone", SynthParameterKind::Continuous, 0, 127, 1, soundfont::kDefaultLevel, soundfont::kDefaultLevel});
  RegisterParameter({soundfont::kVelocityId, "Velocity", "Tone", SynthParameterKind::Continuous, 0, 127, 1, 127, 127});
  RegisterTrackFxParameters();
  RegisterPreset(kNoPatchId, kNoPatchName, {});
  (void)PagedSynthEngine::LoadPreset(kNoPatchId);
}

SoundFontSynthEngine::~SoundFontSynthEngine() { CancelLoading(); }

void SoundFontSynthEngine::SetFont(std::shared_ptr<soundfont::Font> font, std::string fileName)
{
  CancelLoading();
  // The TONE settings stay through a change of file.
  SynthEngineState silent = PagedSynthEngine::CaptureState();
  silent.basePresetId = kNoPatchId;
  silent.presetId = kNoPatchId;
  silent.presetName = kNoPatchName;

  font_ = std::move(font);
  fontFileName_ = font_ ? std::move(fileName) : std::string();
  ids_.reset();
  idCount_ = 0;
  loadMessage_.clear();
  const std::size_t count = font_ ? (std::min)(font_->PatchCount(), std::size_t{UINT16_MAX}) : 0;
  if (count != 0)
  {
    ids_.reset(new (memory::slow) PatchId[count]);
    if (ids_)
    {
      for (std::size_t index = 0; index < count; ++index)
      {
        const soundfont::Patch& patch = font_->PatchAt(index);
        std::snprintf(ids_[index].text, sizeof(ids_[index].text), "sf.%u.%u", static_cast<unsigned>(patch.bank), static_cast<unsigned>(patch.program));
      }
      idCount_ = count;
    }
    else
    {
      loadMessage_ = "NO MEMORY";
    }
  }
  IndexFactoryPresets();
  // The patch in memory was the old file's: silent until one of this file is chosen. (Letting go of a
  // patch here is safe: the audio never frees one, it hands its own back to the UI thread.)
  program_.reset();
  programId_.clear();
  (void)PagedSynthEngine::RestoreState(silent);
}

bool SoundFontSynthEngine::LoadPatch(unsigned bank, unsigned program)
{
  char id[16];
  std::snprintf(id, sizeof(id), "sf.%u.%u", bank, program);
  if (PatchOfId(id) < 0) return false;
  return Select(id, true);
}

bool SoundFontSynthEngine::LoadFirstPatch()
{
  if (idCount_ == 0) return false;
  return Select(ids_[0].text, true);
}

bool SoundFontSynthEngine::RetryPatch()
{
  if (wantedId_.empty()) return false;
  const SynthEngineState state = CaptureState();  // with the patch that waits
  const std::string id = wantedId_;
  const std::string name = wantedName_;
  wantedId_.clear();
  wantedName_.clear();
  if (PagedSynthEngine::RestoreState(state)) return Loaded();
  wantedId_ = id;
  wantedName_ = name;
  return false;
}

bool SoundFontSynthEngine::CurrentPatch(unsigned& bank, unsigned& program) const
{
  return wantedId_.empty() && ParsePatchId(BasePresetId(), bank, program);
}

bool SoundFontSynthEngine::LoadPreset(const std::string& presetId)
{
  return Select(presetId, false);
}

// A preset becomes the track's sound. As a preset load (`keepTone` false) the TONE settings go back
// to neutral, as loading a preset does on every engine; for the app choosing a patch on the user's
// behalf (another file, a new track) they stay.
bool SoundFontSynthEngine::Select(const std::string& presetId, bool keepTone)
{
  const int patch = PatchOfId(presetId);
  if (patch >= 0 && !(program_ && programId_ == presetId) && !(loading_ && loadingId_ == presetId))
  {
    // In the background where the device can: the preset changes now, the patch before plays until
    // the new one is there (DispatchPendingMessages) and comes back if it cannot be read.
    const std::uint32_t ticket = font_ ? font_->LoadLater(EngineIndex(), static_cast<std::size_t>(patch), soundfont::kTrackBudgetBytes) : 0;
    if (ticket != 0)
    {
      if (!loading_) fallback_ = CaptureState();
      loading_ = true;
      ticket_ = ticket;
      loadingId_ = presetId;
      loadMessage_.clear();
      if (!ApplyPresetState(presetId, keepTone, patch))
      {
        CancelLoading();
        return false;
      }
      return true;
    }
    // A synchronous fallback can supersede an outstanding ticket too. The earlier request no
    // longer owns a slot, so restore its starting point before trying the new patch.
    if (loading_)
    {
      const SynthEngineState fallback = std::move(fallback_);
      CancelLoading();
      (void)RestoreState(fallback);
    }
    std::string message;
    std::shared_ptr<const audio::sf2::Program> program = LoadProgram(static_cast<std::size_t>(patch), message);
    if (!program)
    {
      loadMessage_ = message;
      return false;
    }
    program_ = std::move(program);
    programId_ = presetId;
    loadMessage_.clear();
  }
  return ApplyPresetState(presetId, keepTone, patch);
}

// The engine's state becomes the preset, its TONE settings kept or set back (Select).
bool SoundFontSynthEngine::ApplyPresetState(const std::string& presetId, bool keepTone, int patch)
{
  if (keepTone && patch >= 0)
  {
    SynthEngineState state = PagedSynthEngine::CaptureState();
    state.basePresetId = presetId;
    state.presetId = presetId;
    state.presetName = FactoryPresetName(static_cast<std::size_t>(patch));
    if (!PagedSynthEngine::RestoreState(state)) return false;
  }
  else if (!PagedSynthEngine::LoadPreset(presetId))
  {
    return false;
  }
  wantedId_.clear();
  wantedName_.clear();
  return true;
}

void SoundFontSynthEngine::CancelLoading()
{
  if (loading_ && font_) font_->CancelLoad(ticket_);
  loading_ = false;
  ticket_ = 0;
  loadingId_.clear();
  fallback_ = SynthEngineState{};
}

std::string SoundFontSynthEngine::TakeLoadFailure()
{
  std::string failure = std::move(failure_);
  failure_.clear();
  return failure;
}

void SoundFontSynthEngine::DispatchPendingMessages()
{
  PagedSynthEngine::DispatchPendingMessages();
  if (!loading_ || !font_) return;
  std::shared_ptr<const audio::sf2::Program> program;
  std::string message;
  if (!font_->TakeLoaded(EngineIndex(), ticket_, program, message)) return;
  const std::string id = loadingId_;
  const SynthEngineState fallback = std::move(fallback_);
  loading_ = false;
  ticket_ = 0;
  loadingId_.clear();
  fallback_ = SynthEngineState{};
  if (program)
  {
    program_ = std::move(program);
    programId_ = id;
    loadMessage_.clear();
    audio::SetSoundFontProgram(EngineIndex(), program_);
    return;
  }
  // The state before comes back, as if the patch had not been chosen (TONE edits made meanwhile go
  // with it); the UI says why once.
  (void)RestoreState(fallback);
  loadMessage_ = message;
  failure_ = message.empty() ? std::string("PATCH NOT LOADED") : message;
}

std::string SoundFontSynthEngine::CurrentPresetName() const
{
  return WithoutNumbers(wantedName_.empty() ? PagedSynthEngine::CurrentPresetName() : wantedName_);
}

SynthEngineState SoundFontSynthEngine::CaptureState() const
{
  SynthEngineState state = PagedSynthEngine::CaptureState();
  if (!wantedId_.empty())
  {
    state.basePresetId = wantedId_;
    state.presetId = wantedId_;
    state.presetName = wantedName_;
  }
  return state;
}

SynthEngineState SoundFontSynthEngine::CaptureChanges() const
{
  SynthEngineState state = PagedSynthEngine::CaptureChanges();
  if (!wantedId_.empty())
  {
    state.basePresetId = wantedId_;
    state.presetId = wantedId_;
    state.presetName = wantedName_;
  }
  return state;
}

bool SoundFontSynthEngine::CanRestore(const SynthEngineState& state) const
{
  if (PagedSynthEngine::CanRestore(state)) return true;
  // A patch of a file that is not there (or not in it): the track takes the rest and waits for it.
  unsigned bank = 0;
  unsigned program = 0;
  if (!ParsePatchId(state.basePresetId, bank, program)) return false;
  SynthEngineState silent = state;
  silent.basePresetId = kNoPatchId;
  silent.presetId = kNoPatchId;
  return PagedSynthEngine::CanRestore(silent);
}

bool SoundFontSynthEngine::RestoreState(const SynthEngineState& state)
{
  wantedId_.clear();
  wantedName_.clear();
  if (PagedSynthEngine::RestoreState(state)) return true;
  unsigned bank = 0;
  unsigned program = 0;
  if (!ParsePatchId(state.basePresetId, bank, program)) return false;
  SynthEngineState silent = state;
  silent.basePresetId = kNoPatchId;
  silent.presetId = kNoPatchId;
  if (!PagedSynthEngine::RestoreState(silent)) return false;
  wantedId_ = state.basePresetId;
  wantedName_ = state.presetName;
  loadMessage_ = font_ ? "PATCH NOT IN FONT" : "FONT MISSING";
  return true;
}

std::size_t SoundFontSynthEngine::FactoryPresetCount() const { return idCount_; }

std::string_view SoundFontSynthEngine::FactoryPresetId(std::size_t index) const
{
  return index < idCount_ ? std::string_view(ids_[index].text) : std::string_view();
}

std::string SoundFontSynthEngine::FactoryPresetName(std::size_t index) const
{
  if (!font_ || index >= idCount_) return "";
  const soundfont::Patch& patch = font_->PatchAt(index);
  char text[40];
  std::snprintf(text, sizeof(text), "%03u:%03u %s", static_cast<unsigned>(patch.bank), static_cast<unsigned>(patch.program), patch.name[0] != 0 ? patch.name : "PATCH");
  return text;
}

std::string SoundFontSynthEngine::FormatParameterValue(const SynthParameterInfo& parameter) const
{
  if (parameter.id == soundfont::kCutoffId && parameter.value >= 127) return "OPEN";
  if (parameter.id == soundfont::kAttackId || parameter.id == soundfont::kReleaseId || parameter.id == soundfont::kTuneId || parameter.id == soundfont::kFineId)
    return Signed(parameter.value);
  return PagedSynthEngine::FormatParameterValue(parameter);
}

// Every value on every preset load: the runtime keeps no patch defaults to fall back to.
bool SoundFontSynthEngine::ShouldSyncParameterOnPresetLoad(const SynthParameterInfo& parameter) const
{
  return ShouldSyncParameterToRuntime(parameter);
}

void SoundFontSynthEngine::OnRuntimeSynced()
{
  const std::string& base = BasePresetId();
  // The state is another preset than the one whose patch is being read in the background (chosen
  // again, restored, set back): that read is not wanted any more.
  if (loading_ && loadingId_ != base) CancelLoading();
  if (programId_ != base && !loading_)
  {
    // A restored state or a user preset: its patch is read here.
    program_.reset();
    programId_ = base;
    loadMessage_.clear();
    const int patch = PatchOfId(base);
    if (patch >= 0) program_ = LoadProgram(static_cast<std::size_t>(patch), loadMessage_);
    else if (base != kNoPatchId) loadMessage_ = font_ ? "PATCH NOT IN FONT" : "FONT MISSING";
    if (program_) loadMessage_.clear();
  }
  audio::SetSoundFontProgram(EngineIndex(), program_);
}

int SoundFontSynthEngine::PatchOfId(std::string_view presetId) const
{
  for (std::size_t index = 0; index < idCount_; ++index)
    if (presetId == ids_[index].text) return static_cast<int>(index);
  return -1;
}

std::shared_ptr<const audio::sf2::Program> SoundFontSynthEngine::LoadProgram(std::size_t patch, std::string& message)
{
  if (!font_)
  {
    message = "NO FONT";
    return {};
  }
  return font_->Load(patch, soundfont::kTrackBudgetBytes, message);
}

}  // namespace synth
