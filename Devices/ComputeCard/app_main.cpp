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
constexpr BaseType_t kRuntimeTaskPinnedCore = 0;

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
    BaseType_t created = pdFAIL;
#if portNUM_PROCESSORS > 1
    created = xTaskCreatePinnedToCore(
        ComputeCardRuntimeTask,
        "cc_runtime",
        kRuntimeTaskStackWords,
        nullptr,
        kRuntimeTaskPriority,
        nullptr,
        kRuntimeTaskPinnedCore);
#else
    created = xTaskCreate(
        ComputeCardRuntimeTask,
        "cc_runtime",
        kRuntimeTaskStackWords,
        nullptr,
        kRuntimeTaskPriority,
        nullptr);
#endif

    if (created != pdPASS)
    {
        ESP_LOGE(kLogTag, "Failed to create ComputeCard runtime task");
        return;
    }
}