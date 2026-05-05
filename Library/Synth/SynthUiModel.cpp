#include "Library/Synth/SynthUiModel.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <sstream>
#include <utility>

namespace synth {

namespace {
constexpr std::size_t kPagedMenuItemCount = kContextMenuCellCount - 1;
constexpr float kTransitionStep = 0.18f;
constexpr auto kModifierHoldDelay = std::chrono::milliseconds(200);
constexpr std::array<SynthEngineKind, 3> kSelectableEngineKinds = {
  SynthEngineKind::Dx7,
  SynthEngineKind::Juno,
  SynthEngineKind::Sampler,
};
constexpr std::array<const char*, 4> kModifierBarLabels = {"TRK", "SND", "LAY", "SYS"};
constexpr std::array<UiBarIcon, 4> kHomeActionBarIcons = {
  UiBarIcon::Plus,
  UiBarIcon::Minus,
  UiBarIcon::Circle,
  UiBarIcon::Play,
};
constexpr std::size_t kPresetBrowserVisibleRows = 7;
constexpr std::size_t kPresetBrowserPageSize = 32;
constexpr std::size_t kPresetSearchMaxLength = 18;

constexpr std::array<std::array<const char*, kKeyboardMatrixColumns>, kKeyboardMatrixRows> kPresetSearchKeyMap = {{
    {{"", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", ""}},
    {{"", "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[", "]", ""}},
    {{"", "", "a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "", ""}},
  {{"", "", "", "z", "x", "c", "v", "b", "n", "m", ",", ".", "", ""}},
}};

const char* ModifierDisplayName(UiModifier modifier)
{
  switch (modifier)
  {
    case UiModifier::Sound:
      return "Track";
    case UiModifier::Parameter:
      return "Sound";
    case UiModifier::Layout:
      return "Layout";
    case UiModifier::Other:
      return "System";
    case UiModifier::None:
    default:
      return "Home";
  }
}

UiModifier ModifierForRow(std::size_t row)
{
  switch (row)
  {
    case 0:
      return UiModifier::Sound;
    case 1:
      return UiModifier::Parameter;
    case 2:
      return UiModifier::Layout;
    case 3:
      return UiModifier::Other;
    default:
      return UiModifier::None;
  }
}

std::size_t RowForModifier(UiModifier modifier)
{
  switch (modifier)
  {
    case UiModifier::Sound:
      return 0;
    case UiModifier::Parameter:
      return 1;
    case UiModifier::Layout:
      return 2;
    case UiModifier::Other:
      return 3;
    case UiModifier::None:
    default:
      return 0;
  }
}

ContextMenuCell MakeItemCell(const std::string& label, const std::string& itemId)
{
  ContextMenuCell cell;
  cell.label = label;
  cell.type = MenuCellType::Item;
  cell.itemId = itemId;
  return cell;
}

ContextMenuCell MakeActionCell(const std::string& label, ContextActionId actionId)
{
  ContextMenuCell cell;
  cell.label = label;
  cell.type = MenuCellType::UtilityAction;
  cell.actionId = actionId;
  return cell;
}

ContextMenuCell MakeNextPageCell(const std::string& label)
{
  ContextMenuCell cell;
  cell.label = label;
  cell.type = MenuCellType::NextPage;
  return cell;
}

ContextMenuGrid BlankGrid()
{
  return ContextMenuGrid{};
}

SynthEngineKind StepSelectableEngineKind(SynthEngineKind currentKind, int delta)
{
  auto iterator = std::find(kSelectableEngineKinds.begin(), kSelectableEngineKinds.end(), currentKind);
  std::size_t index = iterator != kSelectableEngineKinds.end() ? static_cast<std::size_t>(iterator - kSelectableEngineKinds.begin()) : 0;
  index = static_cast<std::size_t>((static_cast<int>(index) + delta + static_cast<int>(kSelectableEngineKinds.size())) %
                                   static_cast<int>(kSelectableEngineKinds.size()));
  return kSelectableEngineKinds[index];
}

std::string ToLower(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

bool ContainsCaseInsensitive(const std::string& haystack, const std::string& needle)
{
  return needle.empty() || ToLower(haystack).find(needle) != std::string::npos;
}

std::vector<SynthPresetInfo> CollectPresetMatches(const ISynthEngine* engine, const std::string& searchText)
{
  if (engine == nullptr)
  {
    return {};
  }

  const std::string normalizedSearch = ToLower(searchText);
  std::vector<SynthPresetInfo> matches;
  for (std::size_t offset = 0;;)
  {
    const SynthPresetPage page = engine->ListPresets({offset, kPresetBrowserPageSize});
    for (const SynthPresetInfo& preset : page.items)
    {
      if (ContainsCaseInsensitive(preset.name, normalizedSearch))
      {
        matches.push_back(preset);
      }
    }

    offset += page.items.size();
    if (page.items.empty() || offset >= page.totalCount)
    {
      break;
    }
  }

  return matches;
}

std::size_t ActivePresetMatchIndex(const std::vector<SynthPresetInfo>& presets)
{
  const auto iterator = std::find_if(presets.begin(), presets.end(), [](const SynthPresetInfo& preset) {
    return preset.active;
  });
  return iterator != presets.end() ? static_cast<std::size_t>(std::distance(presets.begin(), iterator)) : 0;
}

std::string BuildPresetBrowserListText(const std::vector<SynthPresetInfo>& presets, std::size_t selectedIndex)
{
  if (presets.empty())
  {
    return "No presets";
  }

  const std::size_t clampedSelection = std::min(selectedIndex, presets.size() - 1);
  const std::size_t firstVisible = clampedSelection == 0 ? 0 : clampedSelection - 1;
  std::ostringstream stream;
  const std::size_t lastVisible = std::min(presets.size(), firstVisible + kPresetBrowserVisibleRows);
  for (std::size_t index = firstVisible; index < lastVisible; ++index)
  {
    if (index > firstVisible)
    {
      stream << '\n';
    }

    stream << (index == clampedSelection ? ">" : " ") << presets[index].name;
  }

  return stream.str();
}

bool ReadParameter(const ISynthEngine* engine, const std::string& parameterId, SynthParameterInfo& parameter)
{
  return engine != nullptr && engine->GetParameterInfo(parameterId, parameter);
}

int ParameterValueOrDefault(const ISynthEngine* engine, const std::string& parameterId, int fallback)
{
  SynthParameterInfo parameter;
  return ReadParameter(engine, parameterId, parameter) ? parameter.value : fallback;
}

std::string DescribeParameterValueOrFallback(const ISynthEngine* engine,
                                             const std::string& parameterId,
                                             const std::string& fallback = "--")
{
  return engine != nullptr ? engine->DescribeParameterValue(parameterId) : fallback;
}

std::string FormatDx7OperatorSummary(const ISynthEngine* engine, int op)
{
  const std::string prefix = "op" + std::to_string(op) + ".";
  return "OUT " + DescribeParameterValueOrFallback(engine, prefix + "output") +
         " CRS " + DescribeParameterValueOrFallback(engine, prefix + "coarse");
}

std::string FormatDx7AlgorithmSummary(const ISynthEngine* engine)
{
  return DescribeParameterValueOrFallback(engine, "global.algorithm", "ALG --") +
         " FB " + DescribeParameterValueOrFallback(engine, "global.feedback");
}

std::string FormatDx7GlobalSummary(const ISynthEngine* engine)
{
  return "TR " + DescribeParameterValueOrFallback(engine, "global.transpose") +
         " LFO " + DescribeParameterValueOrFallback(engine, "global.lfo_speed");
}

bool IsDx7ParameterEngine(const ISynthEngine* engine)
{
  return engine != nullptr && engine->Descriptor().kind == SynthEngineKind::Dx7;
}

bool IsJunoParameterEngine(const ISynthEngine* engine)
{
  return engine != nullptr && engine->Descriptor().kind == SynthEngineKind::Juno;
}

bool IsDx7ParameterGroupId(const std::string& itemId)
{
  return itemId == "par.dx7.group.op1" || itemId == "par.dx7.group.op2" || itemId == "par.dx7.group.op3" ||
         itemId == "par.dx7.group.op4" || itemId == "par.dx7.group.op5" || itemId == "par.dx7.group.op6" ||
         itemId == "par.dx7.group.alg" || itemId == "par.dx7.group.glb";
}

std::string Dx7GroupLabel(const std::string& groupId)
{
  if (groupId == "par.dx7.group.op1") return "Operator 1";
  if (groupId == "par.dx7.group.op2") return "Operator 2";
  if (groupId == "par.dx7.group.op3") return "Operator 3";
  if (groupId == "par.dx7.group.op4") return "Operator 4";
  if (groupId == "par.dx7.group.op5") return "Operator 5";
  if (groupId == "par.dx7.group.op6") return "Operator 6";
  if (groupId == "par.dx7.group.alg") return "Algorithm";
  if (groupId == "par.dx7.group.glb") return "Global";
  return "DX7";
}

std::string Dx7GroupSummary(const ISynthEngine* engine, const std::string& groupId)
{
  if (groupId == "par.dx7.group.op1") return FormatDx7OperatorSummary(engine, 1);
  if (groupId == "par.dx7.group.op2") return FormatDx7OperatorSummary(engine, 2);
  if (groupId == "par.dx7.group.op3") return FormatDx7OperatorSummary(engine, 3);
  if (groupId == "par.dx7.group.op4") return FormatDx7OperatorSummary(engine, 4);
  if (groupId == "par.dx7.group.op5") return FormatDx7OperatorSummary(engine, 5);
  if (groupId == "par.dx7.group.op6") return FormatDx7OperatorSummary(engine, 6);
  if (groupId == "par.dx7.group.alg") return FormatDx7AlgorithmSummary(engine);
  if (groupId == "par.dx7.group.glb") return FormatDx7GlobalSummary(engine);
  return "SELECT GROUP";
}

std::string Dx7ParameterMenuLabel(const SynthParameterInfo& parameter)
{
  if (parameter.id == "global.algorithm") return "ALG";
  if (parameter.id == "global.feedback") return "FB";
  if (parameter.id == "global.transpose") return "TRNS";
  if (parameter.id == "global.lfo_speed") return "SPD";
  if (parameter.id == "global.lfo_delay") return "DLY";
  if (parameter.id == "global.lfo_pitch") return "PMOD";
  if (parameter.id == "global.lfo_amp") return "AMOD";
  if (parameter.label == "Output") return "OUT";
  if (parameter.label == "Coarse") return "CRS";
  if (parameter.label == "Fine") return "FINE";
  if (parameter.label == "Detune") return "DTN";
  if (parameter.label == "Attack") return "ATK";
  if (parameter.label == "Decay") return "DEC";
  if (parameter.label == "Sustain") return "SUS";
  if (parameter.label == "Release") return "REL";
  if (parameter.label == "Velocity") return "VEL";
  if (parameter.label == "Amp Mod") return "AMOD";
  return parameter.label;
}

bool IsJunoParameterGroupId(const std::string& itemId)
{
  return itemId == "par.juno.group.dco" || itemId == "par.juno.group.vcf" || itemId == "par.juno.group.env" ||
         itemId == "par.juno.group.lfo" || itemId == "par.juno.group.amp" || itemId == "par.juno.group.fx";
}

std::string JunoGroupIdForParameterId(const std::string& itemId)
{
  if (itemId.rfind("dco.", 0) == 0) return "par.juno.group.dco";
  if (itemId.rfind("vcf.", 0) == 0 || itemId == "hpf.mode") return "par.juno.group.vcf";
  if (itemId.rfind("env.", 0) == 0) return "par.juno.group.env";
  if (itemId.rfind("lfo.", 0) == 0) return "par.juno.group.lfo";
  if (itemId.rfind("vca.", 0) == 0) return "par.juno.group.amp";
  if (itemId.rfind("chorus.", 0) == 0) return "par.juno.group.fx";
  return {};
}

std::string JunoGroupLabel(const std::string& groupId)
{
  if (groupId == "par.juno.group.dco") return "DCO";
  if (groupId == "par.juno.group.vcf") return "VCF";
  if (groupId == "par.juno.group.env") return "ENV";
  if (groupId == "par.juno.group.lfo") return "LFO";
  if (groupId == "par.juno.group.amp") return "AMP";
  if (groupId == "par.juno.group.fx") return "FX";
  return "Juno";
}

std::string JunoGroupTitle(const std::string& groupId)
{
  if (groupId == "par.juno.group.dco") return "Oscillator";
  if (groupId == "par.juno.group.vcf") return "Filter";
  if (groupId == "par.juno.group.env") return "Envelope";
  if (groupId == "par.juno.group.lfo") return "Modulation";
  if (groupId == "par.juno.group.amp") return "Amplifier";
  if (groupId == "par.juno.group.fx") return "Effects";
  return "Sections";
}

std::string JunoParameterMenuLabel(const SynthParameterInfo& parameter)
{
  if (parameter.id == "dco.range") return "RNG";
  if (parameter.id == "dco.pwm") return "PWM";
  if (parameter.id == "dco.saw") return "SAW";
  if (parameter.id == "dco.sub") return "SUB";
  if (parameter.id == "dco.noise") return "NOI";
  if (parameter.id == "vcf.cutoff") return "CUT";
  if (parameter.id == "vcf.resonance") return "RES";
  if (parameter.id == "vcf.env") return "ENV";
  if (parameter.id == "vcf.lfo") return "LFO";
  if (parameter.id == "hpf.mode") return "HPF";
  if (parameter.id == "env.attack") return "ATK";
  if (parameter.id == "env.decay") return "DEC";
  if (parameter.id == "env.sustain") return "SUS";
  if (parameter.id == "env.release") return "REL";
  if (parameter.id == "lfo.rate") return "RATE";
  if (parameter.id == "lfo.delay") return "DLY";
  if (parameter.id == "vca.level") return "LVL";
  if (parameter.id == "chorus.mode") return "CHO";
  return parameter.label;
}

std::vector<SynthParameterInfo> CollectJunoGroupParameters(const ISynthEngine* engine, const std::string& groupId)
{
  std::vector<SynthParameterInfo> parameters;
  if (engine == nullptr)
  {
    return parameters;
  }

  auto appendIfPresent = [&](const std::string& parameterId) {
    SynthParameterInfo parameter;
    if (engine->GetParameterInfo(parameterId, parameter))
    {
      parameters.push_back(parameter);
    }
  };

  if (groupId == "par.juno.group.dco")
  {
    appendIfPresent("dco.range");
    appendIfPresent("dco.pwm");
    appendIfPresent("dco.saw");
    appendIfPresent("dco.sub");
    appendIfPresent("dco.noise");
    return parameters;
  }

  if (groupId == "par.juno.group.vcf")
  {
    appendIfPresent("vcf.cutoff");
    appendIfPresent("vcf.resonance");
    appendIfPresent("vcf.env");
    appendIfPresent("vcf.lfo");
    appendIfPresent("hpf.mode");
    return parameters;
  }

  if (groupId == "par.juno.group.env")
  {
    appendIfPresent("env.attack");
    appendIfPresent("env.decay");
    appendIfPresent("env.sustain");
    appendIfPresent("env.release");
    return parameters;
  }

  if (groupId == "par.juno.group.lfo")
  {
    appendIfPresent("lfo.rate");
    appendIfPresent("lfo.delay");
    return parameters;
  }

  if (groupId == "par.juno.group.amp")
  {
    appendIfPresent("vca.level");
    return parameters;
  }

  if (groupId == "par.juno.group.fx")
  {
    appendIfPresent("chorus.mode");
  }

  return parameters;
}

std::string JunoGroupSummary(const ISynthEngine* engine, const std::string& groupId)
{
  if (groupId == "par.juno.group.dco")
  {
    return "RNG " + DescribeParameterValueOrFallback(engine, "dco.range") +
           "  PWM " + DescribeParameterValueOrFallback(engine, "dco.pwm");
  }
  if (groupId == "par.juno.group.vcf")
  {
    return "CUT " + DescribeParameterValueOrFallback(engine, "vcf.cutoff") +
           "  RES " + DescribeParameterValueOrFallback(engine, "vcf.resonance");
  }
  if (groupId == "par.juno.group.env")
  {
    return "A " + DescribeParameterValueOrFallback(engine, "env.attack") +
           "  R " + DescribeParameterValueOrFallback(engine, "env.release");
  }
  if (groupId == "par.juno.group.lfo")
  {
    return "RATE " + DescribeParameterValueOrFallback(engine, "lfo.rate") +
           "  DLY " + DescribeParameterValueOrFallback(engine, "lfo.delay");
  }
  if (groupId == "par.juno.group.amp")
  {
    return "LVL " + DescribeParameterValueOrFallback(engine, "vca.level");
  }
  if (groupId == "par.juno.group.fx")
  {
    return "CHO " + DescribeParameterValueOrFallback(engine, "chorus.mode");
  }
  return "SELECT SECTION";
}

std::string JunoOverviewSummary(const ISynthEngine* engine)
{
  return "CUT " + DescribeParameterValueOrFallback(engine, "vcf.cutoff") +
         "  RES " + DescribeParameterValueOrFallback(engine, "vcf.resonance") +
         "  CHO " + DescribeParameterValueOrFallback(engine, "chorus.mode");
}

bool IsJunoDirectAdjustGroupId(const std::string& groupId)
{
  return groupId == "par.juno.group.amp";
}

bool IsJunoDirectActionGroupId(const std::string& groupId)
{
  return groupId == "par.juno.group.fx";
}

constexpr std::size_t kJunoMenuPageCapacity = 6;

void PopulateJunoSnapshotValues(UiScreenSnapshot& snapshot, const ISynthEngine* engine)
{
  snapshot.junoDcoRange = ParameterValueOrDefault(engine, "dco.range", 1);
  snapshot.junoDcoPwm = ParameterValueOrDefault(engine, "dco.pwm", 0);
  snapshot.junoDcoSub = ParameterValueOrDefault(engine, "dco.sub", 0);
  snapshot.junoDcoNoise = ParameterValueOrDefault(engine, "dco.noise", 0);
  snapshot.junoDcoSaw = ParameterValueOrDefault(engine, "dco.saw", 0);
  snapshot.junoVcfCutoff = ParameterValueOrDefault(engine, "vcf.cutoff", 0);
  snapshot.junoVcfResonance = ParameterValueOrDefault(engine, "vcf.resonance", 0);
  snapshot.junoVcfEnv = ParameterValueOrDefault(engine, "vcf.env", 0);
  snapshot.junoVcfLfo = ParameterValueOrDefault(engine, "vcf.lfo", 0);
  snapshot.junoHpfMode = ParameterValueOrDefault(engine, "hpf.mode", 0);
  snapshot.junoVcaLevel = ParameterValueOrDefault(engine, "vca.level", 0);
  snapshot.junoEnvAttack = ParameterValueOrDefault(engine, "env.attack", 0);
  snapshot.junoEnvDecay = ParameterValueOrDefault(engine, "env.decay", 0);
  snapshot.junoEnvSustain = ParameterValueOrDefault(engine, "env.sustain", 0);
  snapshot.junoEnvRelease = ParameterValueOrDefault(engine, "env.release", 0);
  snapshot.junoLfoRate = ParameterValueOrDefault(engine, "lfo.rate", 0);
  snapshot.junoLfoDelay = ParameterValueOrDefault(engine, "lfo.delay", 0);
  snapshot.junoChorusMode = ParameterValueOrDefault(engine, "chorus.mode", 0);
}

std::size_t JunoParameterPageCount(const ISynthEngine* engine, const std::string& groupId)
{
  const std::size_t count = CollectJunoGroupParameters(engine, groupId).size();
  return std::max<std::size_t>(1, (count + kJunoMenuPageCapacity - 1) / kJunoMenuPageCapacity);
}

std::vector<SynthParameterInfo> CollectDx7GroupParameters(const ISynthEngine* engine, const std::string& groupId)
{
  std::vector<SynthParameterInfo> parameters;
  if (engine == nullptr)
  {
    return parameters;
  }

  auto appendIfPresent = [&](const std::string& parameterId) {
    SynthParameterInfo parameter;
    if (engine->GetParameterInfo(parameterId, parameter))
    {
      parameters.push_back(parameter);
    }
  };

  if (groupId == "par.dx7.group.alg")
  {
    appendIfPresent("global.algorithm");
    appendIfPresent("global.feedback");
    return parameters;
  }

  if (groupId == "par.dx7.group.glb")
  {
    appendIfPresent("global.transpose");
    appendIfPresent("global.lfo_speed");
    appendIfPresent("global.lfo_delay");
    appendIfPresent("global.lfo_pitch");
    appendIfPresent("global.lfo_amp");
    return parameters;
  }

  const SynthParameterPage page = engine->ListParameters({0, 128});
  const std::string groupLabel = Dx7GroupLabel(groupId);
  for (const SynthParameterInfo& parameter : page.items)
  {
    if (parameter.group == groupLabel)
    {
      parameters.push_back(parameter);
    }
  }
  return parameters;
}

std::size_t Dx7ParameterPageCount(const ISynthEngine* engine, const std::string& groupId)
{
  constexpr std::size_t kPageCapacity = 6;
  const std::size_t count = CollectDx7GroupParameters(engine, groupId).size();
  return std::max<std::size_t>(1, (count + kPageCapacity - 1) / kPageCapacity);
}

std::size_t Dx7ParameterMenuIndex(const ISynthEngine* engine,
                                  const std::string& groupId,
                                  const std::string& parameterId,
                                  std::size_t pageIndex)
{
  constexpr std::size_t kPageCapacity = 6;
  const auto parameters = CollectDx7GroupParameters(engine, groupId);
  const auto iterator = std::find_if(parameters.begin(), parameters.end(), [&](const SynthParameterInfo& parameter) {
    return parameter.id == parameterId;
  });
  if (iterator == parameters.end())
  {
    return 0;
  }

  const std::size_t absoluteIndex = static_cast<std::size_t>(std::distance(parameters.begin(), iterator));
  const std::size_t pageStart = pageIndex * kPageCapacity;
  if (absoluteIndex < pageStart || absoluteIndex >= (pageStart + kPageCapacity))
  {
    return 0;
  }

  return absoluteIndex - pageStart;
}

std::pair<std::string, std::string> MenuItemPresentation(UiModifier modifier,
                                                         const std::string& itemId,
                                                         const std::string& patchName,
                                                         const ISynthEngine* engine)
{
  const std::string engineName = engine != nullptr ? engine->Descriptor().displayName : "SYNTH";

  if (modifier == UiModifier::Sound)
  {
    if (itemId == "snd.dx7.op1") return {"Operator 1", FormatDx7OperatorSummary(engine, 1)};
    if (itemId == "snd.dx7.op2") return {"Operator 2", FormatDx7OperatorSummary(engine, 2)};
    if (itemId == "snd.dx7.op3") return {"Operator 3", FormatDx7OperatorSummary(engine, 3)};
    if (itemId == "snd.dx7.op4") return {"Operator 4", FormatDx7OperatorSummary(engine, 4)};
    if (itemId == "snd.dx7.op5") return {"Operator 5", FormatDx7OperatorSummary(engine, 5)};
    if (itemId == "snd.dx7.op6") return {"Operator 6", FormatDx7OperatorSummary(engine, 6)};
    if (itemId == "snd.dx7.alg") return {"Algorithm", FormatDx7AlgorithmSummary(engine)};
    if (itemId == "snd.dx7.glb") return {"Global", FormatDx7GlobalSummary(engine)};
    if (itemId == "snd.engine") return {"Engine", engineName};
    if (itemId == "snd.preset") return {"Preset", patchName};
    if (itemId == "snd.volume") return {"Volume", "92"};
    if (itemId == "snd.filter") return {"Filter", "72"};
  }

  if (modifier == UiModifier::Parameter)
  {
    if (itemId == "par.prev") return {"Previous Parameter", "STEP"};
    if (itemId == "par.next") return {"Next Parameter", "STEP"};
    if (itemId == "par.fine") return {"Fine Adjust", "+12"};
    if (itemId == "par.rst") return {"Reset", "DEFAULT"};
    if (itemId == "par.min") return {"Minimum", "0"};
    if (itemId == "par.max") return {"Maximum", "127"};
    if (itemId == "par.lock") return {"Lock", "OFF"};
    if (itemId == "par.m1") return {"Macro 1", "UNMAPPED"};
    if (itemId == "par.m2") return {"Macro 2", "UNMAPPED"};
    if (itemId == "par.m3") return {"Macro 3", "UNMAPPED"};
    if (itemId == "par.m4") return {"Macro 4", "UNMAPPED"};
    if (itemId == "par.map") return {"Map Macro", "READY"};
    if (itemId == "par.clr") return {"Clear Macro", "READY"};
    if (itemId == "par.save") return {"Save Mapping", "READY"};
  }

  if (modifier == UiModifier::Layout)
  {
    if (itemId == "lay.scl") return {"Scale", "C MIN"};
    if (itemId == "lay.root") return {"Root", "C"};
    if (itemId == "lay.mode") return {"Mode", "IONIAN"};
    if (itemId == "lay.chrd") return {"Chord", "TRIAD"};
    if (itemId == "lay.oct") return {"Octave", "+0"};
    if (itemId == "lay.trns") return {"Transpose", "0"};
    if (itemId == "lay.key") return {"Key Layout", "GRID"};
    if (itemId == "lay.grid") return {"Grid", "4X14"};
    if (itemId == "lay.keyb") return {"Keyboard", "PIANO"};
    if (itemId == "lay.drum") return {"Drum", "OFF"};
    if (itemId == "lay.mpe") return {"MPE", "OFF"};
    if (itemId == "lay.latch") return {"Latch", "OFF"};
    if (itemId == "lay.arp") return {"Arp", "OFF"};
    if (itemId == "lay.save") return {"Save Layout", "DONE"};
  }

  if (modifier == UiModifier::Other)
  {
    if (itemId == "sys.save") return {"Save", patchName};
    if (itemId == "sys.load") return {"Load", "BROWSE"};
    if (itemId == "sys.midi") return {"MIDI", "LOCAL"};
    if (itemId == "sys.sync") return {"Sync", "INT"};
    if (itemId == "sys.usb") return {"USB", "DEVICE"};
    if (itemId == "sys.sd") return {"SD Card", "READY"};
    if (itemId == "sys.set") return {"System", "80%"};
    if (itemId == "sys.brgt") return {"Brightness", "80%"};
    if (itemId == "sys.info") return {"Info", engineName};
    if (itemId == "sys.cal") return {"Calibration", "READY"};
    if (itemId == "sys.test") return {"Test", "OK"};
    if (itemId == "sys.rst") return {"Reset", "HOLD"};
    if (itemId == "sys.upd") return {"Update", "IDLE"};
    if (itemId == "sys.back") return {"Back", "HOME"};
  }

  return {"READY", patchName.empty() ? engineName : patchName};
}

}  // namespace

SynthUiModel::SynthUiModel() = default;

void SynthUiModel::InitializeTrack(std::size_t trackIndex, SynthEngineKind kind)
{
  if (trackIndex >= tracks_.size())
  {
    return;
  }

  tracks_[trackIndex].kind = kind;
  tracks_[trackIndex].patchName = ToString(kind);
  tracks_[trackIndex].looperProgress = 0.0f;
}

void SynthUiModel::SyncTrackState(std::size_t trackIndex, const ISynthEngine* engine)
{
  if (trackIndex >= tracks_.size() || engine == nullptr)
  {
    return;
  }

  tracks_[trackIndex].kind = engine->Descriptor().kind;

  if (!engine->CurrentPresetName().empty())
  {
    tracks_[trackIndex].patchName = engine->CurrentPresetName();
  }
}

void SynthUiModel::SetActiveTrack(std::size_t trackIndex)
{
  if (trackIndex >= tracks_.size())
  {
    return;
  }

  activeTrack_ = trackIndex;
}

void SynthUiModel::Tick(const ISynthEngine* engine)
{
  const auto now = std::chrono::steady_clock::now();
  for (std::size_t row = 0; row < heldModifiers_.size(); ++row)
  {
    if (!heldModifiers_[row] || modifierContextActivated_[row])
    {
      continue;
    }

    if ((now - modifierPressStartedAt_[row]) < kModifierHoldDelay)
    {
      continue;
    }

    const UiScreenSnapshot from = BuildScreenSnapshot(engine);
    ActivateModifier(ModifierForRow(row));
    modifierContextActivated_[row] = true;
    StartTransition(from, BuildScreenSnapshot(engine));
    break;
  }

  if (transition_.active)
  {
    transition_.progress = std::min(1.0f, transition_.progress + kTransitionStep);
    if (transition_.progress >= 1.0f)
    {
      transition_.active = false;
    }
  }
}

MatrixKeyHandling SynthUiModel::PressMatrixKey(ISynthEngine* engine, std::size_t row, std::size_t col)
{
  if (row >= kKeyboardMatrixRows || col >= kKeyboardMatrixColumns)
  {
    return MatrixKeyHandling::Ignored;
  }

  if (col == kHomeLeftBarColumn)
  {
    if (mode_ == UiMode::ContextMenu && contextLatched_)
    {
      const UiScreenSnapshot from = BuildScreenSnapshot(engine);
      ClearHeldModifiers();
      ExitToHome();
      StartTransition(from, BuildScreenSnapshot(engine));
      return MatrixKeyHandling::Ignored;
    }

    if (!heldModifiers_[row])
    {
      heldModifiers_[row] = true;
      modifierContextActivated_[row] = false;
      modifierPressStartedAt_[row] = std::chrono::steady_clock::now();
    }
    return MatrixKeyHandling::ModifierHold;
  }

  if (mode_ == UiMode::Home && col == kHomeRightBarColumn)
  {
    heldHomeActionRows_[row] = true;
    return MatrixKeyHandling::Ignored;
  }

  const std::size_t contextMenuColumns = ContextMenuColumnCount();
  const std::size_t contextMenuFirstColumn = contextMenuColumns == 1 ? kHomeRightBarColumn : kContextMenuFirstColumn;
  if (mode_ == UiMode::ContextMenu && presetBrowserMode_ && activeModifier_ == UiModifier::Sound &&
      col > kHomeLeftBarColumn && col < contextMenuFirstColumn)
  {
    heldContextMenuIndex_ = -1;
    hasMenuHighlight_ = false;

    const char* mappedValue = kPresetSearchKeyMap[row][col];
    if (mappedValue != nullptr && mappedValue[0] != '\0' && presetSearchText_.size() < kPresetSearchMaxLength)
    {
      presetSearchText_ += mappedValue;
      presetBrowserSelectionIndex_ = 0;
    }
    return MatrixKeyHandling::Ignored;
  }

  if (mode_ == UiMode::ContextMenu &&
      col >= contextMenuFirstColumn &&
      col < contextMenuFirstColumn + contextMenuColumns)
  {
    const auto menu = CurrentMenuCells(engine);
    const std::size_t menuIndex = contextMenuColumns == 1 ? row : row * kContextMenuColumns + (col - contextMenuFirstColumn);
    heldContextMenuIndex_ = static_cast<int>(menuIndex);
    if (menuIndex < menu.size())
    {
      ApplyMenuCell(engine, menu[menuIndex], menuIndex);
    }
    return MatrixKeyHandling::MenuAction;
  }

  if (mode_ == UiMode::Home && (col < kHomePerformanceFirstColumn || col > kHomePerformanceLastColumn))
  {
    return MatrixKeyHandling::Ignored;
  }

  return MatrixKeyHandling::PerformanceKey;
}

void SynthUiModel::ReleaseMatrixKey(const ISynthEngine* engine, std::size_t row, std::size_t col)
{
  if (row >= kKeyboardMatrixRows)
  {
    return;
  }

  const std::size_t contextMenuColumns = ContextMenuColumnCount();
  const std::size_t contextMenuFirstColumn = contextMenuColumns == 1 ? kHomeRightBarColumn : kContextMenuFirstColumn;
  if (mode_ == UiMode::ContextMenu &&
      col >= contextMenuFirstColumn &&
      col < contextMenuFirstColumn + contextMenuColumns)
  {
    heldContextMenuIndex_ = -1;
    hasMenuHighlight_ = false;
    return;
  }

  if (col == kHomeRightBarColumn)
  {
    heldHomeActionRows_[row] = false;
    return;
  }

  if (col != kHomeLeftBarColumn || !heldModifiers_[row])
  {
    return;
  }

  const bool activatedContext = modifierContextActivated_[row];
  const auto heldDuration = std::chrono::steady_clock::now() - modifierPressStartedAt_[row];
  heldModifiers_[row] = false;
  modifierContextActivated_[row] = false;

  if (!activatedContext)
  {
    if (heldDuration < kModifierHoldDelay && row < tracks_.size())
    {
      SetActiveTrack(row);
      CancelTransition();
    }
    return;
  }

  if (contextLatched_)
  {
    return;
  }

  const UiScreenSnapshot from = BuildScreenSnapshot(engine);
  ExitToHome();
  StartTransition(from, BuildScreenSnapshot(engine));
}

void SynthUiModel::ExitToHome()
{
  mode_ = UiMode::Home;
  activeModifier_ = UiModifier::None;
  currentPage_ = 0;
  activeMenuRow_ = 0;
  activeMenuColumn_ = 0;
  heldContextMenuIndex_ = -1;
  enginePickerMode_ = false;
  hasPendingEngineKind_ = false;
  pendingEngineKind_ = SynthEngineKind::None;
  presetBrowserMode_ = false;
  presetSearchText_.clear();
  presetBrowserSelectionIndex_ = 0;
  contextLatched_ = false;
  selectedParameterGroupId_.clear();
  ClearContextSelection();
}

std::string SynthUiModel::HeaderEngine(const ISynthEngine* engine) const
{
  if (engine != nullptr)
  {
    return engine->Descriptor().displayName;
  }
  return ToString(tracks_[activeTrack_].kind);
}

std::string SynthUiModel::HeaderTrack() const
{
  return "Track " + std::to_string(activeTrack_ + 1);
}

const std::string& SynthUiModel::PatchName() const
{
  return tracks_[activeTrack_].patchName;
}

float SynthUiModel::LooperProgress() const
{
  return tracks_[activeTrack_].looperProgress;
}

std::string SynthUiModel::ContextSectionTitle() const
{
  if (mode_ != UiMode::ContextMenu)
  {
    return "HOME";
  }

  switch (activeModifier_)
  {
    case UiModifier::Sound:
      return "SOUND";
    case UiModifier::Parameter:
      return "PARAMETER";
    case UiModifier::Layout:
      return "LAYOUT";
    case UiModifier::Other:
      return "SYSTEM";
    case UiModifier::None:
    default:
      return "HOME";
  }
}

std::string SynthUiModel::ContextParameterName(const ISynthEngine* engine) const
{
  (void)engine;
  if (mode_ != UiMode::ContextMenu || !hasContextSelection_)
  {
    return "";
  }

  switch (selectionKind_)
  {
    case ContextSelectionKind::Preset:
      return "Preset";
    case ContextSelectionKind::Parameter:
    case ContextSelectionKind::Status:
      return selectedItemLabel_;
    case ContextSelectionKind::None:
    default:
      return "";
  }
}

std::string SynthUiModel::ContextValueText(const ISynthEngine* engine) const
{
  if (mode_ != UiMode::ContextMenu || !hasContextSelection_)
  {
    return "";
  }

  switch (selectionKind_)
  {
    case ContextSelectionKind::Preset:
      return engine != nullptr ? engine->CurrentPresetName() : PatchName();
    case ContextSelectionKind::Parameter:
      return engine != nullptr ? engine->DescribeParameterValue(selectedItemId_) : "--";
    case ContextSelectionKind::Status:
      return selectedValueText_;
    case ContextSelectionKind::None:
    default:
      return "";
  }
}

ContextMenuGrid SynthUiModel::CurrentMenuCells(const ISynthEngine* engine) const
{
  if (mode_ != UiMode::ContextMenu)
  {
    return BlankGrid();
  }

  switch (activeModifier_)
  {
    case UiModifier::Sound:
      return BuildSoundMenu(engine);
    case UiModifier::Parameter:
      if (parameterAdjustMode_)
      {
        return BuildParameterAdjustMenu();
      }
      return BuildParameterBrowseMenu(engine);
    case UiModifier::Layout:
      return BuildLayoutMenu();
    case UiModifier::Other:
      return BuildOtherMenu();
    case UiModifier::None:
    default:
      return BlankGrid();
  }
}

std::size_t SynthUiModel::ContextMenuColumnCount() const
{
  if (activeModifier_ == UiModifier::Sound)
  {
    return 1;
  }

  if (activeModifier_ == UiModifier::Parameter && tracks_[activeTrack_].kind == SynthEngineKind::Juno &&
      selectedParameterGroupId_ == "par.juno.group.fx")
  {
    return 1;
  }

  return 2;
}

UiScreenSnapshot SynthUiModel::CurrentScreenSnapshot(const ISynthEngine* engine) const
{
  return BuildScreenSnapshot(engine);
}

bool SynthUiModel::ConsumePendingEngineKind(SynthEngineKind& kind)
{
  if (!hasPendingEngineKind_)
  {
    return false;
  }

  kind = pendingEngineKind_;
  hasPendingEngineKind_ = false;
  pendingEngineKind_ = SynthEngineKind::None;
  return true;
}

bool SynthUiModel::IsModifierHighlighted(std::size_t row) const
{
  return mode_ == UiMode::ContextMenu && row == RowForModifier(activeModifier_);
}

bool SynthUiModel::IsMenuCellActive(std::size_t row, std::size_t col) const
{
  if (mode_ != UiMode::ContextMenu)
  {
    return false;
  }

  const std::size_t columns = ContextMenuColumnCount();
  if (col >= columns)
  {
    return false;
  }

  const std::size_t index = columns == 1 ? row : row * kContextMenuColumns + col;
  return heldContextMenuIndex_ == static_cast<int>(index) ||
         (hasMenuHighlight_ && row == activeMenuRow_ && col == activeMenuColumn_);
}

std::string SynthUiModel::DebugSummary(const ISynthEngine* engine) const
{
  std::ostringstream stream;
  stream << (mode_ == UiMode::Home ? "HOME" : "CONTEXT")
         << "  engine=" << HeaderEngine(engine)
         << "  track=" << (activeTrack_ + 1)
         << "  patch=" << PatchName();
  if (mode_ == UiMode::ContextMenu)
  {
    stream << "  section=" << ContextSectionTitle()
           << "  page=" << (currentPage_ + 1);
    if (hasContextSelection_)
    {
      stream << "  param=" << ContextParameterName(engine)
             << "  value=" << ContextValueText(engine);
    }
  }
  return stream.str();
}

UiBarSnapshot SynthUiModel::BuildModifierBarSnapshot(UiModifier highlightedModifier) const
{
  UiBarSnapshot items{};
  for (std::size_t row = 0; row < kModifierBarLabels.size(); ++row)
  {
    items[row].label = kModifierBarLabels[row];
    items[row].active = heldModifiers_[row] ||
                        (highlightedModifier != UiModifier::None && row == RowForModifier(highlightedModifier));
  }
  return items;
}

UiBarSnapshot SynthUiModel::BuildHomeActionBarSnapshot() const
{
  UiBarSnapshot items{};
  for (std::size_t row = 0; row < kHomeActionBarIcons.size(); ++row)
  {
    items[row].icon = kHomeActionBarIcons[row];
    items[row].active = heldHomeActionRows_[row];
  }
  return items;
}

UiBarSnapshot SynthUiModel::BuildContextBarSnapshot(const ISynthEngine* engine) const
{
  UiBarSnapshot items{};
  const auto menu = CurrentMenuCells(engine);
  const std::size_t columns = ContextMenuColumnCount();
  for (std::size_t row = 0; row < kContextMenuRows; ++row)
  {
    for (std::size_t column = 0; column < columns; ++column)
    {
      const std::size_t index = columns == 1 ? row : row * kContextMenuColumns + column;
      items[index].label = menu[index].label;
      items[index].active = heldContextMenuIndex_ == static_cast<int>(index) ||
                            (hasMenuHighlight_ && row == activeMenuRow_ && column == activeMenuColumn_);
    }
  }
  return items;
}

UiScreenSnapshot SynthUiModel::BuildScreenSnapshot(const ISynthEngine* engine) const
{
  UiScreenSnapshot snapshot;
  snapshot.mode = mode_;
  snapshot.modifier = activeModifier_;
  snapshot.engineKind = engine != nullptr ? engine->Descriptor().kind : tracks_[activeTrack_].kind;
  snapshot.focusItemId = selectedItemId_.empty() ? selectedParameterGroupId_ : selectedItemId_;
  snapshot.dx7Algorithm = ParameterValueOrDefault(engine, "global.algorithm", 0);
  snapshot.dx7Feedback = ParameterValueOrDefault(engine, "global.feedback", 0);

  if (mode_ == UiMode::Home)
  {
    snapshot.headerPrimary = HeaderEngine(engine);
    snapshot.headerSecondary = HeaderTrack();
    snapshot.primaryText = PatchName();
    snapshot.looperProgress = LooperProgress();
    snapshot.leftBarItems = BuildModifierBarSnapshot(UiModifier::None);
    snapshot.leftBarVisible = true;
    snapshot.leftBarColumns = 1;
    snapshot.leftBarWidthScale = 1.0f;
    snapshot.rightBarItems = BuildHomeActionBarSnapshot();
    snapshot.rightBarVisible = true;
    snapshot.rightBarColumns = 1;
    snapshot.rightBarWidthScale = 1.0f;
    return snapshot;
  }

  snapshot.headerPrimary = std::string(ModifierDisplayName(activeModifier_));
  if (activeModifier_ == UiModifier::Sound && presetBrowserMode_)
  {
    const auto presets = CollectPresetMatches(engine, presetSearchText_);
    snapshot.headerPrimary.clear();
    snapshot.primaryText = presetSearchText_.empty() ? "Preset" : presetSearchText_ + "_";
    snapshot.secondaryText = BuildPresetBrowserListText(presets, presetBrowserSelectionIndex_);
    snapshot.showPresetBrowser = true;
    snapshot.showSecondaryText = true;
  }
  else if (activeModifier_ == UiModifier::Sound && !enginePickerMode_)
  {
    snapshot.headerPrimary = "Track " + std::to_string(activeTrack_ + 1);
    snapshot.primaryText = HeaderEngine(engine);
    snapshot.secondaryText = PatchName();
    snapshot.showSecondaryText = true;
  }
  else if (snapshot.engineKind == SynthEngineKind::Dx7 && activeModifier_ == UiModifier::Parameter)
  {
    if (parameterAdjustMode_ && hasContextSelection_)
    {
      snapshot.primaryText = ContextParameterName(engine);
      snapshot.secondaryText = ContextValueText(engine);
    }
    else if (!selectedParameterGroupId_.empty())
    {
      snapshot.primaryText = Dx7GroupLabel(selectedParameterGroupId_);
      snapshot.secondaryText = Dx7GroupSummary(engine, selectedParameterGroupId_);
    }
    else
    {
      snapshot.primaryText = "DX7";
      snapshot.secondaryText = FormatDx7AlgorithmSummary(engine);
    }
    snapshot.showSecondaryText = true;
  }
  else if (snapshot.engineKind == SynthEngineKind::Juno && activeModifier_ == UiModifier::Parameter)
  {
    PopulateJunoSnapshotValues(snapshot, engine);
    if (parameterAdjustMode_ && hasContextSelection_)
    {
      const std::string groupId = JunoGroupIdForParameterId(selectedItemId_);
      snapshot.headerPrimary = JunoGroupLabel(groupId);
      snapshot.primaryText = ContextParameterName(engine);
      snapshot.secondaryText = ContextValueText(engine);
    }
    else if (!selectedParameterGroupId_.empty())
    {
      snapshot.headerPrimary = JunoGroupLabel(selectedParameterGroupId_);
      if (selectedParameterGroupId_ == "par.juno.group.fx")
      {
        snapshot.primaryText = "Chorus";
        snapshot.secondaryText = DescribeParameterValueOrFallback(engine, "chorus.mode");
      }
      else
      {
        snapshot.primaryText = JunoGroupTitle(selectedParameterGroupId_);
        snapshot.secondaryText = JunoGroupSummary(engine, selectedParameterGroupId_);
      }
    }
    else
    {
      snapshot.headerPrimary = "Preset";
      snapshot.primaryText = PatchName();
      snapshot.secondaryText = JunoOverviewSummary(engine);
    }
    snapshot.showSecondaryText = true;
  }
  else
  {
    snapshot.primaryText = hasContextSelection_ ? ContextParameterName(engine) : std::string{"Settings"};
    snapshot.secondaryText = hasContextSelection_ ? ContextValueText(engine) : std::string{};
    snapshot.showSecondaryText = hasContextSelection_;
  }
  snapshot.leftBarItems = BuildModifierBarSnapshot(activeModifier_);
  snapshot.leftBarVisible = false;
  snapshot.leftBarColumns = 1;
  snapshot.leftBarWidthScale = 1.0f;
  snapshot.rightBarItems = BuildContextBarSnapshot(engine);
  snapshot.rightBarVisible = true;
  snapshot.rightBarColumns = ContextMenuColumnCount();
  snapshot.rightBarWidthScale = snapshot.rightBarColumns == 1 ? 1.5f : 1.0f;
  return snapshot;
}

void SynthUiModel::StartTransition(const UiScreenSnapshot& from, const UiScreenSnapshot& to)
{
  transition_.from = from;
  transition_.to = to;

  if (from.mode == to.mode && from.modifier == to.modifier && from.primaryText == to.primaryText &&
      from.secondaryText == to.secondaryText && from.leftBarVisible == to.leftBarVisible &&
      from.leftBarWidthScale == to.leftBarWidthScale && from.rightBarColumns == to.rightBarColumns &&
      from.rightBarWidthScale == to.rightBarWidthScale)
  {
    transition_.active = false;
    transition_.progress = 1.0f;
    return;
  }

  transition_.active = true;
  transition_.progress = 0.0f;
}

void SynthUiModel::CancelTransition()
{
  transition_.active = false;
  transition_.progress = 1.0f;
}

void SynthUiModel::ActivateModifier(UiModifier modifier)
{
  if (modifier == UiModifier::None)
  {
    ExitToHome();
    return;
  }

  if (mode_ == UiMode::ContextMenu && activeModifier_ == modifier)
  {
    return;
  }

  mode_ = UiMode::ContextMenu;
  activeModifier_ = modifier;
  currentPage_ = 0;
  activeMenuRow_ = 0;
  activeMenuColumn_ = 0;
  enginePickerMode_ = false;
  hasPendingEngineKind_ = false;
  pendingEngineKind_ = SynthEngineKind::None;
  presetBrowserMode_ = false;
  presetSearchText_.clear();
  presetBrowserSelectionIndex_ = 0;
  contextLatched_ = false;
  selectedParameterGroupId_.clear();
  ClearContextSelection();
}

void SynthUiModel::ClearContextSelection()
{
  hasContextSelection_ = false;
  hasMenuHighlight_ = false;
  parameterAdjustMode_ = false;
  selectionKind_ = ContextSelectionKind::None;
  selectedItemId_.clear();
  selectedItemLabel_.clear();
  selectedValueText_.clear();
  selectedParameterEntryValue_ = 0;
}

void SynthUiModel::HighlightMenuIndex(std::size_t index)
{
  hasMenuHighlight_ = true;
  if (ContextMenuColumnCount() == 1)
  {
    activeMenuRow_ = index;
    activeMenuColumn_ = 0;
    return;
  }

  activeMenuRow_ = index / kContextMenuColumns;
  activeMenuColumn_ = index % kContextMenuColumns;
}

void SynthUiModel::SelectStatus(const std::string& label, const std::string& value, const std::string& itemId)
{
  hasContextSelection_ = true;
  selectionKind_ = ContextSelectionKind::Status;
  selectedItemId_ = itemId;
  selectedItemLabel_ = label;
  selectedValueText_ = value;
}

void SynthUiModel::EnterEnginePicker()
{
  enginePickerMode_ = true;
  enginePickerInitialKind_ = tracks_[activeTrack_].kind;
  enginePickerPreviewKind_ = enginePickerInitialKind_;
  hasPendingEngineKind_ = false;
  pendingEngineKind_ = SynthEngineKind::None;
  heldContextMenuIndex_ = -1;
  SelectStatus("Engine", ToString(enginePickerPreviewKind_), "snd.engine");
}

void SynthUiModel::EnterPresetBrowser(const ISynthEngine* engine)
{
  presetBrowserMode_ = true;
  presetSearchText_.clear();
  presetBrowserSelectionIndex_ = 0;
  heldContextMenuIndex_ = -1;
  hasMenuHighlight_ = false;
  hasContextSelection_ = true;
  selectionKind_ = ContextSelectionKind::Preset;
  selectedItemId_ = "snd.preset";
  selectedItemLabel_ = "Preset";
  selectedValueText_ = engine != nullptr ? engine->CurrentPresetName() : PatchName();

  const auto presets = CollectPresetMatches(engine, presetSearchText_);
  if (!presets.empty())
  {
    presetBrowserSelectionIndex_ = ActivePresetMatchIndex(presets);
  }
}

void SynthUiModel::EnterParameterAdjustMode(const SynthParameterInfo& parameter, std::size_t menuIndex)
{
  hasContextSelection_ = true;
  parameterAdjustMode_ = true;
  selectionKind_ = ContextSelectionKind::Parameter;
  selectedItemId_ = parameter.id;
  selectedItemLabel_ = parameter.label;
  selectedParameterEntryValue_ = parameter.value;
  hasMenuHighlight_ = false;
  HighlightMenuIndex(menuIndex);
  hasMenuHighlight_ = false;
}

void SynthUiModel::ApplyMenuCell(ISynthEngine* engine, const ContextMenuCell& cell, std::size_t index)
{
  if (cell.type == MenuCellType::Empty)
  {
    return;
  }

  if (FirstHeldModifier() != UiModifier::None)
  {
    contextLatched_ = true;
  }

  HighlightMenuIndex(index);
  if (cell.type == MenuCellType::NextPage)
  {
    currentPage_ = (currentPage_ + 1) % std::max<std::size_t>(1, PageCountForModifier(engine));
    ClearContextSelection();
    return;
  }

  if (cell.type == MenuCellType::Item)
  {
    if (activeModifier_ == UiModifier::Sound && cell.itemId == "snd.engine")
    {
      EnterEnginePicker();
      return;
    }

    if (activeModifier_ == UiModifier::Sound && cell.itemId == "snd.preset")
    {
      EnterPresetBrowser(engine);
      return;
    }

    if (activeModifier_ == UiModifier::Parameter && IsDx7ParameterEngine(engine))
    {
      if (IsDx7ParameterGroupId(cell.itemId))
      {
        selectedParameterGroupId_ = cell.itemId;
        currentPage_ = 0;
        ClearContextSelection();
        return;
      }

      if (cell.itemId == "par.dx7.back")
      {
        selectedParameterGroupId_.clear();
        currentPage_ = 0;
        ClearContextSelection();
        return;
      }

      if (!selectedParameterGroupId_.empty() && engine != nullptr)
      {
        SynthParameterInfo parameter;
        if (engine->GetParameterInfo(cell.itemId, parameter))
        {
          EnterParameterAdjustMode(parameter, index);
          return;
        }
      }
    }

    if (activeModifier_ == UiModifier::Parameter && IsJunoParameterEngine(engine))
    {
      if (IsJunoParameterGroupId(cell.itemId))
      {
        selectedParameterGroupId_ = cell.itemId;
        currentPage_ = 0;
        if (cell.itemId == "par.juno.group.amp" && engine != nullptr)
        {
          SynthParameterInfo parameter;
          if (engine->GetParameterInfo("vca.level", parameter))
          {
            EnterParameterAdjustMode(parameter, index);
            return;
          }
        }

        if (cell.itemId == "par.juno.group.fx")
        {
          SelectStatus("Chorus", DescribeParameterValueOrFallback(engine, "chorus.mode"), "chorus.mode");
          return;
        }

        ClearContextSelection();
        return;
      }

      if (cell.itemId == "par.juno.back")
      {
        selectedParameterGroupId_.clear();
        currentPage_ = 0;
        ClearContextSelection();
        return;
      }

      if (!selectedParameterGroupId_.empty() && engine != nullptr)
      {
        SynthParameterInfo parameter;
        if (engine->GetParameterInfo(cell.itemId, parameter))
        {
          EnterParameterAdjustMode(parameter, index);
          return;
        }
      }
    }

    const auto presentation = MenuItemPresentation(activeModifier_, cell.itemId, tracks_[activeTrack_].patchName, engine);
    SelectStatus(presentation.first, presentation.second, cell.itemId);
    return;
  }

  if (cell.type != MenuCellType::UtilityAction)
  {
    return;
  }

  switch (cell.actionId)
  {
    case ContextActionId::EngineNext:
      if (enginePickerMode_)
      {
        enginePickerPreviewKind_ = StepSelectableEngineKind(enginePickerPreviewKind_, 1);
        SelectStatus("Engine", ToString(enginePickerPreviewKind_), "snd.engine");
      }
      break;
    case ContextActionId::EnginePrev:
      if (enginePickerMode_)
      {
        enginePickerPreviewKind_ = StepSelectableEngineKind(enginePickerPreviewKind_, -1);
        SelectStatus("Engine", ToString(enginePickerPreviewKind_), "snd.engine");
      }
      break;
    case ContextActionId::EngineReset:
      if (enginePickerMode_)
      {
        enginePickerPreviewKind_ = enginePickerInitialKind_;
        SelectStatus("Engine", ToString(enginePickerPreviewKind_), "snd.engine");
      }
      break;
    case ContextActionId::EngineLoad:
      if (enginePickerMode_)
      {
        pendingEngineKind_ = enginePickerPreviewKind_;
        hasPendingEngineKind_ = true;
        enginePickerMode_ = false;
        heldContextMenuIndex_ = -1;
        currentPage_ = 0;
        ClearContextSelection();
      }
      break;
    case ContextActionId::PresetLoad:
      if (presetBrowserMode_ && engine != nullptr)
      {
        const auto presets = CollectPresetMatches(engine, presetSearchText_);
        if (!presets.empty())
        {
          const std::size_t selectedIndex = std::min(presetBrowserSelectionIndex_, presets.size() - 1);
          if (engine->LoadPreset(presets[selectedIndex].id))
          {
            tracks_[activeTrack_].patchName = engine->CurrentPresetName();
            presetBrowserMode_ = false;
            presetSearchText_.clear();
            presetBrowserSelectionIndex_ = 0;
            heldContextMenuIndex_ = -1;
            ClearContextSelection();
          }
        }
      }
      break;
    case ContextActionId::PresetPrev:
      if (presetBrowserMode_ && presetBrowserSelectionIndex_ > 0)
      {
        --presetBrowserSelectionIndex_;
      }
      break;
    case ContextActionId::PresetNext:
      if (presetBrowserMode_ && engine != nullptr)
      {
        const auto presets = CollectPresetMatches(engine, presetSearchText_);
        if (!presets.empty())
        {
          presetBrowserSelectionIndex_ = std::min(presetBrowserSelectionIndex_ + 1, presets.size() - 1);
        }
      }
      break;
    case ContextActionId::PresetDelete:
      if (presetBrowserMode_ && !presetSearchText_.empty())
      {
        presetSearchText_.pop_back();
        const auto presets = CollectPresetMatches(engine, presetSearchText_);
        presetBrowserSelectionIndex_ = presetSearchText_.empty() ? ActivePresetMatchIndex(presets) : 0;
      }
      break;
    case ContextActionId::PresetClear:
      if (presetBrowserMode_)
      {
        presetSearchText_.clear();
        presetBrowserSelectionIndex_ = ActivePresetMatchIndex(CollectPresetMatches(engine, presetSearchText_));
      }
      break;
    case ContextActionId::ParameterDecLarge:
      if (engine != nullptr)
      {
        (void)engine->AdjustParameterValue(selectedItemId_, -10);
        hasContextSelection_ = true;
        selectionKind_ = ContextSelectionKind::Parameter;
      }
      break;
    case ContextActionId::ParameterDecSmall:
      if (engine != nullptr)
      {
        (void)engine->AdjustParameterValue(selectedItemId_, -1);
        hasContextSelection_ = true;
        selectionKind_ = ContextSelectionKind::Parameter;
      }
      break;
    case ContextActionId::ParameterIncSmall:
      if (engine != nullptr)
      {
        (void)engine->AdjustParameterValue(selectedItemId_, 1);
        hasContextSelection_ = true;
        selectionKind_ = ContextSelectionKind::Parameter;
      }
      break;
    case ContextActionId::ParameterIncLarge:
      if (engine != nullptr)
      {
        (void)engine->AdjustParameterValue(selectedItemId_, 10);
        hasContextSelection_ = true;
        selectionKind_ = ContextSelectionKind::Parameter;
      }
      break;
    case ContextActionId::ParameterMin:
      if (engine != nullptr)
      {
        SynthParameterInfo parameter;
        if (engine->GetParameterInfo(selectedItemId_, parameter))
        {
          (void)engine->SetParameterValue(selectedItemId_, parameter.minValue);
        }
      }
      break;
    case ContextActionId::ParameterMax:
      if (engine != nullptr)
      {
        SynthParameterInfo parameter;
        if (engine->GetParameterInfo(selectedItemId_, parameter))
        {
          (void)engine->SetParameterValue(selectedItemId_, parameter.maxValue);
        }
      }
      break;
    case ContextActionId::ParameterReset:
      if (engine != nullptr)
      {
        (void)engine->SetParameterValue(selectedItemId_, selectedParameterEntryValue_);
      }
      break;
    case ContextActionId::JunoChorusOff:
      if (engine != nullptr)
      {
        (void)engine->SetParameterValue("chorus.mode", 0);
        SelectStatus("Chorus", DescribeParameterValueOrFallback(engine, "chorus.mode"), "chorus.mode");
      }
      break;
    case ContextActionId::JunoChorusI:
      if (engine != nullptr)
      {
        (void)engine->SetParameterValue("chorus.mode", 1);
        SelectStatus("Chorus", DescribeParameterValueOrFallback(engine, "chorus.mode"), "chorus.mode");
      }
      break;
    case ContextActionId::JunoChorusII:
      if (engine != nullptr)
      {
        (void)engine->SetParameterValue("chorus.mode", 2);
        SelectStatus("Chorus", DescribeParameterValueOrFallback(engine, "chorus.mode"), "chorus.mode");
      }
      break;
    case ContextActionId::Back:
      if (presetBrowserMode_)
      {
        presetBrowserMode_ = false;
        presetSearchText_.clear();
        presetBrowserSelectionIndex_ = 0;
        heldContextMenuIndex_ = -1;
        ClearContextSelection();
        break;
      }

      if (enginePickerMode_)
      {
        enginePickerMode_ = false;
        enginePickerPreviewKind_ = SynthEngineKind::None;
        hasPendingEngineKind_ = false;
        pendingEngineKind_ = SynthEngineKind::None;
        ClearContextSelection();
        break;
      }

      if (activeModifier_ == UiModifier::Parameter && parameterAdjustMode_)
      {
        if (IsJunoDirectAdjustGroupId(selectedParameterGroupId_))
        {
          selectedParameterGroupId_.clear();
        }
        parameterAdjustMode_ = false;
        selectionKind_ = ContextSelectionKind::None;
        hasContextSelection_ = false;
        selectedItemId_.clear();
        selectedItemLabel_.clear();
        selectedValueText_.clear();
        selectedParameterEntryValue_ = 0;
        hasMenuHighlight_ = false;
      }
      else
      {
        if (IsJunoDirectActionGroupId(selectedParameterGroupId_))
        {
          selectedParameterGroupId_.clear();
        }
        currentPage_ = 0;
        ClearContextSelection();
      }
      break;
    case ContextActionId::SavePreset:
      if (engine != nullptr)
      {
        (void)engine->SavePreset(engine->CurrentPresetId(), engine->CurrentPresetName());
        tracks_[activeTrack_].patchName = engine->CurrentPresetName();
        SelectStatus("Save Preset", tracks_[activeTrack_].patchName);
      }
      break;
    case ContextActionId::SavePresetAs:
      if (engine != nullptr)
      {
        const std::string name = tracks_[activeTrack_].patchName + " Copy";
        (void)engine->SavePreset("", name);
        tracks_[activeTrack_].patchName = engine->CurrentPresetName();
        SelectStatus("Save As", tracks_[activeTrack_].patchName);
      }
      break;
    case ContextActionId::LayoutChromatic:
      SelectStatus("Layout", "CHROMATIC");
      break;
    case ContextActionId::LayoutScale:
      SelectStatus("Layout", "SCALE");
      break;
    case ContextActionId::LayoutArp:
      SelectStatus("Arp", "ON");
      break;
    case ContextActionId::LayoutLatch:
      SelectStatus("Latch", "ON");
      break;
    case ContextActionId::LayoutOctaveDown:
      SelectStatus("Octave", "-");
      break;
    case ContextActionId::LayoutOctaveUp:
      SelectStatus("Octave", "+");
      break;
    case ContextActionId::LayoutGrid:
      SelectStatus("Grid", "4x14");
      break;
    case ContextActionId::LayoutSave:
      SelectStatus("Layout", "SAVED");
      break;
    case ContextActionId::ShowInfo:
      SelectStatus("Info", HeaderTrack());
      break;
    case ContextActionId::AudioSetup:
      SelectStatus("Audio", "ROUTING");
      break;
    case ContextActionId::MidiSetup:
      SelectStatus("MIDI", "LOCAL");
      break;
    case ContextActionId::ResetPatch:
      SelectStatus("Patch", "RESET");
      break;
    case ContextActionId::None:
    default:
      break;
  }

  hasMenuHighlight_ = false;
}

void SynthUiModel::ClearHeldModifiers()
{
  heldModifiers_.fill(false);
  modifierContextActivated_.fill(false);
  modifierPressStartedAt_.fill(std::chrono::steady_clock::time_point{});
}

std::size_t SynthUiModel::PageCountForModifier(const ISynthEngine* engine) const
{
  if (engine == nullptr)
  {
    return 1;
  }

  switch (activeModifier_)
  {
    case UiModifier::Sound:
      return 1;
    case UiModifier::Parameter:
      if (parameterAdjustMode_)
      {
        return 1;
      }
      if (IsDx7ParameterEngine(engine) && !selectedParameterGroupId_.empty())
      {
        return Dx7ParameterPageCount(engine, selectedParameterGroupId_);
      }
      if (IsJunoParameterEngine(engine) && !selectedParameterGroupId_.empty())
      {
        return JunoParameterPageCount(engine, selectedParameterGroupId_);
      }
      return (IsDx7ParameterEngine(engine) || IsJunoParameterEngine(engine)) ? 1 : 2;
    case UiModifier::Layout:
      return 2;
    case UiModifier::Other:
      return 2;
    case UiModifier::None:
    default:
      return 1;
  }
}

ContextMenuGrid SynthUiModel::BuildSoundMenu(const ISynthEngine* engine) const
{
  (void)engine;

  if (presetBrowserMode_)
  {
    return ContextMenuGrid{
        MakeActionCell(presetSearchText_.empty() ? "BACK" : "DEL",
                       presetSearchText_.empty() ? ContextActionId::Back : ContextActionId::PresetDelete),
        MakeActionCell("PREV", ContextActionId::PresetPrev),
        MakeActionCell("NEXT", ContextActionId::PresetNext),
        MakeActionCell("LOAD", ContextActionId::PresetLoad),
        ContextMenuCell{},
        ContextMenuCell{},
        ContextMenuCell{},
        ContextMenuCell{},
    };
  }

  if (enginePickerMode_)
  {
    return ContextMenuGrid{
        MakeActionCell("RESET", ContextActionId::EngineReset),
        MakeActionCell("PREV", ContextActionId::EnginePrev),
        MakeActionCell("NEXT", ContextActionId::EngineNext),
        MakeActionCell("LOAD", ContextActionId::EngineLoad),
        ContextMenuCell{},
        ContextMenuCell{},
        ContextMenuCell{},
        ContextMenuCell{},
    };
  }

  return ContextMenuGrid{
      MakeItemCell("ENGINE", "snd.engine"),
      MakeItemCell("PRESET", "snd.preset"),
      MakeItemCell("VOLUME", "snd.volume"),
      MakeItemCell("FILTER", "snd.filter"),
      ContextMenuCell{},
      ContextMenuCell{},
      ContextMenuCell{},
      ContextMenuCell{},
  };
}

ContextMenuGrid SynthUiModel::BuildParameterBrowseMenu(const ISynthEngine* engine) const
{
  if (IsDx7ParameterEngine(engine))
  {
    if (selectedParameterGroupId_.empty())
    {
      return ContextMenuGrid{
          MakeItemCell("OP1", "par.dx7.group.op1"),
          MakeItemCell("OP2", "par.dx7.group.op2"),
          MakeItemCell("OP3", "par.dx7.group.op3"),
          MakeItemCell("OP4", "par.dx7.group.op4"),
          MakeItemCell("OP5", "par.dx7.group.op5"),
          MakeItemCell("OP6", "par.dx7.group.op6"),
          MakeItemCell("ALG", "par.dx7.group.alg"),
          MakeItemCell("GLB", "par.dx7.group.glb"),
      };
    }

    constexpr std::size_t kPageCapacity = 6;
    ContextMenuGrid menu = BlankGrid();
    const auto parameters = CollectDx7GroupParameters(engine, selectedParameterGroupId_);
    const std::size_t pageCount = Dx7ParameterPageCount(engine, selectedParameterGroupId_);
    const std::size_t pageStart = currentPage_ * kPageCapacity;
    const std::size_t pageEnd = std::min(parameters.size(), pageStart + kPageCapacity);
    std::size_t menuIndex = 0;
    for (std::size_t parameterIndex = pageStart; parameterIndex < pageEnd && menuIndex < kPageCapacity; ++parameterIndex, ++menuIndex)
    {
      menu[menuIndex] = MakeItemCell(Dx7ParameterMenuLabel(parameters[parameterIndex]), parameters[parameterIndex].id);
    }
    menu[6] = MakeItemCell("BACK", "par.dx7.back");
    if (pageCount > 1)
    {
      menu[7] = MakeNextPageCell(std::to_string(currentPage_ + 1) + "/" + std::to_string(pageCount));
    }
    return menu;
  }

  if (IsJunoParameterEngine(engine))
  {
    if (selectedParameterGroupId_.empty())
    {
      return ContextMenuGrid{
          MakeItemCell("DCO", "par.juno.group.dco"),
          MakeItemCell("VCF", "par.juno.group.vcf"),
          MakeItemCell("ENV", "par.juno.group.env"),
          MakeItemCell("LFO", "par.juno.group.lfo"),
          MakeItemCell("AMP", "par.juno.group.amp"),
          MakeItemCell("FX", "par.juno.group.fx"),
          ContextMenuCell{},
          ContextMenuCell{},
      };
    }

    if (selectedParameterGroupId_ == "par.juno.group.fx")
    {
      return ContextMenuGrid{
          MakeActionCell("OFF", ContextActionId::JunoChorusOff),
          MakeActionCell("I", ContextActionId::JunoChorusI),
          MakeActionCell("II", ContextActionId::JunoChorusII),
          MakeActionCell("BACK", ContextActionId::Back),
          ContextMenuCell{},
          ContextMenuCell{},
          ContextMenuCell{},
          ContextMenuCell{},
      };
    }

    ContextMenuGrid menu = BlankGrid();
    const auto parameters = CollectJunoGroupParameters(engine, selectedParameterGroupId_);
    const std::size_t pageCount = JunoParameterPageCount(engine, selectedParameterGroupId_);
    const std::size_t pageStart = currentPage_ * kJunoMenuPageCapacity;
    const std::size_t pageEnd = std::min(parameters.size(), pageStart + kJunoMenuPageCapacity);
    std::size_t menuIndex = 0;
    for (std::size_t parameterIndex = pageStart; parameterIndex < pageEnd && menuIndex < kJunoMenuPageCapacity;
         ++parameterIndex, ++menuIndex)
    {
      menu[menuIndex] = MakeItemCell(JunoParameterMenuLabel(parameters[parameterIndex]), parameters[parameterIndex].id);
    }

    if (pageCount > 1)
    {
      menu[6] = MakeNextPageCell(std::to_string(currentPage_ + 1) + "/" + std::to_string(pageCount));
    }
    menu[7] = MakeItemCell("BACK", "par.juno.back");
    return menu;
  }

  (void)engine;
  if ((currentPage_ % 2) == 0)
  {
    return ContextMenuGrid{
        MakeItemCell("PREV", "par.prev"),
        MakeItemCell("NEXT", "par.next"),
        MakeItemCell("FINE", "par.fine"),
        MakeItemCell("RST", "par.rst"),
        MakeItemCell("MIN", "par.min"),
        MakeItemCell("MAX", "par.max"),
        MakeItemCell("LOCK", "par.lock"),
        MakeNextPageCell("1/2"),
    };
  }

  return ContextMenuGrid{
      MakeItemCell("M1", "par.m1"),
      MakeItemCell("M2", "par.m2"),
      MakeItemCell("M3", "par.m3"),
      MakeItemCell("M4", "par.m4"),
      MakeItemCell("MAP", "par.map"),
      MakeItemCell("CLR", "par.clr"),
      MakeItemCell("SAVE", "par.save"),
      MakeNextPageCell("2/2"),
  };
}

ContextMenuGrid SynthUiModel::BuildParameterAdjustMenu() const
{
  return ContextMenuGrid{
      MakeActionCell("-1", ContextActionId::ParameterDecSmall),
      MakeActionCell("+1", ContextActionId::ParameterIncSmall),
      MakeActionCell("-10", ContextActionId::ParameterDecLarge),
      MakeActionCell("+10", ContextActionId::ParameterIncLarge),
      MakeActionCell("MIN", ContextActionId::ParameterMin),
      MakeActionCell("MAX", ContextActionId::ParameterMax),
      MakeActionCell("RST", ContextActionId::ParameterReset),
      MakeActionCell("BACK", ContextActionId::Back),
  };
}

ContextMenuGrid SynthUiModel::BuildLayoutMenu() const
{
  if ((currentPage_ % 2) == 0)
  {
    return ContextMenuGrid{
        MakeItemCell("SCL", "lay.scl"),
        MakeItemCell("ROOT", "lay.root"),
        MakeItemCell("MODE", "lay.mode"),
        MakeItemCell("CHRD", "lay.chrd"),
        MakeItemCell("OCT", "lay.oct"),
        MakeItemCell("TRNS", "lay.trns"),
        MakeItemCell("KEY", "lay.key"),
        MakeNextPageCell("1/2"),
    };
  }

  return ContextMenuGrid{
      MakeItemCell("GRID", "lay.grid"),
      MakeItemCell("KEYB", "lay.keyb"),
      MakeItemCell("DRUM", "lay.drum"),
      MakeItemCell("MPE", "lay.mpe"),
      MakeItemCell("LATCH", "lay.latch"),
      MakeItemCell("ARP", "lay.arp"),
      MakeItemCell("SAVE", "lay.save"),
      MakeNextPageCell("2/2"),
  };
}

ContextMenuGrid SynthUiModel::BuildOtherMenu() const
{
  if ((currentPage_ % 2) == 0)
  {
    return ContextMenuGrid{
        MakeItemCell("SAVE", "sys.save"),
        MakeItemCell("LOAD", "sys.load"),
        MakeItemCell("MIDI", "sys.midi"),
        MakeItemCell("SYNC", "sys.sync"),
        MakeItemCell("USB", "sys.usb"),
        MakeItemCell("SD", "sys.sd"),
        MakeItemCell("SET", "sys.set"),
        MakeNextPageCell("1/2"),
    };
  }

  return ContextMenuGrid{
      MakeItemCell("BRGT", "sys.brgt"),
      MakeItemCell("INFO", "sys.info"),
      MakeItemCell("CAL", "sys.cal"),
      MakeItemCell("TEST", "sys.test"),
      MakeItemCell("RST", "sys.rst"),
      MakeItemCell("UPD", "sys.upd"),
      MakeItemCell("BACK", "sys.back"),
      MakeNextPageCell("2/2"),
  };
}

UiModifier SynthUiModel::FirstHeldModifier() const
{
  for (std::size_t row = 0; row < heldModifiers_.size(); ++row)
  {
    if (heldModifiers_[row])
    {
      return ModifierForRow(row);
    }
  }
  return UiModifier::None;
}

}  // namespace synth