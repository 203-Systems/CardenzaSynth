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

// Internal RAM is the scarce resource (PSRAM is reserved for samples), so log headroom at boot.
void LogInternalHeap(const char* stage)
{
    constexpr uint32_t kCaps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    ESP_LOGI(kLogTag,
             "heap %s: free=%u largest=%u min=%u",
             stage,
             static_cast<unsigned>(heap_caps_get_free_size(kCaps)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(kCaps)),
             static_cast<unsigned>(heap_caps_get_minimum_free_size(kCaps)));
}

void LogFailedAllocation(size_t size, uint32_t caps, const char* functionName)
{
    ESP_EARLY_LOGE(kLogTag, "alloc failed: %u bytes caps=0x%x in %s", static_cast<unsigned>(size),
                   static_cast<unsigned>(caps), functionName != nullptr ? functionName : "?");
}

void CardenzaRuntimeTask(void*)
{
    LogInternalHeap("before boot");
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
    LogInternalHeap("after boot");

    TickType_t lastWakeTime = xTaskGetTickCount();
    for (;;)
    {
        os::system::RunFrame();
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
