#pragma once

#include <memory>

#include "Applications/Application.h"

namespace app {

void RegisterBuiltInApplications();
const std::string& DefaultApplicationId();
std::unique_ptr<Application> CreateDefaultApplication();

}  // namespace app