#pragma once

// Writes small SoundFont (.sf2) files for tests: samples, instruments and presets given as lists of
// generators, laid out as the SoundFont 2.04 specification has them. No third-party font is used
// anywhere in the tests.

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "Library/Synth/Audio/SoundFont/Sf2Format.h"

namespace sf2test {

struct Bytes {
  std::vector<std::uint8_t> data;
  void U8(unsigned value) { data.push_back(static_cast<std::uint8_t>(value)); }
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
    for (int index = 0; index < 4; ++index) U8(static_cast<std::uint8_t>(tag[index]));
  }
  void Name(const std::string& name, std::size_t width = 20)
  {
    for (std::size_t index = 0; index < width; ++index) U8(index < name.size() ? static_cast<std::uint8_t>(name[index]) : 0);
  }
  void Append(const Bytes& other) { data.insert(data.end(), other.data.begin(), other.data.end()); }
};

inline Bytes Chunk(const char* tag, const Bytes& body)
{
  Bytes out;
  out.Tag(tag);
  out.U32(static_cast<std::uint32_t>(body.data.size()));
  out.Append(body);
  if ((body.data.size() & 1u) != 0) out.U8(0);
  return out;
}

inline Bytes List(const char* type, const std::vector<Bytes>& chunks)
{
  Bytes body;
  body.Tag(type);
  for (const Bytes& chunk : chunks) body.Append(chunk);
  Bytes out;
  out.Tag("LIST");
  out.U32(static_cast<std::uint32_t>(body.data.size()));
  out.Append(body);
  return out;
}

struct Gen {
  std::uint16_t op;
  std::uint16_t amount;
};
inline Gen G(unsigned op, int value) { return {static_cast<std::uint16_t>(op), static_cast<std::uint16_t>(static_cast<std::int16_t>(value))}; }
inline Gen Range(unsigned op, unsigned low, unsigned high) { return {static_cast<std::uint16_t>(op), static_cast<std::uint16_t>(low | (high << 8))}; }

struct SampleSpec {
  std::string name;
  std::vector<std::int16_t> frames;
  std::uint32_t loopStart = 0;  // from the sample's first frame
  std::uint32_t loopEnd = 0;
  std::uint32_t rate = 44100;
  std::uint8_t pitch = 60;
  std::int8_t correction = 0;
  std::uint16_t link = 0;
  std::uint16_t type = 1;
};
using ZoneSpec = std::vector<Gen>;
struct InstrumentSpec {
  std::string name;
  std::vector<ZoneSpec> zones;
};
struct PresetSpec {
  std::string name;
  std::uint16_t program = 0;
  std::uint16_t bank = 0;
  std::vector<ZoneSpec> zones;
};

struct FontSpec {
  unsigned version = 2;
  std::vector<SampleSpec> samples;
  std::vector<InstrumentSpec> instruments;
  std::vector<PresetSpec> presets;

  std::vector<std::uint8_t> Build() const
  {
    Bytes ifil;
    ifil.U16(version);
    ifil.U16(4);
    Bytes engine;
    engine.Name("EMU8000", 8);
    Bytes name;
    name.Name("Test font", 10);

    Bytes smpl;
    std::vector<std::uint32_t> starts;
    std::uint32_t frame = 0;
    for (const SampleSpec& sample : samples)
    {
      starts.push_back(frame);
      for (std::int16_t value : sample.frames) smpl.U16(static_cast<std::uint16_t>(value));
      for (int pad = 0; pad < 46; ++pad) smpl.U16(0);  // the specification's zero frames after each sample
      frame += static_cast<std::uint32_t>(sample.frames.size()) + 46;
    }

    Bytes phdr, pbag, pmod, pgen, inst, ibag, imod, igen, shdr;
    unsigned bag = 0;
    unsigned gen = 0;
    for (const PresetSpec& preset : presets)
    {
      phdr.Name(preset.name);
      phdr.U16(preset.program);
      phdr.U16(preset.bank);
      phdr.U16(bag);
      phdr.U32(0);
      phdr.U32(0);
      phdr.U32(0);
      for (const ZoneSpec& zone : preset.zones)
      {
        pbag.U16(gen);
        pbag.U16(0);
        ++bag;
        for (const Gen& g : zone)
        {
          pgen.U16(g.op);
          pgen.U16(g.amount);
          ++gen;
        }
      }
    }
    phdr.Name("EOP");
    phdr.U16(0);
    phdr.U16(0);
    phdr.U16(bag);
    phdr.U32(0);
    phdr.U32(0);
    phdr.U32(0);
    pbag.U16(gen);
    pbag.U16(0);
    for (int index = 0; index < 10; ++index) pmod.U8(0);
    pgen.U32(0);

    bag = 0;
    gen = 0;
    for (const InstrumentSpec& instrument : instruments)
    {
      inst.Name(instrument.name);
      inst.U16(bag);
      for (const ZoneSpec& zone : instrument.zones)
      {
        ibag.U16(gen);
        ibag.U16(0);
        ++bag;
        for (const Gen& g : zone)
        {
          igen.U16(g.op);
          igen.U16(g.amount);
          ++gen;
        }
      }
    }
    inst.Name("EOI");
    inst.U16(bag);
    ibag.U16(gen);
    ibag.U16(0);
    for (int index = 0; index < 10; ++index) imod.U8(0);
    igen.U32(0);

    for (std::size_t index = 0; index < samples.size(); ++index)
    {
      const SampleSpec& sample = samples[index];
      shdr.Name(sample.name);
      shdr.U32(starts[index]);
      shdr.U32(starts[index] + static_cast<std::uint32_t>(sample.frames.size()));
      shdr.U32(starts[index] + sample.loopStart);
      shdr.U32(starts[index] + sample.loopEnd);
      shdr.U32(sample.rate);
      shdr.U8(sample.pitch);
      shdr.U8(static_cast<std::uint8_t>(sample.correction));
      shdr.U16(sample.link);
      shdr.U16(sample.type);
    }
    shdr.Name("EOS");
    for (int index = 0; index < 5; ++index) shdr.U32(0);
    shdr.U32(0);  // pitch, correction, link
    shdr.U16(0);  // type

    Bytes body;
    body.Tag("sfbk");
    body.Append(List("INFO", {Chunk("ifil", ifil), Chunk("isng", engine), Chunk("INAM", name)}));
    body.Append(List("sdta", {Chunk("smpl", smpl)}));
    body.Append(List("pdta", {Chunk("phdr", phdr), Chunk("pbag", pbag), Chunk("pmod", pmod), Chunk("pgen", pgen), Chunk("inst", inst),
                              Chunk("ibag", ibag), Chunk("imod", imod), Chunk("igen", igen), Chunk("shdr", shdr)}));
    Bytes riff;
    riff.Tag("RIFF");
    riff.U32(static_cast<std::uint32_t>(body.data.size()));
    riff.Append(body);
    return riff.data;
  }
};

inline std::vector<std::int16_t> Sine(double period, std::size_t frames, double amplitude)
{
  std::vector<std::int16_t> out(frames);
  for (std::size_t frame = 0; frame < frames; ++frame)
    out[frame] = static_cast<std::int16_t>(std::lround(amplitude * std::sin(2.0 * 3.14159265358979323846 * static_cast<double>(frame) / period)));
  return out;
}

inline std::vector<std::int16_t> Constant(std::int16_t value, std::size_t frames) { return std::vector<std::int16_t>(frames, value); }

}  // namespace sf2test
