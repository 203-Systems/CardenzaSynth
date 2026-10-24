#include "Applications/BuiltInApplications.h"

#if defined(CARDENZA_GROOVE_UI)
#include "Applications/Synth/GrooveApp.h"
#else
#include "Applications/Synth/SynthApp.h"
#endif

namespace app {

void RegisterBuiltInApplications()
{
  static bool registered = false;
  if (registered)
  {
    return;
  }

#if defined(CARDENZA_GROOVE_UI)
  apps::RegisterGrooveApplication();
#else
  apps::RegisterSynthApplication();
#endif
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