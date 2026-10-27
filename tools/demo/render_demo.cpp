#include "Applications/Synth/Runtime/ProjectStorage.h"
#include "Applications/Synth/Runtime/SynthStorage.h"
#include "Devices/Device.h"
#include "Devices/DeviceAudio.h"
#include "Library/Synth/Audio/SynthAudioRuntime.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Offline transport: rendering never starts a desktop speaker thread.
namespace device {
void SetSynthAudioRenderProvider(SynthAudioRenderProvider) {}
SynthAudioPipelineBinding ConnectSynthAudioPipeline(std::size_t index) { return {index, true, false, "offline"}; }
}

namespace {
using namespace synth;
struct Event {
  long long frame;
  int priority;
  int note = 0;
  int value = 0;
  std::string parameter;
};
struct Song {
  ProjectData project;
  std::array<std::vector<Event>, 4> events;
};
void Require(bool ok, const std::string& text) { if (!ok) throw std::runtime_error(text); }
long long Frame(double pulse, int bpm) { return std::llround(pulse * 60.0 * audio::kSampleRate / (bpm * 96.0)); }

Song Read(const std::string& path)
{
  std::ifstream input(path);
  Require(input.good(), "Cannot open score");
  Song song;
  auto& p = song.project;
  p.sequence = std::make_unique<SynthSequencer::ProjectState>();
  p.root = 4; p.scale = 1; p.octave = 0; p.activeTrack = 1;
  for (auto& track : p.sequence->tracks) { track.patternCount = 8; track.activePattern = 0; }
  std::array<std::unique_ptr<ISynthEngine>, 4> engines;
  std::string command;
  while (input >> command)
  {
    if (command == "TEMPO") input >> p.sequence->bpm;
    else if (command == "PATCH")
    {
      int track, kind; std::string preset, name;
      input >> track >> kind >> std::quoted(preset) >> std::quoted(name);
      p.kinds.at(track) = static_cast<SynthEngineKind>(kind);
      engines.at(track) = CreateSynthEngine(p.kinds.at(track));
      Require(engines.at(track) && engines.at(track)->LoadPreset(preset), "Invalid patch " + preset);
      p.engines.at(track) = engines.at(track)->CaptureState();
      p.engines.at(track).presetName = name;
    }
    else if (command == "PARAM")
    {
      int track, value; std::string parameter;
      input >> track >> parameter >> value;
      Require(engines.at(track) && engines.at(track)->SetParameterValue(parameter, value), "Invalid parameter " + parameter);
      p.engines.at(track).values[parameter] = value;
    }
    else if (command == "STEP")
    {
      int track, pattern, step, pitch, velocity, length, micro;
      input >> track >> pattern >> step >> pitch >> velocity >> length >> micro;
      auto& notes = p.sequence->tracks.at(track).patterns[pattern].steps.at(step).notes;
      auto slot = std::find_if(notes.begin(), notes.end(), [](const auto& n) { return !n.active; });
      Require(slot != notes.end(), "Pattern exceeds eight notes per step");
      *slot = {static_cast<std::uint8_t>(pitch), static_cast<std::uint8_t>(velocity),
               static_cast<std::uint16_t>(length), static_cast<std::uint8_t>(micro), true};
    }
    else if (command == "NOTE")
    {
      int track, pitch, velocity; double pulse, length;
      input >> track >> pulse >> pitch >> velocity >> length;
      song.events.at(track).push_back({Frame(pulse, p.sequence->bpm), 2, pitch, velocity, {}});
      song.events.at(track).push_back({Frame(pulse + length, p.sequence->bpm), 0, pitch, 0, {}});
    }
    else if (command == "AUTO")
    {
      int track, value; double pulse; std::string parameter;
      input >> track >> pulse >> parameter >> value;
      song.events.at(track).push_back({Frame(pulse, p.sequence->bpm), 1, 0, value, parameter});
    }
    else throw std::runtime_error("Unknown score command " + command);
    Require(!input.fail(), "Malformed score command " + command);
  }
  Require(SynthSequencer::ValidateState(*p.sequence), "Invalid sequence state");
  for (int track = 0; track < 4; ++track)
  {
    Require(engines[track] && engines[track]->RestoreState(p.engines[track]), "Invalid saved synth state");
    auto& events = song.events[track];
    std::stable_sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
      return a.frame == b.frame ? a.priority < b.priority : a.frame < b.frame;
    });
  }
  return song;
}
void LE(std::ostream& out, std::uint32_t value, int bytes)
{
  for (int i = 0; i < bytes; ++i) out.put(static_cast<char>((value >> (8 * i)) & 255));
}
void Render(const Song& song, int track, const std::string& path, double seconds)
{
  auto engine = CreateSynthEngine(song.project.kinds.at(track));
  Require(engine && engine->Init({0, 0x4300, "OfflineDemo"}) && engine->RestoreState(song.project.engines.at(track)), "Cannot initialize renderer");
  std::array<std::int16_t, 256> buffer{};
  audio::RenderFrames(buffer.data(), buffer.size()); // Settle the patch before the musical timeline.
  const long long total = std::llround(seconds * audio::kSampleRate);
  std::ofstream out(path, std::ios::binary);
  Require(out.good(), "Cannot create WAV");
  out.write("RIFF", 4); LE(out, static_cast<std::uint32_t>(36 + total * 2), 4);
  out.write("WAVEfmt ", 8); LE(out, 16, 4); LE(out, 1, 2); LE(out, 1, 2);
  LE(out, audio::kSampleRate, 4); LE(out, audio::kSampleRate * 2, 4); LE(out, 2, 2); LE(out, 16, 2);
  out.write("data", 4); LE(out, static_cast<std::uint32_t>(total * 2), 4);
  const auto& events = song.events.at(track);
  std::size_t cursor = 0;
  long long frame = 0;
  long long clipped = 0;
  int peak = 0;
  while (frame < total)
  {
    while (cursor < events.size() && events[cursor].frame <= frame)
    {
      const auto& e = events[cursor++];
      if (!e.parameter.empty()) audio::SetEngineParameter(0, e.parameter, e.value);
      else if (e.priority == 2) audio::NoteOn(0, static_cast<std::uint8_t>(e.note), static_cast<std::uint8_t>(e.value));
      else audio::NoteOff(0, static_cast<std::uint8_t>(e.note));
    }
    long long count = std::min<long long>(buffer.size(), total - frame);
    if (cursor < events.size()) count = std::min(count, events[cursor].frame - frame);
    Require(count > 0, "Invalid event ordering");
    audio::RenderFrames(buffer.data(), static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
    {
      peak = std::max(peak, std::abs(static_cast<int>(buffer[i])));
      clipped += std::abs(static_cast<int>(buffer[i])) >= 32760;
      LE(out, static_cast<std::uint16_t>(buffer[i]), 2);
    }
    frame += count;
  }
  Require(out.good(), "WAV write failed");
  audio::AllNotesOff(0);
  std::cout << "Track " << track + 1 << ": " << seconds << "s; peak=" << peak << "; clipped samples=" << clipped << '\n';
}
}

int main(int argc, char** argv)
{
  try
  {
    Require(argc >= 3, "Usage: CardenzaDemo score.txt export | track-index output.wav [seconds]");
    auto song = Read(argv[1]);
    if (std::string(argv[2]) == "export")
    {
      Require(synth::storage::EnsureLayout(), "Storage unavailable");
      const std::array<std::string, 4> names{{"NeonCircuit_Drop", "NeonCircuit_Intro", "NeonCircuit_Break", "NeonCircuit_Build"}};
      const std::array<std::size_t, 4> patterns{{0, 5, 6, 7}};
      for (std::size_t version = 0; version < names.size(); ++version)
      {
        for (auto& track : song.project.sequence->tracks) track.activePattern = patterns[version];
        Require(SaveProjectFile(names[version], song.project, false) == ProjectResult::Ok, "Cannot save project " + names[version]);
        ProjectData readback;
        Require(LoadProjectFile(names[version], readback) == ProjectResult::Ok, "Project readback failed");
        for (int track = 0; track < 4; ++track)
        {
          auto engine = CreateSynthEngine(readback.kinds[track]);
          Require(engine && engine->RestoreState(readback.engines[track]), "Readback engine restore failed");
          Require(readback.engines[track].values == song.project.engines[track].values, "Parameter readback mismatch");
          Require(readback.sequence->tracks[track].activePattern == patterns[version], "Pattern readback mismatch");
        }
        std::cout << "Saved and verified " << names[version] << '\n';
      }
    }
    else
    {
      Require(argc >= 4, "Output WAV path required");
      Render(song, std::stoi(argv[2]), argv[3], argc > 4 ? std::stod(argv[4]) : 120.0);
    }
  }
  catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
