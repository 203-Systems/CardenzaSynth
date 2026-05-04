#include "Devices/ComputeCard/main/hal/computecard/computecardHAL.h"

namespace {

esp_err_t KeepFirstError(esp_err_t current, esp_err_t next)
{
    return current == ESP_OK ? next : current;
}

}  // namespace

namespace ComputeCardHAL {

esp_err_t Init()
{
    esp_err_t firstError = System::Init();
    if (firstError != ESP_OK)
    {
        return firstError;
    }

    Keyboard::init();
    firstError = KeepFirstError(firstError, Audio::Init());
    firstError = KeepFirstError(firstError, Display::Init());
    firstError = KeepFirstError(firstError, Storage::Init());
    firstError = KeepFirstError(firstError, IR::Init());
    return firstError;
}

}  // namespace ComputeCardHAL
