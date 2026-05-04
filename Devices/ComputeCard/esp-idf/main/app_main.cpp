#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "Devices/Device.h"
#include "OS/System/System.h"

namespace {

constexpr char kLogTag[] = "ComputeCard";
constexpr TickType_t kFrameTick = pdMS_TO_TICKS(16);
constexpr uint32_t kRuntimeTaskStackWords = 16384 / sizeof(StackType_t);
constexpr UBaseType_t kRuntimeTaskPriority = 5;

void ComputeCardRuntimeTask(void*)
{
    if (!os::system::Begin())
    {
        ESP_LOGE(kLogTag, "Failed to boot ComputeCard runtime");
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(kLogTag,
             "%s [%s] %dx%d",
             Device::Name().c_str(),
             Device::BackendName().c_str(),
             Device::Display::kWidth,
             Device::Display::kHeight);

    for (;;)
    {
        os::system::RunFrame();
        vTaskDelay(kFrameTick);
    }
}

}  // namespace

extern "C" void app_main(void)
{
    const BaseType_t created = xTaskCreate(
        ComputeCardRuntimeTask,
        "cc_runtime",
        kRuntimeTaskStackWords,
        nullptr,
        kRuntimeTaskPriority,
        nullptr);

    if (created != pdPASS)
    {
        ESP_LOGE(kLogTag, "Failed to create ComputeCard runtime task");
        return;
    }
}