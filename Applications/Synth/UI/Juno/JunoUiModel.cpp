#include "Applications/Synth/UI/Juno/JunoUiModel.h"

#include "Applications/Synth/UI/Menu/SynthUiMenu.h"

#include <algorithm>
#include <string>
#include <vector>

namespace synth::ui_model {

bool IsJunoParameterEngine(const ISynthEngine* engine)
{
  return engine != nullptr && engine->Descriptor().kind == SynthEngineKind::Juno;
}

bool IsJunoParameterGroupId(const std::string& itemId)
{
  return itemId == "par.juno.group.dco" || itemId == "par.juno.group.hpf" || itemId == "par.juno.group.vcf" || itemId == "par.juno.group.env" ||
         itemId == "par.juno.group.lfo" || itemId == "par.juno.group.amp" || itemId == "par.juno.group.fx";
}

std::string JunoGroupIdForParameterId(const std::string& itemId)
{
  if (itemId.rfind("dco.", 0) == 0) return "par.juno.group.dco";
  if (itemId == "hpf.mode") return "par.juno.group.hpf";
  if (itemId.rfind("vcf.", 0) == 0) return "par.juno.group.vcf";
  if (itemId.rfind("env.", 0) == 0) return "par.juno.group.env";
  if (itemId.rfind("lfo.", 0) == 0) return "par.juno.group.lfo";
  if (itemId.rfind("vca.", 0) == 0) return "par.juno.group.amp";
  if (itemId.rfind("chorus.", 0) == 0) return "par.juno.group.fx";
  return {};
}

std::string JunoGroupLabel(const std::string& groupId)
{
  if (groupId == "par.juno.group.hpf") return "HPF";
  if (groupId == "par.juno.group.dco") return "DCO";
  if (groupId == "par.juno.group.vcf") return "VCF";
  if (groupId == "par.juno.group.env") return "ENV";
  if (groupId == "par.juno.group.lfo") return "LFO";
  if (groupId == "par.juno.group.amp") return "VCA";
  if (groupId == "par.juno.group.fx") return "CHO";
  return "Juno";
}

std::string JunoGroupTitle(const std::string& groupId)
{
  if (groupId == "par.juno.group.lfo") return "LFO";
  if (groupId == "par.juno.group.dco") return "DCO";
  if (groupId == "par.juno.group.hpf") return "HPF";
  if (groupId == "par.juno.group.vcf") return "VCF";
  if (groupId == "par.juno.group.amp") return "VCA";
  if (groupId == "par.juno.group.env") return "Envelope";
  if (groupId == "par.juno.group.fx") return "Chorus";
  return "Sections";
}

std::string JunoGroupHelpDescription(const std::string& groupId)
{
  if (groupId == "par.juno.group.lfo") return "Shape the shared LFO speed and fade-in before modulation reaches full depth";
  if (groupId == "par.juno.group.dco") return "Set the oscillator mix, range, and pulse tone that feed the rest of the patch";
  if (groupId == "par.juno.group.hpf") return "Trim low frequencies before the main filter to thin out the bottom end";
  if (groupId == "par.juno.group.vcf") return "Shape brightness and filter motion with cutoff, resonance, envelope, and LFO depth";
  if (groupId == "par.juno.group.amp") return "Set the final amplifier level that leaves the synth voice";
  if (groupId == "par.juno.group.env") return "Control how the envelope rises, falls, holds, and releases over time";
  if (groupId == "par.juno.group.fx") return "Choose the built-in chorus mode that widens and thickens the Juno sound";
  return "Select a Juno section to edit its controls";
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
  if (parameter.id == "vcf.env") return "ENVM";
  if (parameter.id == "vcf.lfo") return "LFOM";
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

std::string JunoParameterHelpTitle(const SynthParameterInfo& parameter)
{
  if (parameter.id == "dco.range") return "DCO Range";
  if (parameter.id == "dco.saw") return "Saw Wave";
  if (parameter.id == "dco.sub") return "Sub Osc";
  if (parameter.id == "vcf.env") return "Env Amount";
  if (parameter.id == "vcf.lfo") return "LFO Amount";
  if (parameter.id == "lfo.delay") return "LFO Delay";
  if (parameter.id == "chorus.mode") return "Chorus Mode";
  if (parameter.id == "hpf.mode") return "HPF Mode";
  return parameter.label;
}

std::string JunoParameterHelpDescription(const SynthParameterInfo& parameter)
{
  if (parameter.id == "dco.range") return "Choose the oscillator octave range for a darker 16', balanced 8', or brighter 4' tone";
  if (parameter.id == "dco.pwm") return "Change the pulse wave width to move from hollow tones toward a richer nasal edge";
  if (parameter.id == "dco.saw") return "Turn the bright saw wave on or off in the oscillator mix";
  if (parameter.id == "dco.sub") return "Blend in the sub oscillator to add extra weight one octave below the main pitch";
  if (parameter.id == "dco.noise") return "Add noise to the oscillator mix for breath, attack, or synthetic texture";
  if (parameter.id == "vcf.cutoff") return "Move the filter cutoff to make the patch darker and softer or brighter and more open";
  if (parameter.id == "vcf.resonance") return "Emphasize the frequencies around the cutoff for a sharper, more vocal filter response";
  if (parameter.id == "vcf.env") return "Set how strongly the envelope pushes the filter open and closed over the note";
  if (parameter.id == "vcf.lfo") return "Set how much the LFO sweeps the filter cutoff over time";
  if (parameter.id == "vca.level") return "Set the final output level of the voice before chorus and mixdown";
  if (parameter.id == "env.attack") return "Control how quickly the envelope rises after a note starts";
  if (parameter.id == "env.decay") return "Control how quickly the envelope falls from the peak toward sustain";
  if (parameter.id == "env.sustain") return "Set the level the envelope holds while the key stays pressed";
  if (parameter.id == "env.release") return "Control how quickly the sound fades after you release the key";
  if (parameter.id == "lfo.rate") return "Set how fast the shared LFO cycles for vibrato or filter motion";
  if (parameter.id == "lfo.delay") return "Delay the LFO fade-in so modulation starts after the note has already spoken";
  if (parameter.id == "chorus.mode") return "Choose chorus off, mode I, or mode II to widen and thicken the sound";
  if (parameter.id == "hpf.mode") return "Choose how much low end the high-pass filter removes before the main filter stage";
  return {};
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

  if (groupId == "par.juno.group.hpf")
  {
    appendIfPresent("hpf.mode");
    return parameters;
  }

  if (groupId == "par.juno.group.vcf")
  {
    appendIfPresent("vcf.cutoff");
    appendIfPresent("vcf.resonance");
    appendIfPresent("vcf.env");
    appendIfPresent("vcf.lfo");
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
    return DescribeParameterValueOrFallback(engine, "dco.range") +
           "  PWM " + DescribeParameterValueOrFallback(engine, "dco.pwm") +
           "  SUB " + DescribeParameterValueOrFallback(engine, "dco.sub");
  }
  if (groupId == "par.juno.group.vcf")
  {
    return "CUT " + DescribeParameterValueOrFallback(engine, "vcf.cutoff") +
           "  RES " + DescribeParameterValueOrFallback(engine, "vcf.resonance") +
           "  ENV " + DescribeParameterValueOrFallback(engine, "vcf.env");
  }
  if (groupId == "par.juno.group.hpf")
  {
    return "MODE " + DescribeParameterValueOrFallback(engine, "hpf.mode");
  }
  if (groupId == "par.juno.group.env")
  {
    return "A " + DescribeParameterValueOrFallback(engine, "env.attack") +
           "  D " + DescribeParameterValueOrFallback(engine, "env.decay") +
           "  S " + DescribeParameterValueOrFallback(engine, "env.sustain") +
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
    return DescribeParameterValueOrFallback(engine, "chorus.mode");
  }
  return "SELECT SECTION";
}

std::string JunoOverviewSummary(const ISynthEngine* engine)
{
  return DescribeParameterValueOrFallback(engine, "dco.range") +
         "  CUT " + DescribeParameterValueOrFallback(engine, "vcf.cutoff") +
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

bool IsJunoDiscreteAdjustParameterId(const std::string& parameterId)
{
  return parameterId == "dco.range" || parameterId == "dco.saw";
}

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
  return PagedMenuPageCount(count);
}

}  // namespace synth::ui_model