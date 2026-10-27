#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "Devices/Device.h"
#include "Devices/Cardenza/AudioDiagnostics.h"
#include "OS/System/System.h"

namespace {

constexpr char kLogTag[] = "Cardenza";
constexpr TickType_t kFrameTick = pdMS_TO_TICKS(16);
// ESP-IDF's task creation APIs take bytes, unlike upstream FreeRTOS.
constexpr uint32_t kRuntimeTaskStackBytes = 16384;
constexpr UBaseType_t kRuntimeTaskPriority = 5;
constexpr BaseType_t kRuntimeTaskPinnedCore = 0;

void LogHeap(const char* stage, const char* name, uint32_t caps)
{
    ESP_LOGI(kLogTag,
             "heap %s %s: total=%u free=%u largest=%u min=%u",
             stage, name,
             static_cast<unsigned>(heap_caps_get_total_size(caps)),
             static_cast<unsigned>(heap_caps_get_free_size(caps)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(caps)),
             static_cast<unsigned>(heap_caps_get_minimum_free_size(caps)));
}

void LogMemoryHeadroom(const char* stage)
{
    LogHeap(stage, "internal", MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    LogHeap(stage, "psram", MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void LogFailedAllocation(size_t size, uint32_t caps, const char* functionName)
{
    ESP_EARLY_LOGE(kLogTag, "alloc failed: %u bytes caps=0x%x in %s", static_cast<unsigned>(size),
                   static_cast<unsigned>(caps), functionName != nullptr ? functionName : "?");
}

void CardenzaRuntimeTask(void*)
{
    LogMemoryHeadroom("before boot");
    while (!Device::Init())
    {
        ESP_LOGE(kLogTag, "Hardware initialization failed; retrying in one second");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    if (!os::system::Begin())
    {
        ESP_LOGE(kLogTag, "Failed to boot Cardenza runtime");
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(kLogTag,
             "%s [%s] %dx%d",
             Device::Name().c_str(),
             Device::BackendName().c_str(),
             Device::Display::kWidth,
             Device::Display::kHeight);
    LogMemoryHeadroom("after boot");

    TickType_t lastWakeTime = xTaskGetTickCount();
    for (;;)
    {
        os::system::RunFrame();
        device::PollAudioRecovery();
        device::PollAudioDiagnostics();
        vTaskDelayUntil(&lastWakeTime, kFrameTick);
    }
}

}  // namespace

extern "C" void app_main(void)
{
    heap_caps_register_failed_alloc_callback(LogFailedAllocation);
    BaseType_t created = pdFAIL;
#if portNUM_PROCESSORS > 1
    created = xTaskCreatePinnedToCore(
        CardenzaRuntimeTask,
        "cc_runtime",
        kRuntimeTaskStackBytes,
        nullptr,
        kRuntimeTaskPriority,
        nullptr,
        kRuntimeTaskPinnedCore);
#else
    created = xTaskCreate(
        CardenzaRuntimeTask,
        "cc_runtime",
        kRuntimeTaskStackBytes,
        nullptr,
        kRuntimeTaskPriority,
        nullptr);
#endif

    if (created != pdPASS)
    {
        ESP_LOGE(kLogTag, "Failed to create Cardenza runtime task");
        return;
    }
}
