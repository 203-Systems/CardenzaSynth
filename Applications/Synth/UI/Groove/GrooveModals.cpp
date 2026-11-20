#include "Applications/Synth/UI/Groove/GrooveController.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

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
  if (guide_ || engine == nullptr) return false;
  // While the browser plays another preset, the edits at stake are those of the sound set aside.
  const synth::SynthEngineState* kept = PresetPreviewKept(state_.track);
  if (kept != nullptr ? !preview_->keptEdits : !engine->HasEdits()) return false;
  const std::string name = kept != nullptr ? kept->presetName : engine->CurrentPresetName();
  OpenConfirm(purpose, Upper(title), "EDITS TO " + Upper(name) + " WILL BE LOST", true, payload);
  return true;
}

void Controller::ApplyPreset(const std::string& presetId)
{
  synth::ISynthEngine* engine = ActiveEngine();
  if (engine == nullptr) return;
  ReleaseAllPads();
  // The preview already plays it as loading it would (nothing can edit it in the browser): it stays,
  // and AMY builds no voices again.
  const bool previewed = PresetPreviewKept(state_.track) != nullptr && preview_->loaded == presetId && !engine->HasEdits();
  if (!previewed && !engine->LoadPreset(presetId))
  {
    EndPresetPreview(true);
    ShowToast("PRESET UNAVAILABLE", ToastKind::Error);
    return;
  }
  preview_.reset();
  MarkDirty();
}

const synth::SynthEngineState* Controller::PresetPreviewKept(std::size_t track) const
{
  // Only while the preset on trial is still what the track's engine has: a project that replaced it
  // took the preview's place.
  if (!preview_ || preview_->track != track) return nullptr;
  const synth::ISynthEngine* engine = host_.Engine(track);
  if (engine == nullptr || host_.EngineKind(track) != preview_->kind || engine->CurrentPresetId() != preview_->loaded) return nullptr;
  return &preview_->kept;
}

std::string Controller::KeptPresetId() const
{
  if (const synth::SynthEngineState* kept = PresetPreviewKept(state_.track)) return kept->presetId;
  const synth::ISynthEngine* engine = ActiveEngine();
  return engine != nullptr ? engine->CurrentPresetId() : std::string();
}

bool Controller::PresetBrowserOpen() const
{
  return std::any_of(state_.modals.begin(), state_.modals.end(), [](const Modal& modal) { return modal.kind == ModalKind::PresetBrowser; });
}

std::string Controller::PresetHeard() const
{
  const synth::ISynthEngine* engine = ActiveEngine();
  if (engine == nullptr) return {};
  for (std::size_t index = 0; index < state_.padNote.size(); ++index)
    if (state_.padNote[index] != kNoNote && state_.padTrack[index] == state_.track && state_.padChordCount[index] != 0)
      return engine->CurrentPresetId();
  return {};
}

void Controller::PreviewPreset()
{
  Modal* modal = Top();
  synth::ISynthEngine* engine = ActiveEngine();
  if (modal == nullptr || modal->kind != ModalKind::PresetBrowser || engine == nullptr) return;
  const ListItem* row = modal->Row(modal->cursor);
  if (row == nullptr || row->id == engine->CurrentPresetId()) return;  // that one sounds already
  // Back on the track's own preset: its own sound, edits and all.
  if (const synth::SynthEngineState* kept = PresetPreviewKept(state_.track); kept != nullptr && row->id == kept->presetId)
  {
    EndPresetPreview(true);
    return;
  }
  // The first preset on trial sets the track's sound aside (only what differs from its preset: a
  // few values, often none).
  if (PresetPreviewKept(state_.track) == nullptr)
    preview_ = PresetPreview{state_.track, ActiveKind(), engine->CaptureChanges(), engine->HasEdits(), {}};
  // As loading a preset anywhere: notes of the last one end first. The heavy part (AMY building the
  // voices) waits on the render thread for the output to be ahead; this pad's note waits behind it.
  ReleaseAllPads();
  const std::string id = row->id;
  if (!engine->LoadPreset(id))
  {
    EndPresetPreview(true);
    ShowToast("PRESET UNAVAILABLE", ToastKind::Error);
    return;
  }
  preview_->loaded = id;
}

void Controller::EndPresetPreview(bool restore)
{
  if (!preview_) return;
  const bool current = PresetPreviewKept(preview_->track) != nullptr;
  const PresetPreview preview = std::move(*preview_);
  preview_.reset();
  if (!restore || !current) return;
  synth::ISynthEngine* engine = host_.Engine(preview.track);
  if (preview.track == state_.track) ReleaseAllPads();
  if (engine->RestoreState(preview.kept)) return;
  // Not expected (the state came from this engine a moment ago): the preset at least.
  std::printf("ui: the sound before the preset preview did not come back: %s\n", preview.kept.presetId.c_str());
  (void)engine->LoadPreset(preview.kept.presetId);
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
  const bool reopened = modal->cursor < modal->presets.size();
  const std::size_t previous = reopened ? modal->presets[modal->cursor] : 0;
  // The track's own preset, also while another one is on trial (PreviewPreset).
  const std::string kept = KeptPresetId();
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
      if (reopened ? offset + index == previous : preset.id == kept) modal->cursor = modal->presets.size();
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
  // Marked: the track's own preset, not one on trial (PreviewPreset).
  const std::string kept = KeptPresetId();
  for (std::size_t index = modal.firstItem; index < modal.firstItem + kListVisibleRows && index < modal.presets.size(); ++index)
  {
    const auto page = engine->ListPresets({modal.presets[index], 1});
    if (!page.items.empty()) modal.items.push_back({page.items.front().id, Upper(page.items.front().name), page.items.front().id == kept});
  }
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
  const std::string current = host_.Sampler().BuildUiState(state_.track).sampleFileName;
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
      modal->cursor = static_cast<std::size_t>(std::clamp(static_cast<int>(modal->cursor) + step, 0, count - 1));
      if (modal->kind == ModalKind::PresetBrowser) FillPresetRows(*modal);
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
  if (modal == nullptr || modal->kind != ModalKind::SampleBrowser || modal->items.empty()) return;
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
  const bool presets = modal->kind == ModalKind::PresetBrowser;
  CloseModal();
  // ESC leaves the track as the browser found it: the preset on trial goes, its own sound comes back.
  if (presets) EndPresetPreview(true);
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
    case TextPurpose::ProjectRename:
      CloseModal();
      RenameProjectNamed(payload, text);
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
      // The pattern it asked about, even if a queued switch came in the meantime.
      if (sequencer.ClearPatternEvents(state_.track, std::strtoul(payload.c_str(), nullptr, 10)))
      {
        MarkDirty();
        ShowToast("PATTERN CLEARED");
      }
      break;
    case ConfirmPurpose::DeletePattern:
      DeleteEmptyPattern(std::strtoul(payload.c_str(), nullptr, 10));
      break;
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
