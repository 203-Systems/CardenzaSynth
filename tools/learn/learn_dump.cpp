// Exports what the Learn manual documents straight from the firmware code, as JSON on stdout:
// every engine's INSTR pages with their GRID slots and parameter ranges, defaults and option names,
// the page tabs of each scene, the FN panel, the FN shortcuts and the keyboard choices.
// tools/learn/build.py runs it; the manual's reference tables are generated from its output.
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "Applications/Synth/GrooveApp.h"
#include "Applications/Synth/UI/Groove/GrooveController.h"
#include "Applications/Synth/UI/Groove/GrooveTypes.h"
#include "Applications/Synth/UI/Groove/InstrumentPages.h"
#include "OS/System/System.h"

namespace {

std::string Quote(const std::string& text)
{
  std::string out = "\"";
  for (const char c : text)
  {
    if (c == '"' || c == '\\') out += '\\';
    if (static_cast<unsigned char>(c) < 0x20) continue;
    out += c;
  }
  return out + "\"";
}

const char* KindName(synth::SynthParameterKind kind)
{
  switch (kind)
  {
    case synth::SynthParameterKind::Toggle: return "toggle";
    case synth::SynthParameterKind::Choice: return "choice";
    case synth::SynthParameterKind::Continuous:
    default: return "range";
  }
}

const char* ActionName(groove::SlotAction action)
{
  switch (action)
  {
    case groove::SlotAction::Engine: return "engine";
    case groove::SlotAction::Preset: return "preset";
    case groove::SlotAction::SavePreset: return "save";
    case groove::SlotAction::SavePresetAs: return "save-as";
    case groove::SlotAction::SampleFile: return "sample-file";
    case groove::SlotAction::SampleRecord: return "sample-record";
    case groove::SlotAction::SamplePreview: return "sample-preview";
    case groove::SlotAction::None:
    default: return "";
  }
}

const char* EngineName(synth::SynthEngineKind kind)
{
  switch (kind)
  {
    case synth::SynthEngineKind::Drums: return "DRUMS";
    case synth::SynthEngineKind::Juno: return "JUNO";
    case synth::SynthEngineKind::Dx7: return "DX7";
    case synth::SynthEngineKind::Sampler: return "SAMPLER";
    default: return "ENGINE";
  }
}

// One engine parameter: range, default, and for choices and switches every option's name.
std::string Parameter(synth::ISynthEngine& engine, const std::string& id)
{
  synth::SynthParameterInfo info;
  if (!engine.GetParameterInfo(id, info)) return "null";
  std::string out = "{\"kind\":" + Quote(KindName(info.kind)) + ",\"min\":" + std::to_string(info.minValue) +
                    ",\"max\":" + std::to_string(info.maxValue) + ",\"default\":" + std::to_string(info.defaultValue);
  const int original = info.value;
  (void)engine.SetParameterValue(id, info.defaultValue);
  out += ",\"defaultText\":" + Quote(engine.DescribeParameterValue(id));
  if (info.kind != synth::SynthParameterKind::Continuous && info.maxValue - info.minValue < 32)
  {
    out += ",\"options\":[";
    for (int value = info.minValue; value <= info.maxValue; ++value)
    {
      (void)engine.SetParameterValue(id, value);
      out += (value == info.minValue ? "" : ",") + Quote(engine.DescribeParameterValue(id));
    }
    out += "]";
  }
  else
  {
    (void)engine.SetParameterValue(id, info.minValue);
    out += ",\"minText\":" + Quote(engine.DescribeParameterValue(id));
    (void)engine.SetParameterValue(id, info.maxValue);
    out += ",\"maxText\":" + Quote(engine.DescribeParameterValue(id));
  }
  (void)engine.SetParameterValue(id, original);
  return out + "}";
}

std::string Page(const groove::InstrumentPage& page, synth::ISynthEngine* engine, const groove::Controller* settings)
{
  std::string out = "{\"tab\":" + Quote(page.tab) + ",\"name\":" + Quote(page.name) + ",\"slots\":[";
  bool first = true;
  for (std::size_t index = 0; index < page.slots.size(); ++index)
  {
    const groove::Slot& slot = page.slots[index];
    if (slot.Empty()) continue;
    out += first ? "" : ",";
    first = false;
    out += "{\"index\":" + std::to_string(index) + ",\"label\":" + Quote(slot.label == nullptr ? "" : slot.label);
    if (slot.IsAction()) out += ",\"action\":" + Quote(ActionName(slot.action));
    if (slot.id != nullptr) out += ",\"id\":" + Quote(slot.id);
    if (slot.IsParameter() && engine != nullptr && settings == nullptr) out += ",\"param\":" + Parameter(*engine, slot.id);
    if (slot.IsParameter() && settings != nullptr)
    {
      synth::SynthParameterInfo info;
      std::string text;
      if (settings->SettingValue(slot.id, info, text))
        out += ",\"param\":{\"kind\":" + Quote(KindName(info.kind)) + ",\"min\":" + std::to_string(info.minValue) +
               ",\"max\":" + std::to_string(info.maxValue) + ",\"default\":" + std::to_string(info.defaultValue) +
               ",\"valueText\":" + Quote(text) + "}";
    }
    out += "}";
  }
  return out + "]}";
}

template <typename List>
std::string Names(const List& list)
{
  std::string out = "[";
  for (std::size_t index = 0; index < list.size(); ++index) out += (index == 0 ? "" : ",") + Quote(list[index]);
  return out + "]";
}

}  // namespace

int main()
{
  const auto root = std::filesystem::temp_directory_path() / "cardenza-learn-dump";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  std::filesystem::current_path(root);
  if (!os::system::Begin()) return 1;
  auto* app = dynamic_cast<apps::GrooveApp*>(os::system::ActiveApplication());
  if (app == nullptr) return 1;
  const groove::Controller& ui = app->Ui();

  std::string out = "{\"engines\":[";
  for (std::size_t track = 0; track < groove::kTrackCount; ++track)
  {
    synth::ISynthEngine* engine = app->Engine(track);
    const synth::SynthEngineKind kind = app->EngineKind(track);
    const groove::InstrumentPageList pages = groove::InstrumentPagesFor(kind);
    out += (track == 0 ? "" : ",");
    out += "{\"engine\":" + Quote(EngineName(kind)) + ",\"track\":" + std::to_string(track + 1) +
           ",\"preset\":" + Quote(engine != nullptr ? engine->CurrentPresetName() : "") + ",\"pages\":[";
    for (std::size_t page = 0; page < pages.count; ++page) out += (page == 0 ? "" : ",") + Page(pages.pages[page], engine, nullptr);
    out += "]}";
  }
  out += "],\"scenes\":{";
  const std::array<std::pair<const char*, groove::Scene>, 3> scenes{{
      {"SEQ", groove::Scene::Sequence}, {"TAPE", groove::Scene::Tape}, {"SETUP", groove::Scene::Setup}}};
  for (std::size_t index = 0; index < scenes.size(); ++index)
  {
    out += (index == 0 ? "" : ",") + Quote(scenes[index].first) + ":[";
    for (std::size_t page = 0; page < ui.PageCount(scenes[index].second); ++page)
      out += (page == 0 ? "" : ",") + std::string("{\"tab\":") + Quote(ui.PageTab(scenes[index].second, page)) +
             ",\"name\":" + Quote(ui.PageName(scenes[index].second, page)) + "}";
    out += "]";
  }
  // The SETUP settings pages (KEYS, CHORD, ARP, SYSTEM) with their ranges.
  out += "},\"settingsPages\":[";
  for (std::size_t page = 0; page < groove::kSettingsPageCount; ++page)
    out += (page == 0 ? "" : ",") + Page(groove::SettingsPage(page), nullptr, &ui);
  out += "],\"fnEntries\":[";
  const auto entries = ui.FnEntries();
  for (std::size_t index = 0; index < entries.size(); ++index)
    out += (index == 0 ? "" : ",") + std::string("{\"key\":") + Quote(entries[index].key) + ",\"label\":" + Quote(entries[index].label) + "}";
  out += "],\"fnQuick\":[";
  for (std::size_t index = 0; index < groove::kFnQuickControls.size(); ++index)
  {
    const auto& quick = groove::kFnQuickControls[index];
    out += (index == 0 ? "" : ",") + std::string("{\"up\":") + Quote(std::string(1, quick.up)) + ",\"down\":" +
           Quote(std::string(1, quick.down)) + ",\"id\":" + Quote(quick.id) + ",\"label\":" + Quote(quick.label) + "}";
  }
  std::vector<std::string> scales;
  for (const auto& scale : groove::kScales) scales.push_back(scale.label);
  std::vector<std::string> arpModes(synth::kArpModeNames.begin(), synth::kArpModeNames.end());
  std::vector<std::string> arpRates(synth::kArpRateNames.begin(), synth::kArpRateNames.end());
  std::vector<std::string> chords(groove::kChordNames.begin(), groove::kChordNames.end());
  std::vector<std::string> voicings(groove::kVoicingNames.begin(), groove::kVoicingNames.end());
  std::vector<std::string> layouts(groove::kLayoutNames.begin(), groove::kLayoutNames.end());
  std::vector<std::string> locks(groove::kScaleLockNames.begin(), groove::kScaleLockNames.end());
  out += "],\"lists\":{\"scales\":" + Names(scales) + ",\"chords\":" + Names(chords) + ",\"voicings\":" + Names(voicings) +
         ",\"layouts\":" + Names(layouts) + ",\"locks\":" + Names(locks) + ",\"arpModes\":" + Names(arpModes) +
         ",\"arpRates\":" + Names(arpRates) + "}}";
  std::printf("%s\n", out.c_str());
  os::system::Shutdown();
  std::filesystem::current_path(std::filesystem::temp_directory_path());
  std::filesystem::remove_all(root);
  return 0;
}
