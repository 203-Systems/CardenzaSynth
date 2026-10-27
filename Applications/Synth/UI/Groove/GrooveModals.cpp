#include "Applications/Synth/UI/Groove/GrooveController.h"

#include <algorithm>
#include <cctype>

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
  const std::string previous = modal->cursor < modal->items.size() ? modal->items[modal->cursor].id : std::string{};
  modal->items.clear();
  constexpr std::size_t kPage = 32;
  for (std::size_t offset = 0;; offset += kPage)
  {
    const auto page = engine->ListPresets({offset, kPage});
    for (const auto& preset : page.items)
      if (Matches(preset.name, modal->search)) modal->items.push_back({preset.id, Upper(preset.name), preset.active});
    if (page.items.empty() || offset + kPage >= page.totalCount) break;
  }
  modal->cursor = 0;
  for (std::size_t index = 0; index < modal->items.size(); ++index)
  {
    if ((!previous.empty() && modal->items[index].id == previous) || (previous.empty() && modal->items[index].marked))
    {
      modal->cursor = index;
      break;
    }
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
      if (modal->items.empty()) return;
      const int count = static_cast<int>(modal->items.size());
      // Vertical lists: ADD (the upper key, drawn as an up arrow) moves up, SUB down. The engine
      // picker is a row: ADD moves right.
      const int direction = modal->kind == ModalKind::EnginePicker ? delta : -delta;
      const int step = direction * (coarse ? 8 : 1);
      modal->cursor = static_cast<std::size_t>(std::clamp(static_cast<int>(modal->cursor) + step, 0, count - 1));
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
      if (engine == nullptr || modal->items.empty()) return;
      const std::string id = modal->items[modal->cursor].id;
      CloseModal();
      ReleaseAllPads();
      if (engine->LoadPreset(id)) MarkDirty();
      else ShowToast("PRESET UNAVAILABLE", ToastKind::Error);
      return;
    }
    case ModalKind::EnginePicker:
    {
      if (modal->items.empty()) return;
      const auto kind = static_cast<synth::SynthEngineKind>(std::stoi(modal->items[modal->cursor].id));
      CloseModal();
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
    case ModalKind::TextEntry:
      if (modal->textPurpose == TextPurpose::SampleName)
      {
        sampler.CancelNameEntry(state_.track);
      }
      break;
    case ModalKind::Confirm:
      if (modal->confirmPurpose == ConfirmPurpose::OverwriteSample) sampler.CancelNameEntry(state_.track);
      break;
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
    case TextPurpose::SampleName:
    {
      const bool committed = sampler.CommitDraftName(state_.track, text);
      const auto state = sampler.BuildUiState(state_.track);
      if (state.overwritePending)
      {
        CloseModal();
        OpenConfirm(ConfirmPurpose::OverwriteSample, "REPLACE " + text + "?", "A SAMPLE WITH THIS NAME EXISTS", true);
        return;
      }
      if (!committed)
      {
        ShowToast(state.statusText, ToastKind::Error);
        return;
      }
      CloseModal();
      if (Top() != nullptr && Top()->kind == ModalKind::SampleRecord) CloseModal();
      MarkDirty();
      ShowToast("SAVED " + text, ToastKind::Ok);
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
    case ConfirmPurpose::OverwriteSample:
      if (sampler.ConfirmOverwriteDraft(state_.track))
      {
        if (Top() != nullptr && Top()->kind == ModalKind::SampleRecord) CloseModal();
        MarkDirty();
        ShowToast("SAMPLE REPLACED", ToastKind::Ok);
      }
      else ShowToast(sampler.BuildUiState(state_.track).statusText, ToastKind::Error);
      break;
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
