#include "Applications/BuiltInApplications.h"

#include "Applications/Synth/SynthApp.h"

namespace app {

void RegisterBuiltInApplications()
{
  static bool registered = false;
  if (registered)
  {
    return;
  }

  apps::RegisterSynthApplication();
  registered = true;
}

const std::string& DefaultApplicationId()
{
  static const std::string applicationId = MakeApplicationId("Cardenza", "Synth");
  return applicationId;
}

std::unique_ptr<Application> CreateDefaultApplication()
{
  return CreateApplication(DefaultApplicationId());
}

}  // namespace app