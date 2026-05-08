cmake_minimum_required(VERSION 3.20)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    project(ComputeCardHAL LANGUAGES C CXX)
endif()

set(computecard_hal_sources
    ${CMAKE_CURRENT_LIST_DIR}/computecardHAL.cpp
    ${CMAKE_CURRENT_LIST_DIR}/audio/audio.cpp
    ${CMAKE_CURRENT_LIST_DIR}/audio/codec/es8156.cpp
    ${CMAKE_CURRENT_LIST_DIR}/display/display.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ir/ir.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mic/mic.cpp
    ${CMAKE_CURRENT_LIST_DIR}/system/system.cpp
)

set(computecard_hal_embedded_default OFF)
if(DEFINED IDF_TARGET OR DEFINED COMPONENT_DIR)
    set(computecard_hal_embedded_default ON)
endif()

option(COMPUTECARD_HAL_ENABLE_EMBEDDED_DRIVERS "Build ComputeCard HAL sources that require ESP-IDF drivers" ${computecard_hal_embedded_default})

if(NOT COMPUTECARD_HAL_ENABLE_EMBEDDED_DRIVERS)
    message(FATAL_ERROR "ComputeCardHAL is ESP-IDF only. Use DEVICE=ComputeCardSim for desktop host builds.")
endif()

list(APPEND computecard_hal_sources
    ${CMAKE_CURRENT_LIST_DIR}/keyboard/keyboard.cpp
    ${CMAKE_CURRENT_LIST_DIR}/keyboard/internal/keyboard_diagnostics.cpp
    ${CMAKE_CURRENT_LIST_DIR}/keyboard/internal/keyboard_hardware.cpp
    ${CMAKE_CURRENT_LIST_DIR}/keyboard/internal/keyboard_key_scan.cpp
    ${CMAKE_CURRENT_LIST_DIR}/keyboard/internal/keyboard_led_scan.cpp
    ${CMAKE_CURRENT_LIST_DIR}/storage/storage.cpp
)

add_library(ComputeCardHAL)

target_sources(ComputeCardHAL PRIVATE
    ${computecard_hal_sources}
)

target_include_directories(ComputeCardHAL PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_features(ComputeCardHAL PUBLIC cxx_std_17)
