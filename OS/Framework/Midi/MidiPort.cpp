#include "OS/Framework/Midi/MidiPort.h"

namespace midi {

std::map<uint16_t, MidiPort*>& MidiPort::Registry() {
  static auto* registry = new std::map<uint16_t, MidiPort*>();
  return *registry;
}

std::mutex& MidiPort::RegistryMutex() {
  static auto* mutex = new std::mutex();
  return *mutex;
}

MidiPort::MidiPort(std::string name, uint16_t id, std::size_t queueSize)
    : name_(std::move(name)) {
  Open(id, queueSize, 1);
}

MidiPort::MidiPort(std::string name, MidiPortId portClass, std::size_t queueSize)
    : name_(std::move(name)) {
  Open(static_cast<uint16_t>(portClass), queueSize, 0x100);
}

MidiPort::~MidiPort() {
  Close();
}

uint16_t MidiPort::Open(uint16_t id, std::size_t queueSize, uint16_t idRange) {
  Close();
  if (id == MIDI_PORT_INVALID)
  {
    return MIDI_PORT_INVALID;
  }

  queueSize_ = queueSize;
  for (uint16_t offset = 0; offset < idRange; ++offset)
  {
    const uint16_t candidate = static_cast<uint16_t>(id + offset);
    if (OpenMidiPort(candidate, this))
    {
      id_ = candidate;
      return id_;
    }
  }

  return MIDI_PORT_INVALID;
}

void MidiPort::Close() {
  if (id_ == MIDI_PORT_INVALID)
  {
    return;
  }

  CloseMidiPort(id_);
  id_ = MIDI_PORT_INVALID;
}

bool MidiPort::Get(MidiPacket* destination) {
  if (destination == nullptr)
  {
    return false;
  }

  std::lock_guard<std::mutex> lock(queueMutex_);
  if (queue_.empty())
  {
    return false;
  }

  *destination = queue_.front();
  queue_.pop_front();
  return true;
}

bool MidiPort::Send(MidiPacket packet, uint16_t targetPort) {
  packet.port = id_;
  return RouteMidiPacket(packet, targetPort);
}

bool MidiPort::Receive(MidiPacket packet) {
  std::lock_guard<std::mutex> lock(queueMutex_);
  if (queue_.size() >= queueSize_)
  {
    queue_.pop_front();
  }
  queue_.push_back(packet);
  return true;
}

const std::string& MidiPort::Name() const {
  return name_;
}

void MidiPort::SetName(std::string name) {
  name_ = std::move(name);
}

uint16_t MidiPort::Id() const {
  return id_;
}

bool MidiPort::OpenMidiPort(uint16_t portId, MidiPort* port) {
  if (portId < 0x0100 || port == nullptr)
  {
    return false;
  }

  std::lock_guard<std::mutex> lock(RegistryMutex());
  auto& registry = Registry();
  if (registry.find(portId) != registry.end())
  {
    return false;
  }

  registry[portId] = port;
  return true;
}

void MidiPort::CloseMidiPort(uint16_t portId) {
  std::lock_guard<std::mutex> lock(RegistryMutex());
  Registry().erase(portId);
}

bool MidiPort::RouteMidiPacket(MidiPacket packet, uint16_t targetPort) {
  const uint16_t sourcePort = packet.port;
  std::lock_guard<std::mutex> lock(RegistryMutex());
  auto& registry = Registry();

  if (targetPort == MIDI_PORT_EACH_CLASS)
  {
    bool sent = false;
    uint16_t nextClass = MIDI_PORT_USB;
    for (const auto& entry : registry)
    {
      if (entry.first >= MIDI_PORT_DEVICE_CUSTOM + 0x100)
      {
        break;
      }
      if (entry.first >= nextClass && entry.first != sourcePort)
      {
        sent |= entry.second->Receive(packet);
        nextClass = static_cast<uint16_t>((entry.first / 0x100 + 1) * 0x100);
      }
    }
    return sent;
  }

  if (targetPort == MIDI_PORT_ALL)
  {
    bool sent = false;
    for (const auto& entry : registry)
    {
      if (entry.first != sourcePort)
      {
        sent |= entry.second->Receive(packet);
      }
    }
    return sent;
  }

  const auto iterator = registry.find(targetPort);
  if (iterator == registry.end() || iterator->first == sourcePort)
  {
    return false;
  }
  return iterator->second->Receive(packet);
}

}  // namespace midi
