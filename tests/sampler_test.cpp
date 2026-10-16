#include "Applications/Synth/Runtime/Sampler/SamplerManager.h"
#include "Applications/Synth/Runtime/Sampler/SamplerWav.h"
#include "Devices/Device.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <vector>

namespace {
std::filesystem::path testDirectory;
std::vector<std::int16_t> input;
std::size_t inputCursor = 0;
bool overflow = false;
bool inputRunning = false;
synth::audio::SharedSampleBuffer published;
std::array<synth::audio::SharedSampleBuffer, synth::kSynthTrackCount> publishedByTrack;
std::uint32_t publishedRate = 0;
std::vector<char> Bytes(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
void Put32(std::vector<char>& bytes, std::size_t offset, std::uint32_t value) {
  for (int i = 0; i < 4; ++i) bytes[offset + i] = static_cast<char>(value >> (8 * i));
}
}
namespace synth::audio {
void SetSamplerFrames(std::size_t track, SharedSampleBuffer frames, std::uint32_t rate) {
  publishedByTrack.at(track) = frames;
  published = std::move(frames); publishedRate = rate;
}
void NoteOn(std::size_t, std::uint8_t, std::uint8_t) {}
void NoteOff(std::size_t, std::uint8_t) {}
}
namespace Device::Storage {
std::string SampleDirectoryPath() { return testDirectory.string(); }
bool EnsureDirectoryLayout() { return true; }
}
namespace Device::Audio {
InputStreamInfo InputInfo() { return {}; }
InputStatus StartInput() { inputCursor = 0; inputRunning = true; return InputStatus::Ready; }
void StopInput() { inputRunning = false; }
InputStatus ReadInputFrames(std::int16_t* destination, std::size_t count, std::size_t& read, std::uint32_t) {
  read = 0;
  if (overflow) { overflow = false; return InputStatus::Overflow; }
  read = std::min(count, input.size() - inputCursor);
  std::copy_n(input.data() + inputCursor, read, destination);
  inputCursor += read;
  return InputStatus::Ready;
}
}
int main() {
  using namespace synth;
  testDirectory = std::filesystem::temp_directory_path() /
      ("computecard-sampler-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directory(testDirectory);
  const std::string path = (testDirectory / "original.WAV").string();
  const std::array<std::int16_t, 4> original{{-32768, -17, 0, 32767}};
  assert(sampler::WriteMono16Wav(path, original.data(), original.size(), 22050));
  audio::SharedSampleBuffer loaded;
  std::uint32_t rate = 0;
  assert(sampler::ReadMono16Wav(path, loaded, rate));
  assert(rate == 22050 && loaded->frameCount == original.size());
  assert(std::equal(original.begin(), original.end(), loaded->frames.get()));
  const auto valid = Bytes(path);
  const auto saved = loaded;
  auto reject = [&](std::vector<char> bytes) {
    const auto malformedPath = (testDirectory / "bad.wav").string();
    std::ofstream file(malformedPath, std::ios::binary);
    file.write(bytes.data(), bytes.size()); file.close();
    assert(!sampler::ReadMono16Wav(malformedPath, loaded, rate));
    assert(loaded == saved && rate == 22050);
  };
  auto malformed = valid;
  Put32(malformed, 40, 7); reject(malformed); // Odd PCM length with a pad.
  malformed = valid; malformed.pop_back(); reject(malformed);
  malformed = valid; Put32(malformed, 40, 0xfffffff0u); reject(malformed);
  malformed = valid; Put32(malformed, 16, 8); reject(malformed); // Short fmt.
  malformed = valid; malformed[22] = 2; reject(malformed); // Unsupported stereo.
  malformed = valid; Put32(malformed, 28, 1); reject(malformed); // Wrong byte rate.
  malformed = valid; Put32(malformed, 24, 0); reject(malformed);
  malformed = valid; malformed.resize(44 + (sampler::kMaxSampleFrames + 1) * 2);
  Put32(malformed, 4, static_cast<std::uint32_t>(malformed.size() - 8));
  Put32(malformed, 40, static_cast<std::uint32_t>(malformed.size() - 44)); reject(malformed);

  // Padded unknown chunks and data preceding fmt are valid RIFF layouts.
  std::vector<char> unusual(valid.begin(), valid.begin() + 12);
  unusual.insert(unusual.end(), {'J', 'U', 'N', 'K', 1, 0, 0, 0, 42, 0});
  unusual.insert(unusual.end(), valid.begin() + 36, valid.end());
  unusual.insert(unusual.end(), valid.begin() + 12, valid.begin() + 36);
  Put32(unusual, 4, static_cast<std::uint32_t>(unusual.size() - 8));
  const auto unusualPath = (testDirectory / "unusual.wav").string();
  { std::ofstream file(unusualPath, std::ios::binary); file.write(unusual.data(), unusual.size()); }
  assert(sampler::ReadMono16Wav(unusualPath, loaded, rate));
  assert(std::equal(original.begin(), original.end(), loaded->frames.get()));

  // Failed replacement preserves both the original and an earlier recovery backup.
  { std::ofstream backup(path + ".bak", std::ios::binary); backup << "recover me"; }
  assert(!sampler::WriteMono16Wav(path, original.data(), 2, 44100));
  assert(Bytes(path) == valid && Bytes(path + ".bak").size() == 10);
  std::filesystem::remove(path + ".bak");
  assert(sampler::WriteMono16Wav(path, original.data(), original.size(), 22050));
  assert(!std::filesystem::exists(path + ".bak") && !std::filesystem::exists(path + ".tmp"));

  SamplerManager manager;
  manager.OnTrackEngineChanged(0, SynthEngineKind::Sampler);
  assert(manager.SelectBrowserSample(0, "original.WAV"));
  assert(manager.BuildUiState(0).sampleFileName == "original.WAV");
  assert(manager.ConsumeTrimReset(0) && !manager.ConsumeTrimReset(0));
  const auto current = published;
  assert(!manager.SelectBrowserSample(0, "../original.WAV") && published == current);
  assert(!manager.SelectBrowserSample(0, "bad.wav") && published == current);
  assert(!sampler::AllocateFrames(0) && !sampler::AllocateFrames(sampler::kMaxSampleFrames + 1));
  assert(manager.BeginRecord(0) && !manager.StopRecord(0));
  assert(manager.BuildUiState(0).frameCount == original.size());
  assert(manager.BuildUiState(0).sampleRate == 22050);
  input.assign(4096, 321);
  assert(manager.BeginRecord(0)); manager.Tick();
  assert(inputCursor == input.size()); // Drain more than 512 frames per UI tick.
  assert(manager.StopRecord(0) && manager.BuildUiState(0).frameCount == input.size());
  manager.DiscardDraftTake(0);
  assert(manager.BuildUiState(0).frameCount == original.size());
  assert(manager.BuildUiState(0).sampleRate == 22050 && publishedRate == 22050);
  assert(manager.BeginRecord(0) && manager.StopRecord(0));
  manager.SetDraftTrimWindow(0, 32, 96);
  const auto trim = manager.BuildUiState(0);
  assert(manager.CommitDraftName(0, "trimmed"));
  assert(manager.BuildUiState(0).frameCount == trim.trimEndFrame - trim.trimStartFrame);
  assert(manager.ConsumeTrimReset(0));
  SamplerManager::PreparedSample prepared;
  assert(SamplerManager::PrepareSample("trimmed", prepared));
  assert(prepared.frames->frameCount == manager.BuildUiState(0).frameCount);
  assert(SamplerManager::ValidateSampleReference("trimmed"));
  assert(manager.BeginRecord(0)); overflow = true; manager.Tick();
  assert(!manager.BuildUiState(0).recording && manager.BuildUiState(0).statusText == "INPUT OVERRUN");
  manager.DiscardDraftTake(0); assert(manager.BuildUiState(0).hasSample);

  // Shared file renames update references without changing either track's sound or trim.
  SamplerManager files;
  files.OnTrackEngineChanged(0, SynthEngineKind::Sampler);
  files.OnTrackEngineChanged(1, SynthEngineKind::Sampler);
  assert(files.SelectBrowserSample(0, "original.WAV"));
  assert(files.SelectBrowserSample(1, "original.WAV"));
  files.SetDraftTrimWindow(1, 32, 96);
  const auto sharedTrim = files.BuildUiState(1);
  const auto sharedAudio = publishedByTrack;
  assert(files.RenameCurrentSample(0, "shared"));
  const auto sharedPath = (testDirectory / "shared.wav").string();
  assert(!std::filesystem::exists(path) && Bytes(sharedPath) == valid);
  for (std::size_t track = 0; track < 2; ++track) assert(files.BuildUiState(track).sampleFileName == "shared.wav");
  assert(publishedByTrack == sharedAudio);
  assert(files.BuildUiState(1).trimStartFrame == sharedTrim.trimStartFrame &&
         files.BuildUiState(1).trimEndFrame == sharedTrim.trimEndFrame);
  assert(!files.DeleteCurrentSample(0) && files.BuildUiState(0).deleteConfirm);
  assert(!files.DeleteCurrentSample(0) && files.BuildUiState(0).statusText == "FILE IN USE");
  assert(Bytes(sharedPath) == valid && publishedByTrack == sharedAudio);

  // Browser file actions never load their target into the performing track.
  const auto sparePath = (testDirectory / "spare.wav").string();
  const auto renamedPath = (testDirectory / "renamed.wav").string();
  assert(sampler::WriteMono16Wav(sparePath, original.data(), original.size(), 22050));
  std::string renamedFile;
  assert(files.RenameBrowserSample(0, "spare.wav", "renamed", &renamedFile) && renamedFile == "renamed.wav");
  assert(!std::filesystem::exists(sparePath) && Bytes(renamedPath) == valid);
  assert(files.BuildUiState(0).sampleFileName == "shared.wav" && publishedByTrack == sharedAudio);
  assert(!files.RenameCurrentSample(0, "renamed")); // Existing file collision.
  assert(Bytes(renamedPath) == valid && Bytes(sharedPath) == valid);
  std::filesystem::create_directory(testDirectory / "blocked.wav");
  assert(!files.RenameCurrentSample(0, "blocked")); // Existing directory collision.
  assert(files.BuildUiState(0).sampleFileName == "shared.wav");

  // Confirmation belongs to one exact target and cancellation revokes it.
  assert(!files.DeleteBrowserSample(0, "renamed.wav"));
  assert(!files.DeleteBrowserSample(0, "shared.wav"));
  assert(std::filesystem::exists(renamedPath) && std::filesystem::exists(sharedPath));
  files.CancelNameEntry(0);
  assert(!files.BuildUiState(0).deleteConfirm);
  assert(!files.DeleteBrowserSample(0, "renamed.wav"));
  assert(files.DeleteBrowserSample(0, "renamed.wav"));
  assert(!std::filesystem::exists(renamedPath) && publishedByTrack == sharedAudio);

  // A failed filesystem operation leaves loaded PCM and metadata available.
  files.OnTrackEngineChanged(1, SynthEngineKind::None);
  std::filesystem::rename(sharedPath, sharedPath + ".hold");
  assert(!files.RenameCurrentSample(0, "shared")); // Even a no-op requires a real source.
  std::filesystem::create_directory(sharedPath);
  const auto beforeFailedDelete = publishedByTrack[0];
  assert(!files.DeleteCurrentSample(0));
  assert(!files.DeleteCurrentSample(0) && files.BuildUiState(0).statusText == "DEL ERR");
  assert(std::filesystem::is_directory(sharedPath));
  assert(files.BuildUiState(0).hasSample && publishedByTrack[0] == beforeFailedDelete);
  std::filesystem::remove(sharedPath);
  std::filesystem::rename(sharedPath + ".hold", sharedPath);

  // Loading or managing a file cannot silently replace an in-progress take.
  assert(files.BeginRecord(0)); files.Tick();
  assert(inputRunning && files.BuildUiState(0).recording);
  assert(!files.SelectBrowserSample(0, "trimmed.wav"));
  assert(!files.ApplySample(0, prepared));
  assert(!files.RenameCurrentSample(0, "lost"));
  assert(!files.DeleteCurrentSample(0));
  assert(inputRunning && files.BuildUiState(0).recording && Bytes(sharedPath) == valid);
  assert(files.StopRecord(0) && !inputRunning);
  files.SetDraftTrimWindow(0, 32, 96);
  assert(files.PreviewCurrentSample(0, 60));
  const auto draft = files.BuildUiState(0);
  const auto draftAudio = publishedByTrack[0];
  assert(!files.BeginRecord(0) && files.BuildUiState(0).statusText == "SAVE SAMPLE FIRST");
  assert(!inputRunning && files.BuildUiState(0).previewing && publishedByTrack[0] == draftAudio);
  assert(!files.SelectBrowserSample(0, "trimmed.wav") && !files.ApplySample(0, prepared));
  assert(!files.RenameCurrentSample(0, "lost") && !files.DeleteCurrentSample(0));
  assert(files.BuildUiState(0).hasDraft && files.BuildUiState(0).trimStartFrame == draft.trimStartFrame &&
         files.BuildUiState(0).trimEndFrame == draft.trimEndFrame && publishedByTrack[0] == draftAudio);
  assert(!files.ConfirmOverwriteDraft(0)); // No pending overwrite authorization.
  assert(!files.CommitDraftName(0, "blocked") && !files.BuildUiState(0).overwritePending);
  assert(std::filesystem::is_directory(testDirectory / "blocked.wav") && files.BuildUiState(0).hasDraft);
  files.OnTrackEngineChanged(1, SynthEngineKind::Sampler);
  assert(files.SelectBrowserSample(1, "shared.wav"));
  assert(!files.CommitDraftName(0, "shared") && files.BuildUiState(0).overwritePending);
  assert(!files.ConfirmOverwriteDraft(0) && files.BuildUiState(0).statusText == "FILE IN USE");
  assert(files.BuildUiState(0).hasDraft && Bytes(sharedPath) == valid);
  assert(files.BeginRecord(0, true) && inputRunning && files.BuildUiState(0).recording);
  assert(!files.BuildUiState(0).hasDraft && !files.BuildUiState(0).overwritePending);
  assert(files.StopRecord(0) && files.BuildUiState(0).hasDraft);
  assert(files.BuildUiState(0).trimStartFrame == 0 && files.BuildUiState(0).frameCount == input.size());
  files.DiscardDraftTake(0);
  assert(!files.BuildUiState(0).overwritePending && !files.ConfirmOverwriteDraft(0));
  files.OnTrackEngineChanged(1, SynthEngineKind::None);
  assert(!files.DeleteBrowserSample(0, "shared.wav"));
  assert(files.DeleteBrowserSample(0, "shared.wav"));
  assert(!files.BuildUiState(0).hasSample && files.BuildUiState(0).sampleFileName.empty());
  assert(!publishedByTrack[0] && !std::filesystem::exists(sharedPath));
  std::filesystem::remove_all(testDirectory);
  std::cout << "sampler WAV, replacement, recording, trim, shared file and confirmation checks passed\n";
}
