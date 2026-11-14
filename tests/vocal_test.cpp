// The Vocal engine below the audio runtime: voice packs (the built-in one, packs built here, damaged
// ones), a voice's sound, and which word a track's notes sing.
#undef NDEBUG
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "Library/Synth/Core/Engines/Vocal/VocalBuiltinPack.h"
#include "Library/Synth/Core/Engines/Vocal/VocalPack.h"
#include "Library/Synth/Core/Engines/Vocal/VocalTrack.h"
#include "Library/Synth/Core/Engines/Vocal/VocalVoice.h"

namespace {

using namespace synth::vocal;

// The built-in pack's words, in its order: A I U E O M K S, then the syllables.
constexpr unsigned kA = 0, kI = 1, kO = 4, kMi = 8, kKu = 9, kMa = 10, kSa = 12;

void Put16(std::vector<std::uint8_t>& bytes, std::size_t at, unsigned value)
{
  bytes[at] = static_cast<std::uint8_t>(value);
  bytes[at + 1] = static_cast<std::uint8_t>(value >> 8);
}

void Put32(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value)
{
  Put16(bytes, at, value & 0xFFFFu);
  Put16(bytes, at + 2, value >> 16);
}

void PutText(std::vector<std::uint8_t>& bytes, std::size_t at, const char* text)
{
  std::memcpy(bytes.data() + at, text, std::strlen(text));
}

void Sign(std::vector<std::uint8_t>& pack)
{
  Put32(pack, 16, Pack::Checksum(pack.data() + 20, pack.size() - 20));
}

struct TestSegment {
  unsigned phoneme;
  unsigned durationMs;
};
struct TestEntry {
  unsigned id;
  const char* name;
  std::vector<TestSegment> segments;
};

// A pack with one wave per phoneme; every wave has `volume` and the modulation `depth` / `rate` (its
// table climbs for half its length, then falls back).
std::vector<std::uint8_t> BuildPack(const std::vector<std::vector<int>>& waves, const std::vector<TestEntry>& entries, unsigned volume = 32,
                                    unsigned depth = 0, unsigned rate = 0)
{
  std::size_t segmentCount = 0;
  for (const auto& entry : entries) segmentCount += entry.segments.size();
  const std::size_t po = kHeaderBytes + waves.size() * kWaveBytes;
  const std::size_t zo = po + waves.size() * kPhonemeBytes;
  const std::size_t eo = zo + waves.size() * kZoneBytes;
  const std::size_t so = eo + entries.size() * kEntryBytes;
  std::vector<std::uint8_t> bytes(so + segmentCount * kSegmentBytes, 0);
  std::memcpy(bytes.data(), "NPVOCAL\0", 8);
  Put16(bytes, 8, kFormat);
  Put16(bytes, 10, kHeaderBytes);
  Put32(bytes, 12, static_cast<std::uint32_t>(bytes.size()));
  Put16(bytes, 20, static_cast<unsigned>(waves.size()));
  Put16(bytes, 22, static_cast<unsigned>(waves.size()));
  Put16(bytes, 24, static_cast<unsigned>(waves.size()));
  Put16(bytes, 26, static_cast<unsigned>(entries.size()));
  Put16(bytes, 28, static_cast<unsigned>(segmentCount));
  Put16(bytes, 30, entries.front().id);
  PutText(bytes, 32, "Test Voice");
  PutText(bytes, 64, "Tests");
  PutText(bytes, 96, "test.voice");
  for (std::size_t wave = 0; wave < waves.size(); ++wave)
  {
    std::uint8_t* w = bytes.data() + kHeaderBytes + wave * kWaveBytes;
    for (unsigned sample = 0; sample < kWaveSamples; ++sample) w[sample] = static_cast<std::uint8_t>(waves[wave][sample]);
    w[field::kWaveVolume] = static_cast<std::uint8_t>(volume);
    w[field::kWaveModDepth] = static_cast<std::uint8_t>(depth);
    Put16(bytes, kHeaderBytes + wave * kWaveBytes + field::kWaveModRate, rate);
    for (unsigned step = 0; step < kModulationSteps; ++step) w[field::kWaveModTable + step] = step < 16 ? 1 : 7;
    const std::size_t p = po + wave * kPhonemeBytes;
    Put16(bytes, p, static_cast<unsigned>(wave));  // first zone
    Put16(bytes, p + 2, 1);
    bytes[p + field::kPhonemeGain] = 255;
    bytes[p + field::kPhonemeKind] = 1;
    const std::size_t z = zo + wave * kZoneBytes;
    Put16(bytes, z, static_cast<unsigned>(wave));
    bytes[z + 2] = 60;
    bytes[z + 3] = 0;
    bytes[z + 4] = 127;
  }
  std::size_t segment = 0;
  for (std::size_t index = 0; index < entries.size(); ++index)
  {
    const std::size_t e = eo + index * kEntryBytes;
    Put16(bytes, e, entries[index].id);
    Put16(bytes, e + 2, static_cast<unsigned>(segment));
    Put16(bytes, e + 4, static_cast<unsigned>(entries[index].segments.size()));
    PutText(bytes, e + 8, entries[index].name);
    for (const TestSegment& part : entries[index].segments)
    {
      const std::size_t s = so + segment * kSegmentBytes;
      Put16(bytes, s, part.phoneme);
      Put16(bytes, s + field::kSegmentDurationMs, part.durationMs);
      bytes[s + field::kSegmentGain] = 255;
      ++segment;
    }
  }
  Sign(bytes);
  return bytes;
}

// Waves in the pack's own steps: six bits, stored as multiples of four.
std::vector<int> Square(int low, int high)
{
  std::vector<int> wave(kWaveSamples);
  for (unsigned index = 0; index < kWaveSamples; ++index) wave[index] = index < 32 ? high : low;
  return wave;
}

std::vector<int> Sine(int amplitude)
{
  std::vector<int> wave(kWaveSamples);
  for (unsigned index = 0; index < kWaveSamples; ++index)
    wave[index] = static_cast<int>(std::lround(amplitude * std::sin(2.0 * 3.14159265358979 * index / kWaveSamples) / 4.0)) * 4;
  return wave;
}

float Peak(const std::vector<float>& audio, std::size_t from, std::size_t to)
{
  float peak = 0.0f;
  for (std::size_t index = from; index < to && index < audio.size(); ++index) peak = (std::max)(peak, std::fabs(audio[index]));
  return peak;
}

// The period in frames, from the autocorrelation of [from, from + 4096).
int Period(const std::vector<float>& audio, std::size_t from, int low, int high)
{
  int best = low;
  double bestSum = -1e30;
  for (int lag = low; lag <= high; ++lag)
  {
    double sum = 0.0;
    for (std::size_t index = from; index < from + 4096; ++index) sum += static_cast<double>(audio[index]) * audio[index + static_cast<std::size_t>(lag)];
    if (sum > bestSum)
    {
      bestSum = sum;
      best = lag;
    }
  }
  return best;
}

std::vector<float> RenderVoice(Voice& voice, const VoiceSettings& settings, std::size_t frames)
{
  std::vector<float> audio(frames, 0.0f);
  for (std::size_t done = 0; done < frames; done += 256) voice.Render(audio.data() + done, (std::min<std::size_t>)(256, frames - done), settings, 1.0f);
  return audio;
}

void TestBuiltinPack()
{
  assert(Pack::Validate(kBuiltinPack.data(), kBuiltinPack.size()));
  const PackRef pack = Pack::Builtin();
  assert(pack && pack == Pack::Builtin());
  assert(std::string(pack->Name()) == "Synthetic Vowels" && pack->EntryCount() == 13 && pack->DefaultEntry() == kA);
  assert(std::string(pack->EntryNameAt(kA)) == "A" && std::string(pack->EntryNameAt(kMi)) == "MI" && std::string(pack->EntryNameAt(kKu)) == "KU" &&
         std::string(pack->EntryNameAt(kSa)) == "SA");
  // Ids are the pack's own (MI is 16); the firmware names a pronunciation by its place in the list.
  assert(pack->Find(16) == static_cast<int>(kMi) && pack->Find(8) < 0 && pack->EntryId(kMi) == 16);
  // A syllable is a consonant into the vowel it holds; a vowel is one part.
  assert(pack->EntryHasOnset(kMi) && !pack->EntryHasOnset(kA));
  // A loaded copy is its own pack with the same contents.
  const PackRef copy = Pack::Load(kBuiltinPack.data(), kBuiltinPack.size());
  assert(copy && copy != pack && copy->Size() == pack->Size() && copy->Bytes() != pack->Bytes());
  assert(std::memcmp(copy->Bytes(), pack->Bytes(), pack->Size()) == 0);
}

void TestDamagedPacks()
{
  std::vector<std::uint8_t> bytes(kBuiltinPack.begin(), kBuiltinPack.end());
  assert(Pack::Load(bytes.data(), bytes.size()));
  assert(!Pack::Load(nullptr, 0) && !Pack::Load(bytes.data(), 64) && !Pack::Load(bytes.data(), bytes.size() - 1));
  auto damaged = bytes;
  damaged[kHeaderBytes + 5] ^= 4;  // a sample: the checksum no longer matches
  assert(!Pack::Load(damaged.data(), damaged.size()));
  damaged = bytes;
  damaged[0] = 'X';
  assert(!Pack::Load(damaged.data(), damaged.size()));
  // Right checksum, wrong contents: another format number, a segment naming a phoneme the pack does
  // not have, a default pronunciation that is not there.
  for (const unsigned format : {1u, 3u})
  {
    damaged = bytes;
    Put16(damaged, 8, format);
    Sign(damaged);
    assert(!Pack::Load(damaged.data(), damaged.size()));
  }
  damaged = bytes;
  Put16(damaged, damaged.size() - kSegmentBytes, 200);
  Sign(damaged);
  assert(!Pack::Load(damaged.data(), damaged.size()));
  damaged = bytes;
  Put16(damaged, 30, 9);
  Sign(damaged);
  assert(!Pack::Load(damaged.data(), damaged.size()));
  // A pack built here loads; a phoneme of another kind, or a sample that is no six-bit step, does not.
  const auto built = BuildPack({Square(-128, 124)}, {{0, "A", {{0, 0}}}});
  assert(Pack::Load(built.data(), built.size()));
  damaged = built;
  damaged[kHeaderBytes + kWaveBytes + field::kPhonemeKind] = 0;
  Sign(damaged);
  assert(!Pack::Load(damaged.data(), damaged.size()));
  damaged = built;
  damaged[kHeaderBytes + 3] = 126;
  Sign(damaged);
  assert(!Pack::Load(damaged.data(), damaged.size()));
  damaged = built;
  damaged[kHeaderBytes + field::kWaveVolume] = 33;
  Sign(damaged);
  assert(!Pack::Load(damaged.data(), damaged.size()));
}

void TestVoiceSound()
{
  const PackRef pack = Pack::Builtin();
  VoiceSettings settings;
  Voice voice;
  assert(!voice.Active());
  // A at A4: 440 Hz is a period of 100.2 frames.
  voice.Start(*pack, kA, 69, 1.0f, settings);
  assert(voice.Held());
  std::vector<float> audio = RenderVoice(voice, settings, 22050);
  assert(voice.Held());
  assert(Peak(audio, 0, 8) < 0.2f);            // the attack starts from silence
  assert(Peak(audio, 4000, 22050) > 0.3f && Peak(audio, 0, 22050) <= 1.01f);
  assert(Period(audio, 8000, 80, 120) == 100);
  // An octave up halves the period; the tuning moves it.
  voice.Start(*pack, kA, 81, 1.0f, settings);
  audio = RenderVoice(voice, settings, 22050);
  assert(Period(audio, 8000, 40, 60) == 50);
  settings.transpose = -12;
  voice.Start(*pack, kA, 81, 1.0f, settings);
  audio = RenderVoice(voice, settings, 22050);
  assert(Period(audio, 8000, 80, 120) == 100);
  settings.transpose = 0;

  // Let go, it fades over the release time and ends.
  settings.releaseMs = 30;
  voice.Release(settings);
  assert(voice.Active() && voice.Releasing() && !voice.Held());
  audio = RenderVoice(voice, settings, 4410);
  assert(!voice.Active());
  assert(Peak(audio, 0, 200) > 0.1f && Peak(audio, Frames(30) + 1, 4410) == 0.0f);

  // MI: 40 ms of M, then I held. The vowel that follows differs from a plain I only in its start.
  Voice syllable, vowel;
  syllable.Start(*pack, kMi, 60, 1.0f, settings);
  vowel.Start(*pack, kI, 60, 1.0f, settings);
  const std::vector<float> mi = RenderVoice(syllable, settings, 22050);
  const std::vector<float> i = RenderVoice(vowel, settings, 22050);
  double early = 0.0;
  for (std::size_t frame = 200; frame < 1600; ++frame) early += std::fabs(mi[frame] - i[frame]);
  assert(early > 10.0);  // the M sounds first
  // Later both hold the same wave at the same pitch (not the same phase: compare their levels).
  assert(std::fabs(Peak(mi, 12000, 22050) - Peak(i, 12000, 22050)) < 0.05f);
  assert(syllable.Held() && vowel.Held());

  // A slur glides from the pitch that sounds to the new note; without a glide time it is there at once.
  settings.glideFrames = Frames(100);
  voice.Start(*pack, kA, 69, 1.0f, settings);
  (void)RenderVoice(voice, settings, 8820);
  voice.Tie(81, settings);
  assert(voice.Held() && voice.Note() == 81);
  audio = RenderVoice(voice, settings, 22050);
  const int during = Period(audio, 0, 40, 120);
  assert(during > 55 && during < 100);                 // on its way up
  assert(Period(audio, 12000, 40, 120) == 50);         // arrived
  settings.glideFrames = 0;
  voice.Tie(69, settings);
  audio = RenderVoice(voice, settings, 8820);
  assert(Period(audio, 0, 40, 120) == 100);

  // Vibrato moves the pitch around the note, after its delay.
  settings.vibratoDepth = 1.0f;
  settings.vibratoStep = 6.0f * kControlFrames / kVoiceSampleRate;
  settings.vibratoDelayFrames = 22050;
  voice.Start(*pack, kA, 69, 1.0f, settings);
  audio = RenderVoice(voice, settings, 88200);
  assert(Period(audio, 8000, 80, 120) == 100);
  int shortest = 1000, longest = 0;
  for (std::size_t from = 40000; from < 80000; from += 1470)
  {
    const int period = Period(audio, from, 80, 120);
    shortest = (std::min)(shortest, period);
    longest = (std::max)(longest, period);
  }
  assert(shortest <= 98 && longest >= 102 && shortest >= 92 && longest <= 108);

  // Another pronunciation on the sounding note: no new attack, the voice goes on.
  settings = VoiceSettings{};
  voice.Start(*pack, kA, 69, 1.0f, settings);
  (void)RenderVoice(voice, settings, 8820);
  assert(voice.Envelope() == 1.0f);
  assert(voice.Pronounce(kO, settings));
  audio = RenderVoice(voice, settings, 512);
  assert(voice.Envelope() == 1.0f && Peak(audio, 0, 512) > 0.2f && voice.Entry() == kO);
  voice.Release(settings);
  assert(!voice.Pronounce(kA, settings));

  // K is no tone: its pitch is thrown about, so no period holds from one stretch to the next.
  voice.Start(*pack, 6, 60, 1.0f, settings);
  audio = RenderVoice(voice, settings, 44100);
  assert(Peak(audio, 4000, 44100) > 0.05f);
  int briefest = 1000, lengthiest = 0;
  for (std::size_t from = 4000; from < 36000; from += 2000)
  {
    const int period = Period(audio, from, 20, 400);
    briefest = (std::min)(briefest, period);
    lengthiest = (std::max)(lengthiest, period);
  }
  assert(lengthiest - briefest >= 8);
}

void TestTimedPronunciationEnds()
{
  // A pronunciation whose parts all have a length ends by itself.
  const auto bytes = BuildPack({Sine(100), Sine(60)}, {{0, "TA", {{1, 20}, {0, 50}}}, {1, "A", {{0, 0}}}});
  const PackRef pack = Pack::Load(bytes.data(), bytes.size());
  assert(pack);
  VoiceSettings settings;
  settings.releaseMs = 10;
  Voice voice;
  voice.Start(*pack, 0, 60, 1.0f, settings);
  const std::vector<float> audio = RenderVoice(voice, settings, 8820);
  assert(!voice.Active());
  assert(Peak(audio, 0, Frames(70)) > 0.3f && Peak(audio, Frames(81), 8820) == 0.0f);
}

void TestOscillator()
{
  // The wave is stepped: read whole steps and it is the wave itself, across a step's edge the mean of
  // what the frame covers.
  std::int8_t ramp[kWaveSamples];
  for (unsigned index = 0; index < kWaveSamples; ++index) ramp[index] = static_cast<std::int8_t>(index * 2);
  constexpr std::uint32_t kStep = 1u << 26;
  assert(Voice::ChipSample(ramp, 5 * kStep, kStep, 1.0f / kStep) == 10.0f);
  assert(Voice::ChipSample(ramp, 5 * kStep + kStep / 2, kStep / 4, 4.0f / kStep) == 10.0f);
  assert(std::fabs(Voice::ChipSample(ramp, 5 * kStep + kStep / 2, kStep, 1.0f / kStep) - 11.0f) < 1e-4f);
  assert(std::fabs(Voice::ChipSample(ramp, 5 * kStep, 3 * kStep, 1.0f / (3.0f * kStep)) - 12.0f) < 1e-4f);
  assert(std::fabs(Voice::ChipSample(ramp, 63 * kStep + kStep / 2, kStep, 1.0f / kStep) - 63.0f) < 1e-4f);  // wraps to step 0

  // The modulation rule: nothing for a resting counter or no depth; the hardware's rounding otherwise.
  assert(Voice::ChipPitchRatio(0, 63) == 1.0f && Voice::ChipPitchRatio(17, 0) == 1.0f);
  assert(Voice::ChipPitchRatio(1, 32) == 1.0f + 2.0f / 64.0f);
  assert(Voice::ChipPitchRatio(1, 33) == 1.0f + 4.0f / 64.0f);
  assert(Voice::ChipPitchRatio(-1, 33) == 1.0f - 3.0f / 64.0f);
  assert(Voice::ChipPitchRatio(-64, 63) >= 0.0f);
  assert(Voice::StepModulation(63, 1) == -64 && Voice::StepModulation(-64, 7) == 63 && Voice::StepModulation(20, 4) == 0);
  assert(Voice::StepModulation(0, 3) == 4 && Voice::StepModulation(0, 5) == -4);

  // A wave's volume: a square wave at half the loudest is half as loud.
  auto bytes = BuildPack({Square(-128, 124)}, {{0, "A", {{0, 0}}}}, 16);
  PackRef pack = Pack::Load(bytes.data(), bytes.size());
  assert(pack);
  VoiceSettings settings;
  Voice voice;
  voice.Start(*pack, 0, 69, 1.0f, settings);
  std::vector<float> audio = RenderVoice(voice, settings, 22050);
  assert(Period(audio, 8000, 80, 120) == 100);
  const float half = Peak(audio, 8000, 22050);
  assert(half > 0.4f && half < 0.5f);
  bytes = BuildPack({Square(-128, 124)}, {{0, "A", {{0, 0}}}}, 32);
  pack = Pack::Load(bytes.data(), bytes.size());
  voice.Start(*pack, 0, 69, 1.0f, settings);
  audio = RenderVoice(voice, settings, 22050);
  assert(std::fabs(Peak(audio, 8000, 22050) - 2.0f * half) < 0.05f);

  // With its modulation on, the pitch moves (the table climbs, then falls back).
  bytes = BuildPack({Square(-128, 124)}, {{0, "A", {{0, 0}}}}, 32, 20, 4);
  pack = Pack::Load(bytes.data(), bytes.size());
  voice.Start(*pack, 0, 69, 1.0f, settings);
  audio = RenderVoice(voice, settings, 44100);
  int shortest = 1000, longest = 0;
  for (std::size_t from = 2000; from < 36000; from += 1000)
  {
    const int period = Period(audio, from, 60, 140);
    shortest = (std::min)(shortest, period);
    longest = (std::max)(longest, period);
  }
  assert(longest - shortest >= 12);
}

void TestWordKeys()
{
  // Every word a pack may have has a key, and the key names it again.
  for (int word = 0; word < static_cast<int>(kMaxEntries); ++word)
  {
    const std::uint8_t note = KeyNoteOfWord(word), velocity = KeyVelocityOfWord(word);
    assert(IsWordKey(note) && velocity >= 1 && velocity <= 127 && WordOfKey(note, velocity) == word);
  }
  assert(KeyNoteOfWord(0) == 0 && KeyVelocityOfWord(0) == 1 && KeyNoteOfWord(127) == 1 && KeyVelocityOfWord(127) == 1);
  assert(!IsWordKey(kWordKeyCount) && WordOfKey(0, 0) == -1 && WordOfKey(kWordKeyCount, 1) == -1 && WordOfKey(0, 128) == -1);
}

unsigned SungEntry(const Track& track)
{
  for (std::size_t index = 0; index < kVoiceCount; ++index)
    if (track.VoiceAt(index).Held()) return track.VoiceAt(index).Entry();
  return 9999;
}

std::size_t HeldVoices(const Track& track)
{
  std::size_t count = 0;
  for (std::size_t index = 0; index < kVoiceCount; ++index) count += track.VoiceAt(index).Held();
  return count;
}

void TestTrack()
{
  const PackRef pack = Pack::Builtin();
  Track track;
  std::vector<float> block(256);
  const auto key = [&](unsigned word) { track.WordKey(KeyNoteOfWord(static_cast<int>(word)), KeyVelocityOfWord(static_cast<int>(word))); };
  const auto sing = [&](std::uint8_t note) {
    track.NoteOn(note, 1.0f);
    const unsigned entry = SungEntry(track);
    track.NoteOff(note);
    return entry;
  };
  // Without a pack nothing sounds.
  track.NoteOn(60, 1.0f);
  assert(!track.Active() && !track.Held());
  track.SetPack(pack);
  assert(!track.SetParameter("no.such", 1) && track.SetParameter("amp.level", 127));

  // A note sings the track's own word: the pack's first until another is set.
  track.NoteOn(60, 1.0f);
  assert(track.Held() && SungEntry(track) == kA && track.CurrentStatus().singing == static_cast<int>(kA));
  track.NoteOff(60);
  assert(!track.Held() && track.Active());  // the tail still sounds
  track.Tick(block.size());
  assert(track.CurrentStatus().singing == -1);  // nothing held: nothing is being sung
  assert(track.SetParameter("voice.word", static_cast<int>(kMi)));
  assert(sing(60) == kMi && sing(62) == kMi);

  // A word key says what the note that comes with it sings: that note, and no other after it.
  key(kKu);
  assert(!track.Held() && HeldVoices(track) == 0);  // a key sounds nothing
  std::array<std::uint32_t, 4> held{};
  track.AddHeldNotes(held);
  assert(held[0] == 0 && held[1] == 0);
  assert(sing(60) == kKu);
  assert(sing(60) == kMi);
  // The note first, its key right after (a step holds both, in either order): the same.
  track.NoteOn(60, 1.0f);
  assert(SungEntry(track) == kMi);
  key(kSa);
  assert(SungEntry(track) == kSa && HeldVoices(track) == 1 && track.CurrentStatus().singing == static_cast<int>(kSa));
  for (std::size_t index = 0; index < kVoiceCount; ++index)
    if (track.VoiceAt(index).Held()) assert(track.VoiceAt(index).Age() == 0 && track.VoiceAt(index).Envelope() == 0.0f);
  track.NoteOff(60);
  assert(sing(60) == kMi);
  // A key no note took up runs out: a note much later sings the track's word.
  key(kKu);
  track.Tick(kWordKeyFrames / 2);
  assert(sing(60) == kKu);
  key(kKu);
  track.Tick(kWordKeyFrames / 2);
  track.Tick(kWordKeyFrames / 2);
  assert(sing(60) == kMi);
  // A key for a word the singer does not have is no key.
  track.WordKey(2, 127);
  assert(sing(60) == kMi);
  // The track's word past the singer's last sings the pack's default.
  assert(track.SetParameter("voice.word", 200));
  assert(sing(60) == kA);
  assert(track.SetParameter("voice.word", static_cast<int>(kMi)));

  // A key under a note that is held changes its word at once, without a new attack.
  track.NoteOn(60, 1.0f);
  track.Render(block.data(), block.size());
  for (int blocks = 0; blocks < 8; ++blocks) track.Render(block.data(), block.size());
  std::uint32_t onsets = track.CurrentStatus().onsets;
  key(kMa);
  assert(SungEntry(track) == kMa && HeldVoices(track) == 1 && track.CurrentStatus().onsets == onsets + 1 &&
         track.CurrentStatus().singing == static_cast<int>(kMa));
  for (std::size_t index = 0; index < kVoiceCount; ++index)
    if (track.VoiceAt(index).Held()) assert(track.VoiceAt(index).Age() > 0 && track.VoiceAt(index).Envelope() == 1.0f);
  // The word it already sings, named again, goes on undisturbed (a step names its word with every
  // note it starts), and nothing waits for a later note.
  onsets = track.CurrentStatus().onsets;
  for (int blocks = 0; blocks < 4; ++blocks) track.Render(block.data(), block.size());
  key(kMa);
  assert(SungEntry(track) == kMa && track.CurrentStatus().onsets == onsets);
  for (std::size_t index = 0; index < kVoiceCount; ++index)
    if (track.VoiceAt(index).Held()) assert(track.VoiceAt(index).Age() >= 12 * block.size() && track.VoiceAt(index).Envelope() == 1.0f);
  track.NoteOff(60);
  assert(sing(60) == kMi);  // the keys were for the held note: nothing waits

  // MONO: a note played over a held one is a slur. No new word, one voice, and letting the newer key go
  // returns to the one still down.
  onsets = track.CurrentStatus().onsets;
  track.NoteOn(60, 1.0f);
  assert(SungEntry(track) == kMi && track.CurrentStatus().onsets == onsets + 1);
  track.NoteOn(64, 1.0f);
  assert(HeldVoices(track) == 1 && SungEntry(track) == kMi && track.CurrentStatus().onsets == onsets + 1);
  held = {};
  track.AddHeldNotes(held);
  assert(held[1] == 1u << (60 - 32) && held[2] == 1u << (64 - 64) && held[0] == 0 && held[3] == 0);
  track.NoteOff(64);
  assert(HeldVoices(track) == 1 && track.Held());
  bool back = false;
  for (std::size_t index = 0; index < kVoiceCount; ++index) back = back || (track.VoiceAt(index).Held() && track.VoiceAt(index).Note() == 60);
  assert(back);
  track.NoteOff(60);
  assert(!track.Held() && HeldVoices(track) == 0);

  // POLY: notes played together sing the same word, each in its own voice.
  assert(track.SetParameter("voice.mode", 1));
  onsets = track.CurrentStatus().onsets;
  key(kKu);
  track.NoteOn(60, 1.0f);
  track.NoteOn(64, 1.0f);
  track.NoteOn(67, 1.0f);
  assert(HeldVoices(track) == 3 && track.CurrentStatus().onsets == onsets + 1);
  for (std::size_t index = 0; index < kVoiceCount; ++index)
    if (track.VoiceAt(index).Held()) assert(track.VoiceAt(index).Entry() == kKu);
  track.NoteOff(64);
  assert(HeldVoices(track) == 2);
  track.AllNotesOff();
  assert(!track.Held() && HeldVoices(track) == 0);
  // More notes than voices: the newest take the oldest voices, and nothing breaks.
  for (std::uint8_t note = 60; note < 70; ++note) track.NoteOn(note, 1.0f);
  assert(HeldVoices(track) == kVoiceCount);
  track.AllNotesOff();
  assert(track.SetParameter("voice.mode", 0));

  // The sound: a voice in the block at the track's gain, and silence after the tail.
  assert(track.SetParameter("voice.word", static_cast<int>(kA)));
  track.NoteOn(69, 1.0f);
  std::vector<float> audio(22050, 0.0f);
  for (std::size_t done = 0; done < audio.size(); done += 256) track.Render(audio.data() + done, (std::min<std::size_t>)(256, audio.size() - done));
  const float level = Peak(audio, 8000, 22050);
  assert(level > 0.05f && level < kVoiceGain * 1.05f);
  track.NoteOff(69);
  for (int blocks = 0; blocks < 40; ++blocks) track.Render(block.data(), block.size());
  assert(!track.Active());

  // Another pack: everything stops, and a key that waited is gone.
  key(kKu);
  track.NoteOn(60, 1.0f);
  key(kSa);
  track.NoteOff(60);
  key(kKu);
  track.SetPack(Pack::Load(kBuiltinPack.data(), kBuiltinPack.size()));
  assert(!track.Active() && !track.Held());
  assert(sing(60) == kA);
}

}  // namespace

int main()
{
  TestBuiltinPack();
  TestDamagedPacks();
  TestVoiceSound();
  TestTimedPronunciationEnds();
  TestOscillator();
  TestWordKeys();
  TestTrack();
  std::puts("vocal_test: ok");
  return 0;
}
