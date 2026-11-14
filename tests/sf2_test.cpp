// The SoundFont engine on the host: its integer conversions against the formulas, the file reader and
// the preset flattening against small fonts written here, the voice against a double-precision model
// of the same algorithm, the envelopes, the filter and the pitch against the SoundFont's units, the
// track's rules between notes, and damaged files (truncated, bytes changed) that must never crash it.
// No third-party SoundFont is used.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "Library/Synth/Audio/SoundFont/Sf2File.h"
#include "Library/Synth/Audio/SoundFont/Sf2Program.h"
#include "Library/Synth/Audio/SoundFont/Sf2Tables.h"
#include "Library/Synth/Audio/SoundFont/Sf2Track.h"
#include "Library/Synth/Audio/SoundFont/Sf2Voice.h"

namespace sf2 = synth::audio::sf2;

namespace {

int g_failures = 0;
int g_checks = 0;

#define CHECK(condition, ...)                                         \
  do                                                                  \
  {                                                                   \
    ++g_checks;                                                       \
    if (!(condition))                                                 \
    {                                                                 \
      ++g_failures;                                                   \
      std::printf("FAIL %s:%d: %s: ", __FILE__, __LINE__, #condition); \
      std::printf(__VA_ARGS__);                                       \
      std::printf("\n");                                              \
    }                                                                 \
  } while (0)

constexpr double kPi = 3.14159265358979323846;
constexpr double kKeyZeroHz = 8.1757989156;
constexpr double kRate = 44100.0;

// ---------------------------------------------------------------------------------------------------
// Writing SoundFonts

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

Bytes Chunk(const char* tag, const Bytes& body)
{
  Bytes out;
  out.Tag(tag);
  out.U32(static_cast<std::uint32_t>(body.data.size()));
  out.Append(body);
  if ((body.data.size() & 1u) != 0) out.U8(0);
  return out;
}

Bytes List(const char* type, const std::vector<Bytes>& chunks)
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
Gen G(unsigned op, int value) { return {static_cast<std::uint16_t>(op), static_cast<std::uint16_t>(static_cast<std::int16_t>(value))}; }
Gen Range(unsigned op, unsigned low, unsigned high) { return {static_cast<std::uint16_t>(op), static_cast<std::uint16_t>(low | (high << 8))}; }

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

std::vector<std::int16_t> Sine(double period, std::size_t frames, double amplitude)
{
  std::vector<std::int16_t> out(frames);
  for (std::size_t frame = 0; frame < frames; ++frame) out[frame] = static_cast<std::int16_t>(std::lround(amplitude * std::sin(2.0 * kPi * frame / period)));
  return out;
}

std::vector<std::int16_t> Constant(std::int16_t value, std::size_t frames) { return std::vector<std::int16_t>(frames, value); }

std::uint32_t g_random = 0x2468ACE1u;
std::uint32_t Random()
{
  g_random = g_random * 1664525u + 1013904223u;
  return g_random;
}

std::vector<std::int16_t> Noise(std::size_t frames, int amplitude)
{
  std::vector<std::int16_t> out(frames);
  for (auto& value : out) value = static_cast<std::int16_t>(static_cast<int>(Random() >> 16) % (2 * amplitude + 1) - amplitude);
  return out;
}

// One preset, one instrument, one zone playing one sample.
std::vector<std::uint8_t> SingleFont(const SampleSpec& sample, ZoneSpec zone)
{
  FontSpec font;
  font.samples.push_back(sample);
  zone.push_back(G(sf2::kSampleId, 0));
  font.instruments.push_back({"Single", {zone}});
  font.presets.push_back({"Single", 0, 0, {{G(sf2::kInstrument, 0)}}});
  return font.Build();
}

// A font in memory, opened.
struct Font {
  std::vector<std::uint8_t> bytes;
  sf2::MemorySource source;
  std::uint8_t window[2048] = {};
  sf2::Reader reader;
  sf2::File file;
  sf2::Status status = sf2::Status::Ok;

  explicit Font(std::vector<std::uint8_t> data)
      : bytes(std::move(data)), source(bytes.data(), bytes.size()), reader(source, window, sizeof(window))
  {
    status = file.Open(reader);
  }
};

std::unique_ptr<Font> Open(std::vector<std::uint8_t> bytes) { return std::make_unique<Font>(std::move(bytes)); }

std::unique_ptr<sf2::Program> BuildPreset(Font& font, unsigned bank, unsigned program, std::size_t budget = 8u << 20)
{
  const int position = font.file.FindPreset(bank, program);
  if (position < 0) return nullptr;
  sf2::Program::Result result = sf2::Program::Build(font.file, static_cast<std::size_t>(position), budget);
  return std::move(result.program);
}

double Snr(const std::vector<double>& reference, const std::vector<double>& actual, std::size_t from, std::size_t to)
{
  double signal = 0.0;
  double noise = 0.0;
  for (std::size_t frame = from; frame < to; ++frame)
  {
    signal += reference[frame] * reference[frame];
    noise += (reference[frame] - actual[frame]) * (reference[frame] - actual[frame]);
  }
  if (noise == 0.0) return 200.0;
  return 10.0 * std::log10(signal / noise);
}

double Rms(const std::vector<double>& values, std::size_t from, std::size_t to)
{
  double sum = 0.0;
  for (std::size_t frame = from; frame < to; ++frame) sum += values[frame] * values[frame];
  return std::sqrt(sum / static_cast<double>(to - from));
}

double RmsError(const std::vector<double>& reference, const std::vector<double>& actual, std::size_t from, std::size_t to)
{
  double sum = 0.0;
  for (std::size_t frame = from; frame < to; ++frame) sum += (reference[frame] - actual[frame]) * (reference[frame] - actual[frame]);
  return std::sqrt(sum / static_cast<double>(to - from));
}

// Renders a voice a control step at a time; `endedAt`: the first frame after which it was off.
std::vector<double> RenderVoice(sf2::Voice& voice, std::size_t frames, std::size_t releaseAt = SIZE_MAX, std::vector<std::uint32_t>* steps = nullptr,
                                std::size_t* endedAt = nullptr)
{
  std::vector<std::int32_t> out(frames, 0);
  if (endedAt != nullptr) *endedAt = SIZE_MAX;
  for (std::size_t frame = 0; frame < frames; frame += 64)
  {
    if (frame == releaseAt) voice.Release();
    const std::size_t count = (std::min)(std::size_t{64}, frames - frame);
    voice.Render(out.data() + frame, count);
    if (steps != nullptr) steps->push_back(voice.PositionStepQ16());
    if (endedAt != nullptr && *endedAt == SIZE_MAX && !voice.Active()) *endedAt = frame + count;
  }
  return std::vector<double>(out.begin(), out.end());
}

// ---------------------------------------------------------------------------------------------------
// The tests

void TestTables()
{
  double worst = 0.0;
  for (int centsQ8 = -12000 * 256; centsQ8 <= 12000 * 256; centsQ8 += 9473)
  {
    const double exact = std::ldexp(std::pow(2.0, centsQ8 / 256.0 / 1200.0), 40);
    const double got = static_cast<double>(sf2::Exp2(centsQ8, 40));
    worst = (std::max)(worst, std::fabs(got - exact) / exact);
  }
  CHECK(worst < 2e-7, "Exp2 relative error %g", worst);

  int gainError = 0;
  for (int quarter = 0; quarter <= 1440 * 4; ++quarter)
  {
    const double cb = quarter / 4.0;
    const double exact = 32768.0 * std::pow(10.0, -cb / 200.0);
    const int got = static_cast<int>(sf2::GainQ15(static_cast<std::int32_t>(cb * 4096)));
    gainError = (std::max)(gainError, static_cast<int>(std::lround(std::fabs(got - exact))));
  }
  CHECK(gainError <= 1, "GainQ15 error %d", gainError);
  CHECK(sf2::GainQ15(-5) == 32768 && sf2::GainQ15(1440 << 12) == 0, "GainQ15 ends");

  // Where a release from the attack uses it: levels a listener hears (gain units of 1/32768 are
  // coarse below that).
  double inverse = 0.0;
  for (int cb = 0; cb <= 300; ++cb)
  {
    const double back = sf2::AttenuationOfGain(sf2::GainQ15(cb << 12)) / 4096.0;
    inverse = (std::max)(inverse, std::fabs(back - cb));
  }
  CHECK(inverse < 0.5, "AttenuationOfGain off by %g cB", inverse);

  double stepsError = 0.0;
  for (int tc = -12000; tc <= 8000; tc += 113)
  {
    const double exact = std::pow(2.0, tc / 1200.0) * kRate / 64.0 * 256.0;
    stepsError = (std::max)(stepsError, std::fabs(sf2::ControlStepsQ8(tc) - exact) - 1e-4 * exact);
  }
  CHECK(stepsError < 1.0, "ControlStepsQ8 off by %g beyond 1e-4", stepsError);

  bool monotonic = true;
  for (unsigned velocity = 1; velocity < 128; ++velocity) monotonic = monotonic && sf2::VelocityAttenuationQ12(velocity) <= sf2::VelocityAttenuationQ12(velocity - 1);
  CHECK(monotonic && sf2::VelocityAttenuationQ12(127) == 0, "velocity curve");
  const double vel64 = sf2::VelocityAttenuationQ12(64) / 4096.0;
  CHECK(std::fabs(vel64 - 400.0 * std::log10(127.0 / 64.0)) < 0.1, "velocity 64 is %g cB", vel64);

  double coefficientError = 0.0;
  for (int index = 0; index < 480; index += 7)
  {
    const int cents = 1500 + 25 * index;
    for (int resonance : {0, 60, 120, 230})
    {
      const double hz = (std::min)(kKeyZeroHz * std::pow(2.0, cents / 1200.0), 0.45 * kRate);
      const double g = std::tan(kPi * hz / kRate);
      const double k = 1.0 / std::pow(10.0, (resonance / 10.0 - 3.01) / 20.0);
      const double a1 = 1.0 / (1.0 + g * (g + k));
      const sf2::FilterCoefficients got = sf2::FilterFor(cents * 256, resonance);
      coefficientError = (std::max)(coefficientError, std::fabs(got.a1 / 1073741824.0 - a1));
      coefficientError = (std::max)(coefficientError, std::fabs(got.a2 / 1073741824.0 - g * a1));
      coefficientError = (std::max)(coefficientError, std::fabs(got.a3 / 1073741824.0 - g * g * a1));
    }
  }
  CHECK(coefficientError < 2e-6, "filter coefficients off by %g", coefficientError);
}

FontSpec MergeFont()
{
  FontSpec font;
  SampleSpec sine;
  sine.name = "sine100";
  sine.frames = Sine(100.0, 1000, 16000.0);
  sine.loopStart = 200;
  sine.loopEnd = 1000;
  sine.pitch = 69;
  sine.correction = 7;
  font.samples.push_back(sine);
  SampleSpec dc;
  dc.name = "dc";
  dc.frames = Constant(16384, 4000);
  dc.loopStart = 1000;
  dc.loopEnd = 3000;
  font.samples.push_back(dc);
  SampleSpec left;
  left.name = "left";
  left.frames = Sine(50.0, 500, 8000.0);
  left.type = 4;
  left.link = 3;
  font.samples.push_back(left);
  SampleSpec right = left;
  right.name = "right";
  right.type = 2;
  right.link = 2;
  font.samples.push_back(right);

  font.instruments.push_back({"Sine", {{G(sf2::kSampleModes, 1), G(sf2::kSampleId, 0)}}});
  font.instruments.push_back({"Layered",
                              {{G(sf2::kInitialAttenuation, 100), G(sf2::kCoarseTune, 2)},
                               {Range(sf2::kKeyRange, 0, 59), G(sf2::kInitialAttenuation, 50), G(sf2::kSampleModes, 1), G(sf2::kSampleId, 0)},
                               {Range(sf2::kKeyRange, 60, 127), G(sf2::kSampleModes, 1), G(sf2::kSampleId, 0)}}});
  font.instruments.push_back({"Stereo", {{G(sf2::kSampleId, 2)}, {G(sf2::kSampleId, 3)}}});
  font.instruments.push_back({"Kit",
                              {{Range(sf2::kKeyRange, 42, 42), G(sf2::kExclusiveClass, 1), G(sf2::kSampleModes, 1), G(sf2::kSampleId, 1)},
                               {Range(sf2::kKeyRange, 46, 46), G(sf2::kExclusiveClass, 1), G(sf2::kSampleModes, 1), G(sf2::kSampleId, 1)},
                               {Range(sf2::kKeyRange, 36, 36), G(sf2::kSampleModes, 1), G(sf2::kSampleId, 1)}}});

  // Out of bank and program order on purpose.
  font.presets.push_back({"Merge",
                          1,
                          0,
                          {{G(sf2::kCoarseTune, 1)},
                           {Range(sf2::kKeyRange, 40, 80), Range(sf2::kVelRange, 0, 100), G(sf2::kFineTune, 10), G(sf2::kSampleModes, 0),
                            G(sf2::kOverridingRootKey, 30), G(sf2::kInstrument, 1)}}});
  font.presets.push_back({"Kit", 0, 128, {{G(sf2::kInstrument, 3)}}});
  font.presets.push_back({"Stereo", 2, 0, {{G(sf2::kInstrument, 2)}}});
  font.presets.push_back({"Variation", 0, 1, {{G(sf2::kInstrument, 0)}}});
  font.presets.push_back({"Layers", 3, 0, {{G(sf2::kInstrument, 0)}, {G(sf2::kInstrument, 0)}, {G(sf2::kInstrument, 0)}}});
  font.presets.push_back({"Piano  ", 0, 0, {{G(sf2::kInstrument, 0)}}});
  return font;
}

void TestFile()
{
  auto font = Open(MergeFont().Build());
  CHECK(font->status == sf2::Status::Ok, "open status %d", static_cast<int>(font->status));
  CHECK(font->file.PresetCount() == 6, "presets %zu", font->file.PresetCount());
  const unsigned order[6][2] = {{0, 0}, {0, 1}, {0, 2}, {0, 3}, {1, 0}, {128, 0}};
  for (std::size_t position = 0; position < 6 && position < font->file.PresetCount(); ++position)
    CHECK(font->file.Preset(position).bank == order[position][0] && font->file.Preset(position).program == order[position][1], "preset %zu is %u:%u", position,
          font->file.Preset(position).bank, font->file.Preset(position).program);
  char name[21];
  CHECK(font->file.PresetName(0, name) && std::string(name) == "Piano", "name '%s' (trailing spaces go)", name);
  CHECK(font->file.FindPreset(128, 0) == 5 && font->file.FindPreset(7, 7) == -1, "find preset");

  FontSpec three = MergeFont();
  three.version = 3;
  CHECK(Open(three.Build())->status == sf2::Status::Unsupported, "an SF3 file is refused");
  std::vector<std::uint8_t> notRiff = MergeFont().Build();
  notRiff[0] = 'X';
  CHECK(Open(notRiff)->status == sf2::Status::NotSoundFont, "not a RIFF file");
  CHECK(Open({})->status == sf2::Status::NotSoundFont, "an empty file");
}

void TestFlatten()
{
  auto font = Open(MergeFont().Build());
  auto merge = BuildPreset(*font, 0, 1);
  CHECK(merge != nullptr, "merge preset builds");
  if (merge)
  {
    CHECK(merge->RegionCount() == 2, "regions %zu", merge->RegionCount());
    const sf2::Region& low = merge->RegionAt(0);
    const sf2::Region& high = merge->RegionAt(1);
    // Ranges intersect.
    CHECK(low.keyLow == 40 && low.keyHigh == 59 && low.velLow == 0 && low.velHigh == 100, "low range %u-%u %u-%u", low.keyLow, low.keyHigh, low.velLow, low.velHigh);
    CHECK(high.keyLow == 60 && high.keyHigh == 80, "high range %u-%u", high.keyLow, high.keyHigh);
    // The instrument's own zone replaces its global zone; the global zone stands in for the other.
    // Times 0.4 (docs/sf2-engine-design.md).
    CHECK(low.attenuationCb == 20 && high.attenuationCb == 40, "attenuation %d %d", low.attenuationCb, high.attenuationCb);
    // The preset adds: coarse 2 (instrument global) + 1 (preset global), fine 10, and the sample's
    // correction 7.
    CHECK(low.tuneCents == 317 && high.tuneCents == 317, "tune %d %d", low.tuneCents, high.tuneCents);
    // Generators a preset may not carry are ignored there.
    CHECK(low.mode == 1 && low.rootKey == 69, "mode %u root %u", low.mode, low.rootKey);
    CHECK(merge->SlotCount() == 1 && merge->SlotFrames(0) == 1000, "one slot of the loop's frames: %zu %u", merge->SlotCount(), merge->SlotFrames(0));
    const sf2::Region* found[2];
    CHECK(merge->Find(50, 90, found, 2) == 1 && found[0] == &low, "key 50");
    CHECK(merge->Find(50, 110, found, 2) == 0, "velocity out of range");
    CHECK(merge->Find(90, 64, found, 2) == 0, "key out of range");
  }

  auto stereo = BuildPreset(*font, 0, 2);
  CHECK(stereo && stereo->RegionCount() == 1, "of a stereo pair one region stays");

  auto layers = BuildPreset(*font, 0, 3);
  const sf2::Region* found[4];
  CHECK(layers && layers->RegionCount() == 3 && layers->Find(60, 100, found, 2) == 2, "layers are found up to the capacity");

  const int position = font->file.FindPreset(0, 1);
  const sf2::Program::Result small = sf2::Program::Build(font->file, static_cast<std::size_t>(position), 256);
  CHECK(small.status == sf2::Status::TooBig && small.neededBytes > 2000 && !small.program, "too big: %d, %zu bytes", static_cast<int>(small.status), small.neededBytes);
  CHECK(sf2::Program::Build(font->file, 99, 1u << 20).status == sf2::Status::NotFound, "no such preset");

  // Loop points: reversed are swapped, outside the sample are held in it, too short is no loop.
  SampleSpec dc;
  dc.name = "dc";
  dc.frames = Constant(16384, 4000);
  dc.loopStart = 1000;
  dc.loopEnd = 3000;
  {
    auto swapped = Open(SingleFont(dc, {G(sf2::kSampleModes, 1), G(sf2::kStartloopAddrsOffset, 2500)}));
    auto program = BuildPreset(*swapped, 0, 0);
    CHECK(program && program->RegionAt(0).loopStart == 3000 && program->RegionAt(0).loopEnd == 3500 && program->RegionAt(0).mode == 1, "reversed loop");
  }
  {
    auto clamped = Open(SingleFont(dc, {G(sf2::kSampleModes, 1), G(sf2::kEndloopAddrsCoarseOffset, 1)}));
    auto program = BuildPreset(*clamped, 0, 0);
    CHECK(program && program->RegionAt(0).loopEnd == 4000, "loop end held in the sample");
  }
  {
    auto tiny = Open(SingleFont(dc, {G(sf2::kSampleModes, 1), G(sf2::kEndloopAddrsOffset, -1999)}));
    auto program = BuildPreset(*tiny, 0, 0);
    CHECK(program && program->RegionAt(0).mode == 0 && program->SlotFrames(0) == 4000, "a one-frame loop is no loop");
  }
  {
    // A sample past the end of the sample data is left out, and with it the preset is empty.
    FontSpec broken;
    broken.samples.push_back(dc);
    broken.instruments.push_back({"Broken", {{G(sf2::kSampleId, 5)}}});
    broken.presets.push_back({"Broken", 0, 0, {{G(sf2::kInstrument, 0)}}});
    auto font2 = Open(broken.Build());
    const sf2::Program::Result result = sf2::Program::Build(font2->file, 0, 1u << 20);
    CHECK(result.status == sf2::Status::Empty, "a preset of missing samples: %d", static_cast<int>(result.status));
  }
}

void TestPitch()
{
  SampleSpec sine;
  sine.name = "sine";
  sine.frames = Sine(100.0, 1000, 16000.0);
  sine.loopStart = 200;
  sine.loopEnd = 1000;
  sine.pitch = 69;
  sine.rate = 22050;
  sine.correction = -17;
  auto font = Open(SingleFont(sine, {G(sf2::kSampleModes, 1), G(sf2::kScaleTuning, 50), G(sf2::kCoarseTune, -3), G(sf2::kFineTune, 25)}));
  auto program = BuildPreset(*font, 0, 0);
  CHECK(program != nullptr, "pitch font");
  if (!program) return;
  // Within 0.03 cents of the formula, plus what a 16.16 step can say (it gets coarse when a sample
  // plays octaves down).
  double worst = -1e9;
  for (unsigned key = 0; key < 128; ++key)
  {
    sf2::Voice voice;
    voice.Start(program->RegionAt(0), program->SlotData(0), key, 100, {}, 1);
    const double cents = 50.0 * (static_cast<double>(key) - 69.0) - 300.0 + 25.0 - 17.0;
    const double exact = std::pow(2.0, cents / 1200.0) * 22050.0 / kRate;
    const double got = voice.PositionStepQ16() / 65536.0;
    const double allowed = 0.03 + 1200.0 * std::log2(1.0 + 1.0 / (exact * 65536.0));
    worst = (std::max)(worst, std::fabs(1200.0 * std::log2(got / exact)) - allowed);
  }
  CHECK(worst < 0.0, "pitch off by %g cents more than allowed", worst);

  auto fixed = Open(SingleFont(sine, {G(sf2::kSampleModes, 1), G(sf2::kKeynum, 69)}));
  auto fixedProgram = BuildPreset(*fixed, 0, 0);
  if (fixedProgram)
  {
    sf2::Voice low;
    sf2::Voice high;
    low.Start(fixedProgram->RegionAt(0), fixedProgram->SlotData(0), 20, 100, {}, 1);
    high.Start(fixedProgram->RegionAt(0), fixedProgram->SlotData(0), 100, 100, {}, 1);
    CHECK(low.PositionStepQ16() == high.PositionStepQ16(), "a fixed key plays one pitch");
  }
}

// The integer voice against the same algorithm in double precision: the loop's seam, interpolation
// and the gain's glide.
void TestLoopAgainstModel()
{
  SampleSpec sine;
  sine.name = "sine";
  sine.frames = Sine(100.0, 1000, 16000.0);
  sine.loopStart = 200;
  sine.loopEnd = 1000;
  sine.pitch = 60;
  auto font = Open(SingleFont(sine, {G(sf2::kSampleModes, 1)}));
  auto program = BuildPreset(*font, 0, 0);
  CHECK(program != nullptr, "loop font");
  if (!program) return;
  const sf2::Region& region = program->RegionAt(0);
  const std::int16_t* data = program->SlotData(0);
  for (unsigned key : {60u, 67u, 47u, 84u})
  {
    sf2::Voice voice;
    voice.Start(region, data, key, 127, {}, 1);
    const std::uint32_t step = voice.PositionStepQ16();
    const std::size_t frames = 20000;
    const std::vector<double> got = RenderVoice(voice, frames);
    std::vector<double> model(frames);
    double position = region.start;
    const double length = region.loopEnd - region.loopStart;
    for (std::size_t frame = 0; frame < frames; ++frame)
    {
      const std::size_t index = static_cast<std::size_t>(position);
      const double fraction = position - index;
      const double a = data[index];
      const double b = index + 1 == region.loopEnd ? data[region.loopStart] : data[index + 1];
      const double gain = frame < 64 ? frame / 64.0 : 1.0;
      model[frame] = (a + (b - a) * fraction) * gain;
      position += step / 65536.0;
      if (position >= region.loopEnd) position -= length;
    }
    const double snr = Snr(model, got, 0, frames);
    CHECK(snr > 60.0, "key %u: %g dB against the model", key, snr);
  }
}

void TestEnvelope()
{
  SampleSpec dc;
  dc.name = "dc";
  dc.frames = Constant(16384, 4000);
  dc.loopStart = 1000;
  dc.loopEnd = 3000;
  // Attack 0.1 s, decay 1 s for 100 dB, sustain 20 dB down, release 1 s for 100 dB.
  auto font = Open(SingleFont(dc, {G(sf2::kSampleModes, 1), G(sf2::kAttackVolEnv, -3986), G(sf2::kDecayVolEnv, 0), G(sf2::kSustainVolEnv, 200),
                                   G(sf2::kReleaseVolEnv, 0)}));
  auto program = BuildPreset(*font, 0, 0);
  CHECK(program != nullptr, "envelope font");
  if (!program) return;
  sf2::Voice voice;
  voice.Start(program->RegionAt(0), program->SlotData(0), 60, 127, {}, 1);
  const std::size_t releaseAt = 66112;  // 1.5 s, on a control step
  std::size_t ended = 0;
  const std::vector<double> out = RenderVoice(voice, releaseAt + 44100, releaseAt, nullptr, &ended);

  std::size_t peak = 0;
  while (peak < out.size() && out[peak] < 0.99 * 16384) ++peak;
  CHECK(peak > 4300 && peak < 4600, "the attack reaches 99 %% at frame %zu (0.1 s is 4410)", peak);
  const double decayed = out[peak + 4410];
  CHECK(std::fabs(decayed / (16384 * std::pow(10.0, -0.5)) - 1.0) < 0.05, "0.1 s into the decay: %g", decayed);
  const double sustain = out[61740];
  CHECK(std::fabs(sustain / 1638.4 - 1.0) < 0.01, "sustain %g (1638.4)", sustain);
  const double released = out[releaseAt + 17640];
  CHECK(std::fabs(released / 16.384 - 1.0) < 0.08, "0.4 s into the release %g (16.4)", released);
  // From 20 dB down to 100 dB at 100 dB a second.
  CHECK(!voice.Active() && ended > releaseAt + 34600 && ended < releaseAt + 35900, "off %zu frames after the release (0.8 s is 35280)", ended - releaseAt);

  // Velocity: the gain goes with its square.
  sf2::Voice soft;
  soft.Start(program->RegionAt(0), program->SlotData(0), 60, 64, {}, 1);
  const std::vector<double> softOut = RenderVoice(soft, 61760);
  const double difference = 20.0 * std::log10(softOut[61740] / sustain);
  CHECK(std::fabs(difference - 40.0 * std::log10(64.0 / 127.0)) < 0.1, "velocity 64 is %g dB", difference);

  // Let go during the attack: down from where it was, without a jump.
  sf2::Voice early;
  early.Start(program->RegionAt(0), program->SlotData(0), 60, 127, {}, 1);
  const std::vector<double> earlyOut = RenderVoice(early, 8000, 2048);
  double jump = 0.0;
  for (std::size_t frame = 1; frame < earlyOut.size(); ++frame) jump = (std::max)(jump, std::fabs(earlyOut[frame] - earlyOut[frame - 1]));
  CHECK(jump < 300.0, "released in the attack: the largest step is %g", jump);
  CHECK(earlyOut[2100] < earlyOut[2047] && earlyOut[2100] > 0.5 * earlyOut[2047], "it falls from where it was: %g after %g", earlyOut[2100], earlyOut[2047]);
}

void TestModulation()
{
  SampleSpec sine;
  sine.name = "sine";
  sine.frames = Sine(100.0, 1000, 16000.0);
  sine.loopStart = 200;
  sine.loopEnd = 1000;
  sine.pitch = 60;
  {
    // Vibrato of 100 cents at 0 cents (8.18 Hz).
    auto font = Open(SingleFont(sine, {G(sf2::kSampleModes, 1), G(sf2::kVibLfoToPitch, 100), G(sf2::kFreqVibLfo, 0)}));
    auto program = BuildPreset(*font, 0, 0);
    CHECK(program != nullptr, "vibrato font");
    if (!program) return;
    sf2::Voice voice;
    voice.Start(program->RegionAt(0), program->SlotData(0), 60, 100, {}, 1);
    std::vector<std::uint32_t> steps;
    RenderVoice(voice, 44100, SIZE_MAX, &steps);
    double high = -1e9;
    double low = 1e9;
    for (std::uint32_t step : steps)
    {
      const double cents = 1200.0 * std::log2(step / 65536.0);
      high = (std::max)(high, cents);
      low = (std::min)(low, cents);
    }
    // The triangle is read 84 times a cycle: its tip may fall between two readings.
    CHECK(high > 96.0 && high < 101.0 && low < -96.0 && low > -101.0, "vibrato between %g and %g cents", low, high);
  }
  {
    // The modulation envelope an octave up at once, then down to nothing in 1 s.
    auto font = Open(SingleFont(sine, {G(sf2::kSampleModes, 1), G(sf2::kModEnvToPitch, 1200), G(sf2::kDecayModEnv, 0), G(sf2::kSustainModEnv, 1000)}));
    auto program = BuildPreset(*font, 0, 0);
    CHECK(program != nullptr, "pitch envelope font");
    if (!program) return;
    sf2::Voice voice;
    voice.Start(program->RegionAt(0), program->SlotData(0), 60, 100, {}, 1);
    std::vector<std::uint32_t> steps;
    RenderVoice(voice, 66150, SIZE_MAX, &steps);
    const double start = 1200.0 * std::log2(steps[1] / 65536.0);
    const double half = 1200.0 * std::log2(steps[345] / 65536.0);  // 0.5 s
    const double end = 1200.0 * std::log2(steps.back() / 65536.0);
    CHECK(start > 1180.0 && std::fabs(half - 600.0) < 15.0 && std::fabs(end) < 0.05, "pitch envelope %g, %g, %g cents", start, half, end);
  }
}

// The filter against the textbook state-variable lowpass in double precision, and its response.
void TestFilter()
{
  SampleSpec noise;
  noise.name = "noise";
  noise.frames = Noise(8000, 10000);
  for (int index : {240, 0, 30, 400})
  {
    for (int resonance : {0, 120})
    {
      const int cents = 1500 + 25 * index;
      auto font = Open(SingleFont(noise, {G(sf2::kInitialFilterFc, cents), G(sf2::kInitialFilterQ, resonance)}));
      auto program = BuildPreset(*font, 0, 0);
      if (!program) continue;
      sf2::Voice voice;
      voice.Start(program->RegionAt(0), program->SlotData(0), 60, 127, {}, 1);
      const std::vector<double> got = RenderVoice(voice, 7900);
      const double hz = (std::min)(kKeyZeroHz * std::pow(2.0, cents / 1200.0), 0.45 * kRate);
      const double g = std::tan(kPi * hz / kRate);
      const double k = 1.0 / std::pow(10.0, (resonance / 10.0 - 3.01) / 20.0);
      const double a1 = 1.0 / (1.0 + g * (g + k));
      const double a2 = g * a1;
      const double a3 = g * a2;
      double ic1 = 0.0;
      double ic2 = 0.0;
      std::vector<double> model(got.size());
      for (std::size_t frame = 0; frame < model.size(); ++frame)
      {
        const double v3 = noise.frames[frame] - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        model[frame] = v2;
      }
      // The first control step glides the gain up; from then on the gain is unity. Within a unit of
      // the output of the exact filter, at a low cutoff too.
      const double error = RmsError(model, got, 64, got.size());
      CHECK(error < 1.0, "filter at %d cents, %d cB: %g units from the model (signal %g)", cents, resonance, error, Rms(model, 64, model.size()));
    }
  }

  SampleSpec tone;
  tone.name = "sine440";
  tone.frames = Sine(kRate / 440.0, 44100, 16000.0);
  tone.pitch = 60;
  const auto response = [&](int cents, int resonance) {
    auto font = Open(SingleFont(tone, {G(sf2::kInitialFilterFc, cents), G(sf2::kInitialFilterQ, resonance)}));
    auto program = BuildPreset(*font, 0, 0);
    if (!program) return -999.0;
    sf2::Voice voice;
    voice.Start(program->RegionAt(0), program->SlotData(0), 60, 127, {}, 1);
    const std::vector<double> out = RenderVoice(voice, 40000);
    return 20.0 * std::log10(Rms(out, 11025, 33075) / (16000.0 / std::sqrt(2.0)));
  };
  const auto expected = [](int cents, int resonance) {
    const double hz = kKeyZeroHz * std::pow(2.0, cents / 1200.0);
    const double omega = std::tan(kPi * 440.0 / kRate) / std::tan(kPi * hz / kRate);
    const double k = 1.0 / std::pow(10.0, (resonance / 10.0 - 3.01) / 20.0);
    return -10.0 * std::log10((1.0 - omega * omega) * (1.0 - omega * omega) + k * k * omega * omega);
  };
  for (const auto& setting : {std::pair<int, int>{4500, 0}, std::pair<int, int>{6900, 120}, std::pair<int, int>{9300, 0}, std::pair<int, int>{6900, 0}})
  {
    const double got = response(setting.first, setting.second);
    const double want = expected(setting.first, setting.second);
    CHECK(std::fabs(got - want) < 0.3, "440 Hz through %d cents, %d cB: %g dB (%g)", setting.first, setting.second, got, want);
  }
}

// Mode 3 plays on past the loop once the key is up; mode 1 keeps looping.
void TestLoopModes()
{
  SampleSpec tail;
  tail.name = "tail";
  tail.frames = Constant(16384, 3000);
  std::fill(tail.frames.begin() + 1500, tail.frames.end(), static_cast<std::int16_t>(-16384));
  tail.loopStart = 500;
  tail.loopEnd = 1500;
  for (int mode : {3, 1})
  {
    auto font = Open(SingleFont(tail, {G(sf2::kSampleModes, mode), G(sf2::kReleaseVolEnv, 0)}));
    auto program = BuildPreset(*font, 0, 0);
    if (!program) continue;
    CHECK(program->SlotFrames(0) == (mode == 3 ? 3000u : 1500u), "mode %d keeps %u frames", mode, program->SlotFrames(0));
    sf2::Voice voice;
    voice.Start(program->RegionAt(0), program->SlotData(0), 60, 127, {}, 1);
    const std::vector<double> out = RenderVoice(voice, 12000, 4096);
    const double before = *std::min_element(out.begin() + 64, out.begin() + 4096);
    const double after = *std::min_element(out.begin() + 4096, out.end());
    CHECK(before > 0.0, "mode %d loops while held", mode);
    if (mode == 3) CHECK(after < -10000.0 && !voice.Active(), "mode 3 plays the tail and ends with the sample: %g", after);
    else CHECK(after > 0.0 && voice.Active(), "mode 1 loops through the release: %g", after);
  }
}

std::vector<double> RenderTrack(sf2::Track& track, std::size_t frames)
{
  std::vector<std::int32_t> out(frames, 0);
  for (std::size_t frame = 0; frame < frames; frame += 256) track.Render(out.data() + frame, (std::min)(std::size_t{256}, frames - frame));
  return std::vector<double>(out.begin(), out.end());
}

void TestTrack()
{
  auto font = Open(MergeFont().Build());
  auto sine = BuildPreset(*font, 0, 0);
  auto kit = BuildPreset(*font, 128, 0);
  CHECK(sine && kit, "track presets");
  if (!sine || !kit) return;

  sf2::Track track;
  track.SetProgram(kit.get());
  // Exclusive class: the closed hi-hat ends the open one; another drum is untouched.
  track.NoteOn(36, 100);
  track.NoteOn(46, 100);
  RenderTrack(track, 1024);
  track.NoteOn(42, 100);
  RenderTrack(track, 64 * 24);
  CHECK(track.SoundingVoices() == 2 && !track.Holds(46) && track.Holds(42) && track.Holds(36), "exclusive class: %zu voices", track.SoundingVoices());
  // The same key again lets the earlier note go.
  track.NoteOn(36, 100);
  RenderTrack(track, 64 * 24);
  CHECK(track.SoundingVoices() == 2, "the same key again: %zu voices", track.SoundingVoices());
  track.AllNotesOff();
  RenderTrack(track, 44100);
  CHECK(!track.Busy(), "all notes off ends them");

  // Nine held notes on eight voices: the oldest held one gives way, without a click.
  SampleSpec dc;
  dc.name = "dc";
  dc.frames = Constant(16384, 4000);
  dc.loopStart = 1000;
  dc.loopEnd = 3000;
  auto pad = Open(SingleFont(dc, {G(sf2::kSampleModes, 1), G(sf2::kReleaseVolEnv, 2400)}));
  auto padProgram = BuildPreset(*pad, 0, 0);
  sf2::Track pads;
  pads.SetProgram(padProgram.get());
  for (unsigned key = 60; key < 68; ++key) pads.NoteOn(key, 127);
  RenderTrack(pads, 1024);
  pads.NoteOn(80, 127);
  const std::vector<double> out = RenderTrack(pads, 1024);
  double jump = 0.0;
  for (std::size_t frame = 1; frame < out.size(); ++frame) jump = (std::max)(jump, std::fabs(out[frame] - out[frame - 1]));
  CHECK(pads.StolenHeld() == 1 && !pads.Holds(60) && pads.Holds(80) && pads.SoundingVoices() == 8, "a voice taken: %u stolen", pads.StolenHeld());
  CHECK(jump <= 600.0, "taking a voice moves the output by %g at most", jump);
  // A key let go before its note started (it waited for a voice) still ends.
  pads.NoteOn(81, 127);
  pads.NoteOff(81);
  RenderTrack(pads, 256);
  CHECK(!pads.Holds(81), "released while waiting");
  pads.SetProgram(sine.get());
  CHECK(!pads.Busy(), "a new program stops the voices");
}

// Damaged files: every truncation and thousands of changed bytes. Nothing may crash or read out of
// bounds (run this test with the address sanitizer to see the latter).
void TestDamagedFiles()
{
  const std::vector<std::uint8_t> good = MergeFont().Build();
  std::size_t presetData = 0;
  for (std::size_t index = 0; index + 4 <= good.size(); ++index)
    if (std::string(reinterpret_cast<const char*>(&good[index]), 4) == "pdta") presetData = index;
  CHECK(presetData > 0, "found the preset data");

  std::size_t opened = 0;
  std::size_t built = 0;
  const auto exercise = [&](std::vector<std::uint8_t> bytes) {
    auto font = Open(std::move(bytes));
    if (font->status != sf2::Status::Ok) return;
    ++opened;
    for (std::size_t position = 0; position < font->file.PresetCount(); ++position)
    {
      char name[21];
      (void)font->file.PresetName(position, name);
      sf2::Program::Result result = sf2::Program::Build(font->file, position, 1u << 20);
      if (!result.program) continue;
      ++built;
      sf2::Track track;
      track.SetProgram(result.program.get());
      for (unsigned key : {0u, 36u, 42u, 60u, 127u}) track.NoteOn(key, 1 + key % 127);
      RenderTrack(track, 1024);
      track.AllNotesOff();
      RenderTrack(track, 512);
    }
  };
  for (std::size_t length = 0; length < good.size(); length += length < presetData ? 97 : 1)
    exercise(std::vector<std::uint8_t>(good.begin(), good.begin() + static_cast<std::ptrdiff_t>(length)));
  for (int round = 0; round < 4000; ++round)
  {
    std::vector<std::uint8_t> bytes = good;
    const int changes = 1 + static_cast<int>(Random() % 4);
    for (int change = 0; change < changes; ++change)
    {
      const std::size_t at = round % 8 == 0 ? Random() % bytes.size() : presetData + Random() % (bytes.size() - presetData);
      bytes[at] = static_cast<std::uint8_t>(Random() >> 24);
    }
    exercise(std::move(bytes));
  }
  CHECK(opened > 1000 && built > 1000, "damaged files exercised: %zu opened, %zu presets built", opened, built);
  std::printf("sf2: damaged files: %zu opened, %zu presets built and played\n", opened, built);
}

}  // namespace

int main()
{
  TestTables();
  TestFile();
  TestFlatten();
  TestPitch();
  TestLoopAgainstModel();
  TestEnvelope();
  TestModulation();
  TestFilter();
  TestLoopModes();
  TestTrack();
  TestDamagedFiles();
  std::printf("sf2: %d checks, %d failed\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
