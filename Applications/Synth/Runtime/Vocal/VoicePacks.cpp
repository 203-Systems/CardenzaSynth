#include "Applications/Synth/Runtime/Vocal/VoicePacks.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <memory>
#include <new>

#include "Library/Memory/Memory.h"
#include "Library/Synth/Core/SynthStorage.h"
#include "OS/Storage/Storage.h"

namespace synth {

namespace {

bool EndsWithExtension(const std::string& name)
{
  const std::string extension = kVoicePackExtension;
  if (name.size() <= extension.size()) return false;
  for (std::size_t index = 0; index < extension.size(); ++index)
    if (std::tolower(static_cast<unsigned char>(name[name.size() - extension.size() + index])) != extension[index]) return false;
  return true;
}

}  // namespace

bool IsVoicePackFileName(const std::string& fileName)
{
  return fileName.size() <= 64 && EndsWithExtension(fileName) && fileName.front() != '.' &&
         fileName.find_first_of("/\\:\"") == std::string::npos &&
         std::all_of(fileName.begin(), fileName.end(), [](unsigned char c) { return c >= 32 && c < 127; });
}

std::vector<std::string> ListVoicePacks()
{
  std::vector<std::string> names;
  if (!os::storage::CardPresent()) return names;
  for (const os::storage::DirectoryEntry& entry : os::storage::ListDirectory(storage::VoiceDirectory()))
    if (!entry.directory && IsVoicePackFileName(entry.name)) names.push_back(entry.name);
  return names;
}

vocal::PackRef LoadVoicePack(const std::string& fileName)
{
  if (!IsVoicePackFileName(fileName) || !os::storage::CardPresent()) return {};
  const std::string path = os::storage::JoinPath(storage::VoiceDirectory(), fileName);
  const long long size = os::storage::FileSize(path);
  if (size < static_cast<long long>(vocal::kHeaderBytes) || size > static_cast<long long>(vocal::kMaxPackBytes))
  {
    if (size >= 0) std::printf("Groove: voice %s is not a pack (%lld bytes)\n", fileName.c_str(), size);
    return {};
  }
  // The file is gathered in PSRAM, through a small block in internal RAM: the card's DMA cannot read
  // into PSRAM.
  constexpr std::size_t kChunkBytes = 2048;
  std::unique_ptr<std::uint8_t[]> chunk(new (std::nothrow) std::uint8_t[kChunkBytes]);
  std::unique_ptr<std::uint8_t[]> bytes(new (memory::slow) std::uint8_t[static_cast<std::size_t>(size)]);
  if (!chunk || !bytes)
  {
    std::printf("Groove: no memory to read voice %s (%lld bytes)\n", fileName.c_str(), size);
    return {};
  }
  std::FILE* file = std::fopen(path.c_str(), "rb");
  if (file == nullptr)
  {
    os::storage::ReportCardIoError();
    return {};
  }
  std::size_t read = 0;
  while (read < static_cast<std::size_t>(size))
  {
    const std::size_t want = (std::min)(kChunkBytes, static_cast<std::size_t>(size) - read);
    const std::size_t got = std::fread(chunk.get(), 1, want, file);
    if (got == 0) break;
    std::copy_n(chunk.get(), got, bytes.get() + read);
    read += got;
  }
  const bool failed = std::ferror(file) != 0;
  std::fclose(file);
  if (failed || read != static_cast<std::size_t>(size))
  {
    std::printf("Groove: voice %s could not be read (%u of %lld bytes)\n", fileName.c_str(), static_cast<unsigned>(read), size);
    os::storage::ReportCardIoError();
    return {};
  }
  vocal::PackRef pack = vocal::Pack::Load(bytes.get(), read);
  if (!pack) std::printf("Groove: voice %s is not a valid pack\n", fileName.c_str());
  else std::printf("Groove: voice %s loaded: %s, %u pronunciations, %u bytes\n", fileName.c_str(), pack->Name(), pack->EntryCount(),
                   static_cast<unsigned>(read));
  return pack;
}

std::string VoicePackLabel(const std::string& fileName)
{
  std::string label = fileName;
  if (EndsWithExtension(label)) label.resize(label.size() - std::string(kVoicePackExtension).size());
  std::transform(label.begin(), label.end(), label.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  return label;
}

}  // namespace synth
