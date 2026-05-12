cmake_minimum_required(VERSION 3.20)

get_filename_component(CCOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(CCOS_COMPUTECARD_FIRMWARE_BUILD_DIR "${CCOS_ROOT}/build/ComputeCard")
set(CCOS_COMPUTECARD_IDF_PROJECT_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(CCOS_COMPUTECARD_POWERSHELL_EXE "$ENV{SystemRoot}/System32/WindowsPowerShell/v1.0/powershell.exe")
option(CCOS_AMY_EXPERIMENTAL_MULTICORE "Enable experimental AMY ESP32 multicore rendering" OFF)

if(DEFINED COMPONENT_DIR)
    set(AMY_SRC_DIR ${CCOS_ROOT}/Library/third_party/amy/src)
    include(${CCOS_ROOT}/cmake/ComputeCardOSSourceLists.cmake)

    ccos_collect_common_cpp_sources(${CCOS_ROOT} CCOS_COMMON_CPP_SOURCES)
    ccos_collect_computecard_firmware_cpp_sources(${CCOS_ROOT} CCOS_COMPUTECARD_FIRMWARE_CPP_SOURCES)
    ccos_collect_amy_c_sources(${CCOS_ROOT} AMY_C_SOURCES)

    idf_component_register(
        SRCS
            ${CCOS_ROOT}/Devices/ComputeCard/app_main.cpp
            ${CCOS_COMMON_CPP_SOURCES}
            ${CCOS_COMPUTECARD_FIRMWARE_CPP_SOURCES}
            ${AMY_C_SOURCES}
        INCLUDE_DIRS
            ${CCOS_ROOT}
            ${CMAKE_CURRENT_LIST_DIR}/computecard
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

    target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
    set_property(TARGET ${COMPONENT_LIB} PROPERTY C_STANDARD 11)
    set_property(TARGET ${COMPONENT_LIB} PROPERTY C_STANDARD_REQUIRED ON)

    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        AMY_WAVETABLE
        AMY_NO_MINIAUDIO
    )
    if(CCOS_AMY_EXPERIMENTAL_MULTICORE)
        target_compile_definitions(${COMPONENT_LIB} PRIVATE CCOS_AMY_EXPERIMENTAL_MULTICORE=1)
    endif()

    target_compile_options(${COMPONENT_LIB} PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-Wno-unused-parameter>
        $<$<COMPILE_LANGUAGE:C>:-Wno-sign-compare>
        $<$<COMPILE_LANGUAGE:C>:-Wno-missing-field-initializers>
        $<$<COMPILE_LANGUAGE:C>:-Wno-float-conversion>
        $<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>
        $<$<COMPILE_LANGUAGE:C>:-Wno-pointer-to-int-cast>
    )

    set(CCOS_AUDIO_HOT_SOURCES
        ${CCOS_ROOT}/Library/Synth/Audio/Amy/SynthAudioRuntimeAmy.cpp
        ${AMY_C_SOURCES}
    )
    set_source_files_properties(${CCOS_AUDIO_HOT_SOURCES} PROPERTIES COMPILE_OPTIONS "-O3")
elseif(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}")
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(ComputeCardOSFirmware)
else()
    message(FATAL_ERROR "ComputeCard is an ESP-IDF firmware target. Use the Devices/ComputeCard IDF project or DEVICE=ComputeCardSim for desktop builds.")
endif()
