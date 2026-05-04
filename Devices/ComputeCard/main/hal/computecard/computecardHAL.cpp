#include "Devices/ComputeCard/main/hal/computecard/computecardHAL.h"

#if __has_include(<esp_log.h>)
#include <esp_log.h>
#endif

namespace {

ComputeCardHAL::InitReport g_initReport;
bool g_initialized = false;
bool g_keyboardInitialized = false;

#if __has_include(<esp_log.h>)
constexpr char kLogTag[] = "ComputeCardHAL";
#endif

esp_err_t KeepFirstError(esp_err_t current, esp_err_t next)
{
    return current == ESP_OK ? next : current;
}

void LogModuleResult(const char* moduleName, esp_err_t status, bool optional)
{
#if __has_include(<esp_log.h>)
    if (status == ESP_OK)
    {
        ESP_LOGI(kLogTag, "%s initialized", moduleName);
        return;
    }

    if (optional)
    {
        ESP_LOGW(kLogTag, "%s unavailable: %s", moduleName, esp_err_to_name(status));
    }
    else
    {
        ESP_LOGE(kLogTag, "%s failed: %s", moduleName, esp_err_to_name(status));
    }
#else
    (void)moduleName;
    (void)status;
    (void)optional;
#endif
}

}  // namespace

namespace ComputeCardHAL {

esp_err_t Init()
{
    if (g_initialized)
    {
        return g_initReport.overall;
    }

    g_initReport = {};

    g_initReport.system = System::Init();
    LogModuleResult("system", g_initReport.system, false);
    if (g_initReport.system != ESP_OK)
    {
        g_initReport.overall = g_initReport.system;
        return g_initReport.overall;
    }

    if (!g_keyboardInitialized)
    {
        Keyboard::init();
        g_keyboardInitialized = true;
#if __has_include(<esp_log.h>)
        ESP_LOGI(kLogTag, "keyboard initialized");
#endif
    }

    g_initReport.audio = Audio::Init();
    LogModuleResult("audio", g_initReport.audio, false);

    g_initReport.display = Display::Init();
    LogModuleResult("display", g_initReport.display, false);

    g_initReport.storage = Storage::Init();
    LogModuleResult("storage", g_initReport.storage, true);

    g_initReport.ir = IR::Init();
    LogModuleResult("ir", g_initReport.ir, true);

    esp_err_t firstError = ESP_OK;
    firstError = KeepFirstError(firstError, g_initReport.system);
    firstError = KeepFirstError(firstError, g_initReport.audio);
    firstError = KeepFirstError(firstError, g_initReport.display);
    g_initReport.overall = firstError;
    g_initialized = (g_initReport.overall == ESP_OK);
    return g_initReport.overall;
}

const InitReport& LastInitReport()
{
    return g_initReport;
}

}  // namespace ComputeCardHAL
