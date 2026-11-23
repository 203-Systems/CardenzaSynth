#pragma once

// The SoundFont the on-board stress test plays (Applications/Synth/StressTest.cpp), written by it to the
// card: a patch, "Stress Keys" (0:0), that makes every note cost what a real one does, and the same an
// octave up (0:1), a second patch to switch to while it plays. Six looped keyboard
// samples split over the keys, and over all of them a looped pad layer through a filter moved by its
// own envelope, with vibrato: two regions, two voices, a note. About 170 KB, so four tracks can each
// read it (soundfont::kTrackBudgetBytes). Host tests read it back (tests/sf2_test.cpp).
//
// Written as it goes, a block of frames at a time: nothing of the size of the file is in memory.

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "Library/Synth/Audio/SoundFont/Sf2Format.h"

namespace synth::stress {

inline constexpr const char* kSoundFontFileName = "ZZ-ST-FONT.sf2";

namespace detail {

struct Out {
  std::vector<std::uint8_t> bytes;
  void U8(unsigned value) { bytes.push_back(static_cast<std::uint8_t>(value)); }
  void U16(unsigned value)
  {
    U8(value & 255u);
    U8((value >> 8) & 255u);
  }
  void U32(std::uint32_t value)
  {
    U16(value & 0xFFFFu);
    U16(value >> 16);
  }
  void Tag(const char* tag)
  {
    for (int index = 0; index < 4; ++index) U8(static_cast<unsigned char>(tag[index]));
  }
  void Name(const char* name, std::size_t width)
  {
    bool ended = false;
    for (std::size_t index = 0; index < width; ++index)
    {
      ended = ended || name[index] == 0;
      U8(ended ? 0 : static_cast<unsigned char>(name[index]));
    }
  }
  void Chunk(const char* tag, const Out& body)
  {
    Tag(tag);
    U32(static_cast<std::uint32_t>(body.bytes.size()));
    bytes.insert(bytes.end(), body.bytes.begin(), body.bytes.end());
    if ((body.bytes.size() & 1u) != 0) U8(0);
  }
};

struct Generator {
  unsigned op;
  int amount;
};

struct SampleSpec {
  const char* name;
  std::uint32_t frames;
  std::uint32_t loopStart;
  std::uint32_t loopEnd;
  std::uint8_t rootKey;
  std::int8_t correction;
  double period;  // frames
  bool pad;
};

constexpr std::uint32_t kKeyFrames = 13230;  // 0.3 s
constexpr std::uint32_t kPadFrames = 4096;
constexpr std::uint32_t kGuardFrames = 46;   // the zero frames the specification asks for after each sample

inline double PeriodOf(int key) { return 44100.0 / (440.0 * std::pow(2.0, (key - 69) / 12.0)); }

inline std::array<SampleSpec, 7> Samples()
{
  std::array<SampleSpec, 7> samples{};
  static const char* const kNames[] = {"KEY C2", "KEY C3", "KEY C4", "KEY C5", "KEY C6", "KEY C7"};
  for (int index = 0; index < 6; ++index)
  {
    const int root = 36 + 12 * index;
    const double period = PeriodOf(root);
    // A loop of whole periods (as near as frames allow) in the steady part after the decay.
    const std::uint32_t loopStart = 8820;
    const auto periods = static_cast<std::uint32_t>(4000.0 / period) + 1;
    const auto loopEnd = loopStart + static_cast<std::uint32_t>(std::lround(periods * period));
    samples[static_cast<std::size_t>(index)] = {kNames[index], kKeyFrames, loopStart, loopEnd, static_cast<std::uint8_t>(root), 0, period, false};
  }
  // 32 periods of 128 frames: 344.5 Hz, F4 23 cents flat; loops seamlessly whole.
  samples[6] = {"PAD", kPadFrames, 0, kPadFrames, 65, -23, 128.0, true};
  return samples;
}

inline std::int16_t SampleFrame(const SampleSpec& sample, std::uint32_t frame)
{
  const double phase = 2.0 * 3.14159265358979323846 * static_cast<double>(frame) / sample.period;
  if (sample.pad)
  {
    // A band-limited ramp: eight harmonics.
    double value = 0.0;
    for (int harmonic = 1; harmonic <= 8; ++harmonic) value += std::sin(phase * harmonic) / harmonic;
    return static_cast<std::int16_t>(std::lround(9000.0 * value));
  }
  // A struck tone settling to a third of its level before the loop.
  const double settle = frame < sample.loopStart ? 1.0 - 0.7 * static_cast<double>(frame) / sample.loopStart : 0.3;
  const double value = std::sin(phase) + 0.5 * std::sin(2.0 * phase) + 0.25 * std::sin(3.0 * phase);
  return static_cast<std::int16_t>(std::lround(14000.0 * settle * value));
}

}  // namespace detail

// False when a write failed (the file is then incomplete: the caller removes it).
inline bool WriteStressSoundFont(std::FILE* file)
{
  using namespace detail;
  namespace sf2 = audio::sf2;
  const std::array<SampleSpec, 7> samples = Samples();

  // The INFO list.
  Out ifil;
  ifil.U16(2);
  ifil.U16(4);
  Out engine;
  engine.Name("EMU8000", 8);
  Out title;
  title.Name("Stress Font", 12);
  Out infoBody;
  infoBody.Tag("INFO");
  infoBody.Chunk("ifil", ifil);
  infoBody.Chunk("isng", engine);
  infoBody.Chunk("INAM", title);

  // Where each sample starts in the sample data.
  std::array<std::uint32_t, 7> starts{};
  std::uint32_t frames = 0;
  for (std::size_t index = 0; index < samples.size(); ++index)
  {
    starts[index] = frames;
    frames += samples[index].frames + kGuardFrames;
  }

  // The preset data: two presets of one instrument of seven zones.
  std::vector<std::vector<Generator>> zones;
  static const int kSplits[6][2] = {{0, 41}, {42, 53}, {54, 65}, {66, 77}, {78, 89}, {90, 127}};
  for (int index = 0; index < 6; ++index)
  {
    // Keys: loud, a short attack, decaying to a sustain 12 dB down, 0.3 s release.
    zones.push_back({{sf2::kKeyRange, kSplits[index][0] | (kSplits[index][1] << 8)},
                     {sf2::kAttackVolEnv, -7973},
                     {sf2::kDecayVolEnv, 1200},
                     {sf2::kSustainVolEnv, 120},
                     {sf2::kReleaseVolEnv, -2084},
                     {sf2::kSampleModes, 1},
                     {sf2::kSampleId, index}});
  }
  // The pad: quieter, its filter opened by its own envelope and kept moving by vibrato, so every
  // control step works out new coefficients.
  zones.push_back({{sf2::kKeyRange, 0 | (127 << 8)},
                   {sf2::kInitialFilterFc, 7800},
                   {sf2::kInitialFilterQ, 80},
                   {sf2::kModEnvToFilterFc, 3000},
                   {sf2::kAttackModEnv, -3986},
                   {sf2::kDecayModEnv, 1200},
                   {sf2::kVibLfoToPitch, 25},
                   {sf2::kFreqVibLfo, 0},
                   {sf2::kInitialAttenuation, 150},
                   {sf2::kAttackVolEnv, -3986},
                   {sf2::kReleaseVolEnv, -1200},
                   {sf2::kSampleModes, 1},
                   {sf2::kSampleId, 6}});

  Out phdr, pbag, pmod, pgen, inst, ibag, imod, igen, shdr;
  const auto preset = [&](const char* name, unsigned program, unsigned bag) {
    phdr.Name(name, 20);
    phdr.U16(program);
    phdr.U16(0);  // bank
    phdr.U16(bag);
    phdr.U32(0);
    phdr.U32(0);
    phdr.U32(0);
  };
  preset("Stress Keys", 0, 0);
  preset("Stress Keys Up", 1, 1);
  preset("EOP", 0, 2);
  // 0:0 is the instrument; 0:1 the instrument an octave up (a preset's generators add to it).
  pbag.U16(0);
  pbag.U16(0);
  pbag.U16(1);
  pbag.U16(0);
  pbag.U16(3);
  pbag.U16(0);
  for (int index = 0; index < 10; ++index) pmod.U8(0);
  pgen.U16(sf2::kInstrument);
  pgen.U16(0);
  pgen.U16(sf2::kCoarseTune);
  pgen.U16(12);
  pgen.U16(sf2::kInstrument);
  pgen.U16(0);
  pgen.U32(0);

  inst.Name("Stress Keys", 20);
  inst.U16(0);
  unsigned generators = 0;
  for (const auto& zone : zones)
  {
    ibag.U16(generators);
    ibag.U16(0);
    for (const Generator& generator : zone)
    {
      igen.U16(generator.op);
      igen.U16(static_cast<std::uint16_t>(static_cast<std::int16_t>(generator.amount)));
      ++generators;
    }
  }
  inst.Name("EOI", 20);
  inst.U16(static_cast<unsigned>(zones.size()));
  ibag.U16(generators);
  ibag.U16(0);
  for (int index = 0; index < 10; ++index) imod.U8(0);
  igen.U32(0);

  for (std::size_t index = 0; index < samples.size(); ++index)
  {
    const SampleSpec& sample = samples[index];
    shdr.Name(sample.name, 20);
    shdr.U32(starts[index]);
    shdr.U32(starts[index] + sample.frames);
    shdr.U32(starts[index] + sample.loopStart);
    shdr.U32(starts[index] + sample.loopEnd);
    shdr.U32(44100);
    shdr.U8(sample.rootKey);
    shdr.U8(static_cast<std::uint8_t>(sample.correction));
    shdr.U16(0);
    shdr.U16(1);  // mono
  }
  shdr.Name("EOS", 20);
  for (int index = 0; index < 5; ++index) shdr.U32(0);
  shdr.U32(0);
  shdr.U16(0);

  Out pdtaBody;
  pdtaBody.Tag("pdta");
  pdtaBody.Chunk("phdr", phdr);
  pdtaBody.Chunk("pbag", pbag);
  pdtaBody.Chunk("pmod", pmod);
  pdtaBody.Chunk("pgen", pgen);
  pdtaBody.Chunk("inst", inst);
  pdtaBody.Chunk("ibag", ibag);
  pdtaBody.Chunk("imod", imod);
  pdtaBody.Chunk("igen", igen);
  pdtaBody.Chunk("shdr", shdr);

  const std::uint32_t smplBytes = frames * 2;
  const std::uint32_t sdtaSize = 4 + 8 + smplBytes;
  const auto infoSize = static_cast<std::uint32_t>(infoBody.bytes.size());
  const auto pdtaSize = static_cast<std::uint32_t>(pdtaBody.bytes.size());

  Out head;
  head.Tag("RIFF");
  head.U32(4 + (8 + infoSize) + (8 + sdtaSize) + (8 + pdtaSize));
  head.Tag("sfbk");
  head.Tag("LIST");
  head.U32(infoSize);
  head.bytes.insert(head.bytes.end(), infoBody.bytes.begin(), infoBody.bytes.end());
  head.Tag("LIST");
  head.U32(sdtaSize);
  head.Tag("sdta");
  head.Tag("smpl");
  head.U32(smplBytes);
  if (std::fwrite(head.bytes.data(), 1, head.bytes.size(), file) != head.bytes.size()) return false;

  std::array<std::uint8_t, 1024> block{};
  std::size_t used = 0;
  const auto put = [&](std::int16_t value) {
    const auto bits = static_cast<std::uint16_t>(value);
    block[used++] = static_cast<std::uint8_t>(bits & 255u);
    block[used++] = static_cast<std::uint8_t>(bits >> 8);
    if (used < block.size()) return true;
    used = 0;
    return std::fwrite(block.data(), 1, block.size(), file) == block.size();
  };
  for (const SampleSpec& sample : samples)
  {
    for (std::uint32_t frame = 0; frame < sample.frames; ++frame)
      if (!put(SampleFrame(sample, frame))) return false;
    for (std::uint32_t frame = 0; frame < kGuardFrames; ++frame)
      if (!put(0)) return false;
  }
  if (used != 0 && std::fwrite(block.data(), 1, used, file) != used) return false;

  Out tail;
  tail.Tag("LIST");
  tail.U32(pdtaSize);
  tail.bytes.insert(tail.bytes.end(), pdtaBody.bytes.begin(), pdtaBody.bytes.end());
  return std::fwrite(tail.bytes.data(), 1, tail.bytes.size(), file) == tail.bytes.size();
}

}  // namespace synth::stress
