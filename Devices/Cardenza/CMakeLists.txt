cmake_minimum_required(VERSION 3.20)

get_filename_component(CARDENZA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(CARDENZA_FIRMWARE_BUILD_DIR "${CARDENZA_ROOT}/build/Cardenza")
set(CARDENZA_IDF_PROJECT_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(CARDENZA_POWERSHELL_EXE "$ENV{SystemRoot}/System32/WindowsPowerShell/v1.0/powershell.exe")
# The effects' per-sample kernels (Library/Synth/Audio/Fx) in IRAM instead of flash: no flash-cache
# misses while AMY competes for the 16 KB instruction cache, at the cost of internal RAM.
option(CARDENZA_FX_IRAM "Place the audio effects' per-sample kernels in IRAM" OFF)
# CardenzaHAL, an ESP-IDF component of its own: the submodule, or a HAL clone for a test build of a HAL
# change that is not upstream yet (make HAL_DEV=<clone>). The component is named after its directory.
if(DEFINED ENV{CARDENZA_HAL_DEV})
    get_filename_component(CARDENZA_HAL_DIR "$ENV{CARDENZA_HAL_DEV}" ABSOLUTE)
else()
    set(CARDENZA_HAL_DIR "${CMAKE_CURRENT_LIST_DIR}/cardenza")
endif()
get_filename_component(CARDENZA_HAL_COMPONENT "${CARDENZA_HAL_DIR}" NAME)

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
            ${AMY_SRC_DIR}
        REQUIRES
            ${CARDENZA_HAL_COMPONENT}
            driver
            esp_app_format
            esp_lcd
            esp_rom
            esp_timer
            fatfs
            freertos
            log
            nvs_flash
            pthread
            sdmmc
            vfs
    )

    target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
    # A crash goes into the log ring before ESP-IDF prints it (Devices/Cardenza/DeviceLog.cpp).
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_panic_handler")
    set_property(TARGET ${COMPONENT_LIB} PROPERTY C_STANDARD 11)
    set_property(TARGET ${COMPONENT_LIB} PROPERTY C_STANDARD_REQUIRED ON)

    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        AMY_WAVETABLE
        AMY_NO_MINIAUDIO
    )
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
elseif(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}" "${CARDENZA_HAL_DIR}")
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    # The component manager's lock (dependencies.lock, tracked) hashes the component names; a HAL
    # clone's differs from the submodule's, so a HAL_DEV build keeps its lock in the build tree.
    if(DEFINED ENV{CARDENZA_HAL_DEV})
        idf_build_set_property(DEPENDENCIES_LOCK "${CMAKE_BINARY_DIR}/dependencies.lock")
    endif()
    project(CardenzaSynthFirmware)
else()
    message(FATAL_ERROR "Cardenza is an ESP-IDF firmware target. Use the Devices/Cardenza IDF project or DEVICE=CardenzaSim for desktop builds.")
endif()
