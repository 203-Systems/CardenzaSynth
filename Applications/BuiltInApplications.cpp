#include "Applications/BuiltInApplications.h"

#include "Applications/Synth/SynthApp.h"

namespace app {

void RegisterBuiltInApplications()
{
  apps::RegisterSynthApplication();
}

const std::string& DefaultApplicationId()
{
  static const std::string applicationId = MakeApplicationId("ComputeCard", "Synth");
  return applicationId;
}

std::unique_ptr<Application> CreateDefaultApplication()
{
  RegisterBuiltInApplications();
  return CreateApplication(DefaultApplicationId());
}

}  // namespace app