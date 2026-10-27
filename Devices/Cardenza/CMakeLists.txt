cmake_minimum_required(VERSION 3.20)

get_filename_component(CARDENZA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
include(${CARDENZA_ROOT}/cmake/FirmwareVersion.cmake)
cardenza_read_firmware_version("${CARDENZA_ROOT}" PROJECT_VER)
set(CARDENZA_FIRMWARE_BUILD_DIR "${CARDENZA_ROOT}/build/Cardenza")
set(CARDENZA_IDF_PROJECT_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(CARDENZA_POWERSHELL_EXE "$ENV{SystemRoot}/System32/WindowsPowerShell/v1.0/powershell.exe")
option(CARDENZA_AMY_EXPERIMENTAL_MULTICORE "Enable experimental AMY ESP32 multicore rendering" OFF)
option(CARDENZA_ALLOW_LOCAL_HAL_CHANGES "Build explicitly user-approved local HAL edits" OFF)
# The effects' per-sample kernels (Library/Synth/Audio/Fx) in IRAM instead of flash: no flash-cache
# misses while AMY competes for the 16 KB instruction cache, at the cost of internal RAM.
option(CARDENZA_FX_IRAM "Place the audio effects' per-sample kernels in IRAM" OFF)

if(DEFINED COMPONENT_DIR)
    set(AMY_SRC_DIR ${CARDENZA_ROOT}/Library/third_party/amy/src)
    include(${CARDENZA_ROOT}/cmake/CardenzaSynthSourceLists.cmake)

    cardenza_collect_common_cpp_sources(${CARDENZA_ROOT} CARDENZA_COMMON_CPP_SOURCES)
    cardenza_collect_firmware_cpp_sources(${CARDENZA_ROOT} CARDENZA_FIRMWARE_CPP_SOURCES)
    cardenza_collect_amy_c_sources(${CARDENZA_ROOT} AMY_C_SOURCES)

    idf_component_register(
        SRCS
            ${CARDENZA_ROOT}/Devices/Cardenza/app_main.cpp
            ${CARDENZA_COMMON_CPP_SOURCES}
            ${CARDENZA_FIRMWARE_CPP_SOURCES}
            ${AMY_C_SOURCES}
        INCLUDE_DIRS
            ${CARDENZA_ROOT}
            ${CMAKE_CURRENT_LIST_DIR}/CardenzaHAL
            ${AMY_SRC_DIR}
        REQUIRES
            driver
            esp_lcd
            esp_rom
            esp_timer
            fatfs
            freertos
            log
            pthread
            sdmmc
            vfs
    )

    cardenza_target_build_info(${COMPONENT_LIB} "${CARDENZA_ROOT}")

    target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
    set_property(TARGET ${COMPONENT_LIB} PROPERTY C_STANDARD 11)
    set_property(TARGET ${COMPONENT_LIB} PROPERTY C_STANDARD_REQUIRED ON)

    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        AMY_WAVETABLE
        AMY_NO_MINIAUDIO
    )
    if(CARDENZA_AMY_EXPERIMENTAL_MULTICORE)
        target_compile_definitions(${COMPONENT_LIB} PRIVATE CARDENZA_AMY_EXPERIMENTAL_MULTICORE=1)
    endif()
    if(CARDENZA_FX_IRAM)
        target_compile_definitions(${COMPONENT_LIB} PRIVATE CARDENZA_FX_IRAM=1)
    endif()
    cardenza_groove_ui_enabled(CARDENZA_GROOVE_UI_ENABLED)
    if(CARDENZA_GROOVE_UI_ENABLED)
        target_compile_definitions(${COMPONENT_LIB} PRIVATE CARDENZA_GROOVE_UI=1)
    endif()

    target_compile_options(${COMPONENT_LIB} PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-Wno-unused-parameter>
        $<$<COMPILE_LANGUAGE:C>:-Wno-sign-compare>
        $<$<COMPILE_LANGUAGE:C>:-Wno-missing-field-initializers>
        $<$<COMPILE_LANGUAGE:C>:-Wno-float-conversion>
        $<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>
        $<$<COMPILE_LANGUAGE:C>:-Wno-pointer-to-int-cast>
    )

    # The effects DSP (cardenza_collect_synth_fx_cpp_sources) stays at the project's -O2: -O3 does
    # not shorten its per-sample loops on the ESP32-S3, it only grows the code competing for the
    # instruction cache.
    set(CARDENZA_AUDIO_HOT_SOURCES
        ${CARDENZA_ROOT}/Library/Synth/Audio/Amy/SynthAudioRuntimeAmy.cpp
        ${AMY_C_SOURCES}
    )
    set_source_files_properties(${CARDENZA_AUDIO_HOT_SOURCES} PROPERTIES COMPILE_OPTIONS "-O3")

    if(CONFIG_STDATOMIC_S32C1I_SPIRAM_WORKAROUND)
        # These audio translation units keep their atomics in internal RAM;
        # normal new/malloc also stay internal (SPIRAM_USE_CAPS_ALLOC). Preserve
        # inline hardware atomics for the ISR counters and render-thread state.
        # Leave the SDK's PSRAM-safe atomic implementation enabled elsewhere.
        set_property(SOURCE
            ${CARDENZA_ROOT}/Devices/Cardenza/DeviceAudio.cpp
            ${CARDENZA_ROOT}/Library/Synth/Audio/Amy/SynthAudioRuntimeAmy.cpp
            APPEND PROPERTY COMPILE_OPTIONS "-mno-disable-hardware-atomics")
    endif()
elseif(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}")
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(CardenzaSynthFirmware)
else()
    message(FATAL_ERROR "Cardenza is an ESP-IDF firmware target. Use the Devices/Cardenza IDF project or DEVICE=CardenzaSim for desktop builds.")
endif()
