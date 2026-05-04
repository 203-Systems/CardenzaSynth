#pragma once

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace app {

class Application {
public:
  virtual ~Application() = default;
  virtual void Setup(const std::vector<std::string>& args) = 0;
  virtual void Tick() = 0;
  virtual void RenderFrame() = 0;
};

using ApplicationFactory = std::unique_ptr<Application> (*)();

struct ApplicationInfo {
  std::string id;
  std::string name;
  std::string author;
  bool isSystem = false;
  ApplicationFactory factory = nullptr;
};

inline std::map<std::string, ApplicationInfo>& Registry() {
  static std::map<std::string, ApplicationInfo> registry;
  return registry;
}

inline std::string MakeApplicationId(std::string_view author, std::string_view name) {
  return std::string(author) + "/" + std::string(name);
}

inline bool RegisterApplication(const ApplicationInfo& info) {
  return Registry().emplace(info.id, info).second;
}

inline std::unique_ptr<Application> CreateApplication(const std::string& id) {
  const auto iterator = Registry().find(id);
  if (iterator == Registry().end() || iterator->second.factory == nullptr)
  {
    return nullptr;
  }
  return iterator->second.factory();
}

inline std::vector<ApplicationInfo> ListApplications() {
  std::vector<ApplicationInfo> applications;
  applications.reserve(Registry().size());
  for (const auto& entry : Registry())
  {
    applications.push_back(entry.second);
  }
  return applications;
}

template <typename T> std::unique_ptr<Application> MakeApplication() {
  return std::make_unique<T>();
}

}  // namespace app
