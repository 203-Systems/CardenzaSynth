#include "Applications/Synth/UI/Groove/GrooveController.h"

#include "Library/Synth/Core/Engines/Drums/DrumsSynthEngine.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace groove {

namespace {

std::string Upper(std::string text)
{
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  return text;
}

bool Matches(const std::string& label, const std::string& search)
{
  return search.empty() || Upper(label).find(search) != std::string::npos;
}

std::string TrimSpaces(const std::string& text)
{
  const auto first = text.find_first_not_of(' ');
  if (first == std::string::npos) return {};
  const auto last = text.find_last_not_of(' ');
  return text.substr(first, last - first + 1);
}

}  // namespace

Modal* Controller::Top()
{
  return state_.modals.empty() ? nullptr : &state_.modals.back();
}

void Controller::OpenModal(Modal modal)
{
  // Modals never stack more than three deep (list -> options -> text / confirm).
  if (state_.modals.size() >= 3) state_.modals.erase(state_.modals.begin());
  state_.modals.push_back(std::move(modal));
}

void Controller::CloseModal()
{
  if (!state_.modals.empty()) state_.modals.pop_back();
}

void Controller::OpenTextEntry(TextPurpose purpose, const std::string& title, const std::string& initial, std::size_t maxLength)
{
  Modal modal;
  modal.kind = ModalKind::TextEntry;
  modal.textPurpose = purpose;
  modal.title = title;
  modal.maxLength = maxLength;
  std::string text;
  for (const char c : Upper(initial))
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-') text.push_back(c);
  modal.text = text.substr(0, maxLength);
  modal.textCursor = modal.text.size();
  OpenModal(std::move(modal));
}

void Controller::OpenConfirm(ConfirmPurpose purpose, const std::string& title, const std::string& detail, bool danger, const std::string& payload)
{
  Modal modal;
  modal.kind = ModalKind::Confirm;
  modal.confirmPurpose = purpose;
  modal.title = title;
  modal.detail = detail;
  modal.danger = danger;
  modal.payload = payload;
  OpenModal(std::move(modal));
}

bool Controller::ConfirmLosingEdits(ConfirmPurpose purpose, const std::string& title, const std::string& payload)
{
  // A lesson plays on a practice project the guide throws away, and its steps say which key to press
  // next: a question no step mentions would only stop it.
  const synth::ISynthEngine* engine = ActiveEngine();
  if (guide_ || engine == nullptr || !engine->HasEdits()) return false;
  OpenConfirm(purpose, Upper(title), "EDITS TO " + Upper(engine->CurrentPresetName()) + " WILL BE LOST", true, payload);
  return true;
}

void Controller::ApplyPreset(const std::string& presetId)
{
  synth::ISynthEngine* engine = ActiveEngine();
  if (engine == nullptr) return;
  ReleaseAllPads();
  if (engine->LoadPreset(presetId)) MarkDirty();
  else ShowToast("PRESET UNAVAILABLE", ToastKind::Error);
}

void Controller::ApplyEngine(synth::SynthEngineKind kind)
{
  if (kind == ActiveKind()) return;
  ReleaseAllPads();
  std::string error;
  if (host_.SwitchEngine(state_.track, kind, error))
  {
    MarkDirty();
    state_.page[static_cast<std::size_t>(Scene::Instrument)] = 0;
    state_.instrumentFocus.fill(0);
    ShowToast(std::string("T") + std::to_string(state_.track + 1) + " " + std::string(EngineShortName(kind)), ToastKind::Ok);
  }
  else ShowToast(error.empty() ? "ENGINE UNAVAILABLE" : error, ToastKind::Error);
}

void Controller::ShowNoCardNotice()
{
  OpenConfirm(ConfirmPurpose::Notice, "NO SD CARD", "NOTHING WILL BE SAVED\nINSERT A CARD AND RESTART TO SAVE", false);
  std::printf("ui: no SD card: nothing will be saved\n");
}

void Controller::OpenPresetBrowser()
{
  if (ActiveEngine() == nullptr) return;
  Modal modal;
  modal.kind = ModalKind::PresetBrowser;
  modal.title = "PRESETS";
  OpenModal(std::move(modal));
  RefreshPresetBrowser();
}

void Controller::RefreshPresetBrowser()
{
  Modal* modal = Top();
  const synth::ISynthEngine* engine = ActiveEngine();
  if (modal == nullptr || engine == nullptr) return;
  if (modal->payload == "sounds")
  {
    // The drum sounds that match, the cursor on the pad's.
    synth::SynthParameterInfo info;
    const std::size_t lane = DrumLane();
    const int current = engine->GetParameterInfo(synth::kDrumSoundIds[lane], info) ? info.value : 0;
    modal->presets.clear();
    modal->cursor = 0;
    for (std::size_t index = 0; index < synth::drums::SoundCount(); ++index)
    {
      if (!Matches(synth::drums::Sound(index).name, modal->search)) continue;
      if (static_cast<int>(index) == current) modal->cursor = modal->presets.size();
      modal->presets.push_back(static_cast<std::uint16_t>(index));
    }
    FillSoundRows(*modal);
    return;
  }
  const bool reopened = modal->cursor < modal->presets.size();
  const std::size_t previous = reopened ? modal->presets[modal->cursor] : 0;
  modal->presets.clear();
  modal->cursor = 0;
  constexpr std::size_t kPage = 16;
  for (std::size_t offset = 0;; offset += kPage)
  {
    const auto page = engine->ListPresets({offset, kPage});
    modal->presets.reserve(page.totalCount);
    for (std::size_t index = 0; index < page.items.size(); ++index)
    {
      const auto& preset = page.items[index];
      if (!Matches(preset.name, modal->search)) continue;
      if (reopened ? offset + index == previous : preset.active) modal->cursor = modal->presets.size();
      modal->presets.push_back(static_cast<std::uint16_t>(offset + index));
    }
    if (page.items.empty() || offset + kPage >= page.totalCount) break;
  }
  FillPresetRows(*modal);
}

void Controller::FillPresetRows(Modal& modal) const
{
  const synth::ISynthEngine* engine = ActiveEngine();
  modal.items.clear();
  modal.itemCount = modal.presets.size();
  modal.firstItem = ListFirstRow(modal.cursor);
  if (engine == nullptr) return;
  for (std::size_t index = modal.firstItem; index < modal.firstItem + kListVisibleRows && index < modal.presets.size(); ++index)
  {
    const auto page = engine->ListPresets({modal.presets[index], 1});
    if (!page.items.empty()) modal.items.push_back({page.items.front().id, Upper(page.items.front().name), page.items.front().active});
  }
}

// ---------------------------------------------------------------------------------------------
// Drum pads: a sound from the list, a sample from the card, the microphone

void Controller::FillSoundRows(Modal& modal) const
{
  synth::SynthParameterInfo info;
  const synth::ISynthEngine* engine = ActiveEngine();
  const int current = engine != nullptr && engine->GetParameterInfo(synth::kDrumSoundIds[DrumLane()], info) ? info.value : -1;
  const bool sample = !host_.PadSample(state_.track, DrumLane()).empty();
  modal.items.clear();
  modal.itemCount = modal.presets.size();
  modal.firstItem = ListFirstRow(modal.cursor);
  for (std::size_t index = modal.firstItem; index < modal.firstItem + kListVisibleRows && index < modal.presets.size(); ++index)
  {
    const std::uint16_t sound = modal.presets[index];
    modal.items.push_back({std::to_string(sound), synth::drums::Sound(sound).name, !sample && sound == current});
  }
}

void Controller::OpenSoundBrowser()
{
  synth::ISynthEngine* engine = ActiveEngine();
  synth::SynthParameterInfo info;
  if (ActiveKind() != synth::SynthEngineKind::Drums || engine == nullptr || !engine->GetParameterInfo(synth::kDrumSoundIds[DrumLane()], info)) return;
  Modal modal;
  modal.kind = ModalKind::PresetBrowser;
  modal.payload = "sounds";
  modal.title = "P" + std::to_string(DrumLane() + 1) + " SOUND";
  modal.restoreValue = info.value;
  modal.detail = host_.PadSample(state_.track, DrumLane());
  OpenModal(std::move(modal));
  RefreshPresetBrowser();
}

// The chosen pad plays `sound` now (a sample on it comes off), and is heard.
void Controller::PreviewPadSound(int sound)
{
  synth::ISynthEngine* engine = ActiveEngine();
  if (engine == nullptr || ActiveKind() != synth::SynthEngineKind::Drums) return;
  const std::size_t lane = DrumLane();
  std::string message;
  if (!host_.PadSample(state_.track, lane).empty()) (void)host_.SetPadSample(state_.track, lane, "", message);
  if (engine->SetParameterValue(synth::kDrumSoundIds[lane], sound)) MarkDirty();
  AuditionPad();
}

void Controller::AuditionPad()
{
  if (ActiveKind() != synth::SynthEngineKind::Drums) return;
  const std::uint8_t note = synth::kDrumNotes[std::min<std::size_t>(DrumLane(), synth::kDrumNotes.size() - 1)];
  host_.NoteOn(state_.track, note, kDrumRowVelocity[1]);
  host_.NoteOff(state_.track, note);
}

void Controller::ClearPadSample()
{
  if (ActiveKind() != synth::SynthEngineKind::Drums || host_.PadSample(state_.track, DrumLane()).empty()) return;
  std::string message;
  if (!host_.SetPadSample(state_.track, DrumLane(), "", message)) return;
  MarkDirty();
  ShowToast("PAD " + std::to_string(DrumLane() + 1) + ": " + DrumSoundName(DrumLane()));
  AuditionPad();
}

void Controller::OpenPadSampleBrowser()
{
  if (ActiveKind() != synth::SynthEngineKind::Drums) return;
  Modal modal;
  modal.kind = ModalKind::SampleBrowser;
  modal.payload = "pad";
  modal.title = "P" + std::to_string(DrumLane() + 1) + " SAMPLE";
  OpenModal(std::move(modal));
  RefreshSampleBrowser();
}

void Controller::OpenPadRecord()
{
  if (ActiveKind() != synth::SynthEngineKind::Drums) return;
  Modal modal;
  modal.kind = ModalKind::PadRecord;
  modal.title = "RECORD PAD " + std::to_string(DrumLane() + 1);
  OpenModal(std::move(modal));
}

// ENTER on the pad's recording: starts it, and stops and keeps it (also at the end of the two seconds).
void Controller::PadRecordEnter()
{
  const PadRecordState recording = host_.PadRecording();
  std::string message;
  if (!recording.active)
  {
    if (!host_.BeginPadRecord(state_.track, DrumLane(), message)) ShowToast(message, ToastKind::Error);
    return;
  }
  if (host_.FinishPadRecord(message))
  {
    if (Top() != nullptr && Top()->kind == ModalKind::PadRecord) CloseModal();
    MarkDirty();
    ShowToast(message + " ON PAD " + std::to_string(recording.pad + 1), ToastKind::Ok);
    return;
  }
  ShowToast(message, ToastKind::Error);
}

void Controller::OpenEnginePicker()
{
  Modal modal;
  modal.kind = ModalKind::EnginePicker;
  modal.title = "ENGINE";
  const auto current = ActiveKind();
  const std::array<synth::SynthEngineKind, 4> kinds{{synth::SynthEngineKind::Dx7, synth::SynthEngineKind::Juno,
                                                      synth::SynthEngineKind::Sampler, synth::SynthEngineKind::Drums}};
  for (std::size_t index = 0; index < kinds.size(); ++index)
  {
    modal.items.push_back({std::to_string(static_cast<int>(kinds[index])), std::string(EngineShortName(kinds[index])), kinds[index] == current});
    if (kinds[index] == current) modal.cursor = index;
  }
  OpenModal(std::move(modal));
}

void Controller::OpenSampleBrowser()
{
  if (ActiveKind() != synth::SynthEngineKind::Sampler) return;
  Modal modal;
  modal.kind = ModalKind::SampleBrowser;
  modal.title = "SAMPLES";
  OpenModal(std::move(modal));
  RefreshSampleBrowser();
}

void Controller::RefreshSampleBrowser()
{
  Modal* modal = Top();
  if (modal == nullptr) return;
  const std::string current = modal->payload == "pad" ? host_.PadSample(state_.track, DrumLane()) : host_.Sampler().BuildUiState(state_.track).sampleFileName;
  modal->items.clear();
  for (const std::string& file : host_.ListSamples())
    if (Matches(file, modal->search)) modal->items.push_back({file, Upper(file), file == current});
  modal->cursor = 0;
  for (std::size_t index = 0; index < modal->items.size(); ++index)
    if (modal->items[index].marked) { modal->cursor = index; break; }
}

void Controller::OpenSampleRecord()
{
  Modal modal;
  modal.kind = ModalKind::SampleRecord;
  modal.title = "RECORD";
  OpenModal(std::move(modal));
}

void Controller::ModalAdjust(int delta, bool coarse)
{
  Modal* modal = Top();
  if (modal == nullptr) return;
  switch (modal->kind)
  {
    case ModalKind::Menu:
      return;  // the open menu only takes the GRID (and FN + its letters)
    case ModalKind::Record:
      RecordAdjust(delta);
      return;
    case ModalKind::PresetBrowser:
    case ModalKind::EnginePicker:
    case ModalKind::SampleBrowser:
    case ModalKind::SampleOptions:
    {
      if (modal->Count() == 0) return;
      const int count = static_cast<int>(modal->Count());
      // Vertical lists: ADD (the upper key, drawn as an up arrow) moves up, SUB down. The engine
      // picker is a row: ADD moves right.
      const int direction = modal->kind == ModalKind::EnginePicker ? delta : -delta;
      const int step = direction * (coarse ? 8 : 1);
      const std::size_t before = modal->cursor;
      modal->cursor = static_cast<std::size_t>(std::clamp(static_cast<int>(modal->cursor) + step, 0, count - 1));
      if (modal->kind == ModalKind::PresetBrowser && modal->payload == "sounds")
      {
        // Each sound is heard on the pad as the cursor comes to it.
        if (modal->cursor != before) PreviewPadSound(modal->presets[modal->cursor]);
        FillSoundRows(*modal);
      }
      else if (modal->kind == ModalKind::PresetBrowser) FillPresetRows(*modal);
      return;
    }
    case ModalKind::SampleRecord:
    {
      const auto sampler = host_.Sampler().BuildUiState(state_.track);
      synth::ISynthEngine* engine = ActiveEngine();
      if (!sampler.hasDraft || engine == nullptr) return;
      const char* id = modal->trimMarker == 0 ? "sample.start" : "sample.end";
      (void)engine->AdjustParameterValue(id, delta * (coarse ? 8 : 1));
      return;
    }
    default:
      return;
  }
}

void Controller::ModalEnter()
{
  Modal* modal = Top();
  if (modal == nullptr) return;
  switch (modal->kind)
  {
    case ModalKind::Menu:
      return;
    case ModalKind::Record:
      RecordEnter();
      return;
    case ModalKind::Confirm:
      Confirmed();
      return;
    case ModalKind::PresetBrowser:
    {
      synth::ISynthEngine* engine = ActiveEngine();
      const ListItem* row = modal->Row(modal->cursor);
      if (engine == nullptr || row == nullptr) return;
      if (modal->payload == "sounds")
      {
        // The one under the cursor stays (it already plays, unless ENTER came before any move).
        const int sound = std::stoi(row->id);
        synth::SynthParameterInfo info;
        const bool sample = !host_.PadSample(state_.track, DrumLane()).empty();
        if (sample || !engine->GetParameterInfo(synth::kDrumSoundIds[DrumLane()], info) || info.value != sound) PreviewPadSound(sound);
        CloseModal();
        ShowToast("PAD " + std::to_string(DrumLane() + 1) + ": " + row->label);
        return;
      }
      const std::string id = row->id;
      // The current preset too: loading it again drops its edits.
      if (ConfirmLosingEdits(ConfirmPurpose::ChangePreset, "LOAD " + row->label + "?", id)) return;
      CloseModal();
      ApplyPreset(id);
      return;
    }
    case ModalKind::EnginePicker:
    {
      if (modal->items.empty()) return;
      const auto kind = static_cast<synth::SynthEngineKind>(std::stoi(modal->items[modal->cursor].id));
      if (kind == ActiveKind())
      {
        CloseModal();
        return;
      }
      if (ConfirmLosingEdits(ConfirmPurpose::ChangeEngine, "SWITCH TO " + std::string(EngineShortName(kind)) + "?",
                             std::to_string(static_cast<int>(kind))))
        return;
      CloseModal();
      ApplyEngine(kind);
      return;
    }
    case ModalKind::SampleBrowser:
    {
      if (modal->items.empty()) return;
      const std::string id = modal->items[modal->cursor].id;
      if (modal->payload == "pad")
      {
        std::string message;
        if (host_.SetPadSample(state_.track, DrumLane(), id, message))
        {
          CloseModal();
          MarkDirty();
          ShowToast(Upper(id.substr(0, id.rfind('.'))) + " ON PAD " + std::to_string(DrumLane() + 1), ToastKind::Ok);
          AuditionPad();
        }
        else ShowToast(message, ToastKind::Error);
        return;
      }
      auto& sampler = host_.Sampler();
      if (sampler.SelectBrowserSample(state_.track, id))
      {
        CloseModal();
        MarkDirty();
        ShowToast("LOADED " + Upper(id), ToastKind::Ok);
      }
      else ShowToast(sampler.BuildUiState(state_.track).statusText, ToastKind::Error);
      return;
    }
    case ModalKind::SampleOptions:
    {
      if (modal->items.empty()) return;
      const std::string action = modal->items[modal->cursor].id;
      const std::string file = modal->payload;
      if (action == "rename")
      {
        const auto dot = file.rfind('.');
        OpenTextEntry(TextPurpose::SampleRename, "RENAME SAMPLE", dot == std::string::npos ? file : file.substr(0, dot), 18);
        Top()->payload = file;
      }
      else OpenConfirm(ConfirmPurpose::DeleteSample, "DELETE " + Upper(file) + "?", "THE WAV FILE IS REMOVED FROM THE CARD", true, file);
      return;
    }
    case ModalKind::SampleRecord:
      SampleRecordEnter();
      return;
    case ModalKind::PadRecord:
      PadRecordEnter();
      return;
    case ModalKind::TextEntry:
      CommitText();
      return;
    case ModalKind::None:
    default:
      return;
  }
}

void Controller::ModalFnEnter()
{
  Modal* modal = Top();
  if (modal != nullptr && modal->kind == ModalKind::Menu)
  {
    // FN + ENTER is the screen's second ENTER, menu open or not.
    CloseModal();
    FnEnter();
    return;
  }
  if (modal != nullptr && modal->kind == ModalKind::Record)
  {
    RecordFnEnter();
    return;
  }
  // A pad's list only chooses: renaming and deleting are in the sampler's.
  if (modal == nullptr || modal->kind != ModalKind::SampleBrowser || modal->items.empty() || modal->payload == "pad") return;
  Modal options;
  options.kind = ModalKind::SampleOptions;
  options.title = modal->items[modal->cursor].label;
  options.payload = modal->items[modal->cursor].id;
  options.items = {{"rename", "RENAME", false}, {"delete", "DELETE", false}};
  OpenModal(std::move(options));
}

void Controller::ModalGrid(std::size_t index)
{
  Modal* modal = Top();
  if (modal == nullptr) return;
  if (modal->kind == ModalKind::SampleRecord)
  {
    SampleRecordGrid(index);
    return;
  }
  if (modal->kind == ModalKind::Menu)
  {
    RunMenu(index);
    return;
  }
  if (modal->kind == ModalKind::Record)
  {
    RecordGrid(index);
    return;
  }
  if (modal->kind == ModalKind::EnginePicker && index < modal->items.size())
  {
    modal->cursor = index;
    ModalEnter();
  }
}

void Controller::ModalCancel()
{
  Modal* modal = Top();
  if (modal == nullptr) return;
  auto& sampler = host_.Sampler();
  switch (modal->kind)
  {
    case ModalKind::PresetBrowser:
    case ModalKind::SampleBrowser:
      if (!modal->search.empty())
      {
        modal->search.clear();
        modal->kind == ModalKind::PresetBrowser ? RefreshPresetBrowser() : RefreshSampleBrowser();
        return;
      }
      if (modal->payload == "sounds")
      {
        // The pad gets back what it played before the list opened.
        const int sound = modal->restoreValue;
        const std::string sample = modal->detail;
        CloseModal();
        std::string message;
        if (synth::ISynthEngine* engine = ActiveEngine(); engine != nullptr && sound >= 0) (void)engine->SetParameterValue(synth::kDrumSoundIds[DrumLane()], sound);
        if (!sample.empty() && host_.PadSample(state_.track, DrumLane()).empty()) (void)host_.SetPadSample(state_.track, DrumLane(), sample, message);
        return;
      }
      break;
    case ModalKind::PadRecord:
      if (host_.PadRecording().active)
      {
        host_.CancelPadRecord();
        ShowToast("RECORDING CANCELLED");
      }
      break;
    case ModalKind::SampleRecord:
    {
      const auto state = sampler.BuildUiState(state_.track);
      if (state.recording)
      {
        sampler.CancelRecord(state_.track);
        return;
      }
      if (state.hasDraft)
      {
        OpenConfirm(ConfirmPurpose::DiscardTake, "DISCARD TAKE?", "THE RECORDING IS NOT SAVED", true);
        return;
      }
      break;
    }
    default:
      break;
  }
  CloseModal();
}

void Controller::CommitText()
{
  Modal* modal = Top();
  if (modal == nullptr) return;
  const std::string text = TrimSpaces(modal->text);
  if (modal->textPurpose == TextPurpose::Search)
  {
    // The browser underneath filters by it (an empty field shows everything again).
    CloseModal();
    Modal* browser = Top();
    if (browser != nullptr && (browser->kind == ModalKind::PresetBrowser || browser->kind == ModalKind::SampleBrowser))
    {
      browser->search = text;
      browser->cursor = 0;
      browser->kind == ModalKind::PresetBrowser ? RefreshPresetBrowser() : RefreshSampleBrowser();
    }
    return;
  }
  if (text.empty())
  {
    ShowToast("NAME REQUIRED", ToastKind::Error);
    return;
  }
  const TextPurpose purpose = modal->textPurpose;
  const std::string payload = modal->payload;
  auto& sampler = host_.Sampler();
  switch (purpose)
  {
    case TextPurpose::ProjectSaveAs:
      CloseModal();
      SaveProjectNamed(text, text == state_.projectName);
      return;
    case TextPurpose::PresetSaveAs:
    {
      CloseModal();
      synth::ISynthEngine* engine = ActiveEngine();
      if (engine != nullptr && engine->SavePreset("", text)) ShowToast("SAVED " + text, ToastKind::Ok);
      else ShowToast("PRESET SAVE FAILED", ToastKind::Error);
      return;
    }
    case TextPurpose::SampleRename:
    {
      std::string renamed;
      if (sampler.RenameBrowserSample(state_.track, payload, text, &renamed))
      {
        CloseModal();
        if (Top() != nullptr && Top()->kind == ModalKind::SampleOptions) CloseModal();
        if (Top() != nullptr && Top()->kind == ModalKind::SampleBrowser) RefreshSampleBrowser();
        ShowToast("RENAMED", ToastKind::Ok);
      }
      else ShowToast(sampler.BuildUiState(state_.track).statusText, ToastKind::Error);
      return;
    }
    case TextPurpose::Search:
      return;  // handled above
  }
}

void Controller::Confirmed()
{
  Modal* modal = Top();
  if (modal == nullptr) return;
  const ConfirmPurpose purpose = modal->confirmPurpose;
  const std::string payload = modal->payload;
  CloseModal();
  auto& sampler = host_.Sampler();
  auto& sequencer = host_.Sequencer();
  switch (purpose)
  {
    case ConfirmPurpose::NewProject:
      NewProject();
      break;
    case ConfirmPurpose::LoadProject:
      LoadProjectNamed(payload);
      break;
    case ConfirmPurpose::OverwriteProject:
      SaveProjectNamed(payload, true);
      break;
    case ConfirmPurpose::ClearPattern:
    {
      const auto ui = sequencer.BuildUiState(state_.track);
      if (sequencer.ClearPatternEvents(state_.track, ui.tracks[state_.track].activePattern))
      {
        MarkDirty();
        ShowToast("PATTERN CLEARED");
      }
      break;
    }
    case ConfirmPurpose::DeleteSample:
      // The sampler arms on the first request and deletes on the second for the same file.
      if (sampler.DeleteBrowserSample(state_.track, payload) || sampler.DeleteBrowserSample(state_.track, payload))
      {
        if (Top() != nullptr && Top()->kind == ModalKind::SampleOptions) CloseModal();
        if (Top() != nullptr && Top()->kind == ModalKind::SampleBrowser) RefreshSampleBrowser();
        ShowToast("DELETED", ToastKind::Ok);
      }
      else ShowToast(sampler.BuildUiState(state_.track).statusText, ToastKind::Error);
      break;
    case ConfirmPurpose::DiscardTake:
      sampler.DiscardDraftTake(state_.track);
      if (Top() != nullptr && Top()->kind == ModalKind::SampleRecord) CloseModal();
      ShowToast("TAKE DISCARDED");
      break;
    case ConfirmPurpose::ReplaceTake:
      OpenSampleRecord();
      if (!sampler.BeginRecord(state_.track, true)) ShowToast(sampler.BuildUiState(state_.track).statusText, ToastKind::Error);
      break;
    case ConfirmPurpose::ChangePreset:
      if (Top() != nullptr && Top()->kind == ModalKind::PresetBrowser) CloseModal();
      ApplyPreset(payload);
      break;
    case ConfirmPurpose::ChangeEngine:
      if (Top() != nullptr && Top()->kind == ModalKind::EnginePicker) CloseModal();
      ApplyEngine(static_cast<synth::SynthEngineKind>(std::stoi(payload)));
      break;
    case ConfirmPurpose::Notice:
      break;  // read: closed
  }
}

void Controller::SampleRecordEnter()
{
  auto& sampler = host_.Sampler();
  const auto state = sampler.BuildUiState(state_.track);
  if (state.recording)
  {
    (void)sampler.StopRecord(state_.track);
    return;
  }
  if (state.hasDraft)
  {
    Modal* modal = Top();
    if (modal != nullptr) modal->trimMarker = modal->trimMarker == 0 ? 1 : 0;
    return;
  }
  if (!sampler.BeginRecord(state_.track, false)) ShowToast(sampler.BuildUiState(state_.track).statusText, ToastKind::Error);
}

void Controller::SampleRecordGrid(std::size_t index)
{
  auto& sampler = host_.Sampler();
  const auto state = sampler.BuildUiState(state_.track);
  Modal* modal = Top();
  if (modal == nullptr) return;
  if (!state.hasDraft)
  {
    if (index == 0) SampleRecordEnter();
    return;
  }
  switch (index)
  {
    case 0: modal->trimMarker = 0; break;
    case 1: modal->trimMarker = 1; break;
    case 2: host_.PreviewSample(state_.track); break;
    case 3: sampler.EnterDraftNaming(state_.track); break;
    default: break;
  }
}

}  // namespace groove
