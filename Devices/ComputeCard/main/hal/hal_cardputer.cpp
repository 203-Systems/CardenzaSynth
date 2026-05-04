#include "hal_cardputer.h"

esp_err_t HAL::HalCardputer::init()
{
    return ComputeCardHAL::Init();
}

