#pragma once

#include <vector>
#include <string>

#include "Applications/Application.h"

namespace os::system {

struct BootOptions {
  std::vector<std::string> applicationArgs;
};

bool Begin(const BootOptions& options = {});
void RunFrame();
app::Application* ActiveApplication();

}  // namespace os::system