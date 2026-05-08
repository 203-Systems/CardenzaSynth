if(NOT DEFINED CCOS_ROOT)
    get_filename_component(CCOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

function(ccos_require_file relative_path)
    if(NOT EXISTS "${CCOS_ROOT}/${relative_path}")
        message(FATAL_ERROR "Required architecture file is missing: ${relative_path}")
    endif()
endfunction()

function(ccos_forbid_path relative_path)
    if(EXISTS "${CCOS_ROOT}/${relative_path}")
        message(FATAL_ERROR "Forbidden architecture path still exists: ${relative_path}")
    endif()
endfunction()

function(ccos_require_text relative_path expected_text)
    ccos_require_file("${relative_path}")
    file(READ "${CCOS_ROOT}/${relative_path}" file_content)
    string(FIND "${file_content}" "${expected_text}" match_index)
    if(match_index EQUAL -1)
        message(FATAL_ERROR "${relative_path} does not contain expected architecture marker: ${expected_text}")
    endif()
endfunction()

function(ccos_forbid_text relative_path forbidden_text)
    ccos_require_file("${relative_path}")
    file(READ "${CCOS_ROOT}/${relative_path}" file_content)
    string(FIND "${file_content}" "${forbidden_text}" match_index)
    if(NOT match_index EQUAL -1)
        message(FATAL_ERROR "${relative_path} contains forbidden architecture coupling: ${forbidden_text}")
    endif()
endfunction()

ccos_require_file("docs/architecture.md")
ccos_forbid_path("Devices/ComputeCardSim/WebUI")

set(computecard_firmware_only_sources
    "Devices/ComputeCard/computecard/computecardHAL.cpp"
    "Devices/ComputeCard/computecard/audio/audio.cpp"
    "Devices/ComputeCard/computecard/audio/codec/es8156.cpp"
    "Devices/ComputeCard/computecard/display/display.cpp"
    "Devices/ComputeCard/computecard/ir/ir.cpp"
    "Devices/ComputeCard/computecard/keyboard/internal/keyboard_hardware.h"
    "Devices/ComputeCard/computecard/mic/mic.cpp"
    "Devices/ComputeCard/computecard/storage/storage.cpp"
    "Devices/ComputeCard/computecard/system/system.cpp"
)

foreach(relative_path IN LISTS computecard_firmware_only_sources)
    ccos_require_text("${relative_path}" "ESP-IDF only")
endforeach()

ccos_require_file("Devices/ComputeCard/computecard/keyboard/types.h")
ccos_require_file("Devices/ComputeCard/computecard/keyboard/layout.h")
ccos_require_file("Devices/ComputeCard/computecard/keyboard/internal/keyboard_key_scan.cpp")
ccos_require_file("Devices/ComputeCard/computecard/keyboard/internal/keyboard_led_scan.cpp")
ccos_require_file("Devices/ComputeCardSim/DesktopHost/DesktopSimulatorInput.cpp")
ccos_require_file("Devices/ComputeCardSim/DesktopHost/DesktopSimulatorRender.cpp")

ccos_require_text("cmake/ComputeCardOSSourceLists.cmake" "keyboard/internal/keyboard_key_scan.cpp")
ccos_require_text("cmake/ComputeCardOSSourceLists.cmake" "keyboard/internal/keyboard_led_scan.cpp")
ccos_require_text("cmake/ComputeCardOSSourceLists.cmake" "DesktopHost/DesktopSimulatorInput.cpp")
ccos_require_text("cmake/ComputeCardOSSourceLists.cmake" "DesktopHost/DesktopSimulatorRender.cpp")

ccos_forbid_text("Devices/ComputeCardSim/DesktopHost.cpp" "PressMatrixKey")
ccos_forbid_text("Devices/ComputeCardSim/DesktopHost.cpp" "ReleaseMatrixKey")
ccos_forbid_text("Devices/ComputeCardSim/DesktopHost/DesktopSimulatorInput.cpp" "SynthApp")

file(READ "${CCOS_ROOT}/Devices/ComputeCard/CMakeLists.txt" computecard_cmake)
if(computecard_cmake MATCHES "add_subdirectory\\(computecard\\)")
    message(FATAL_ERROR "ComputeCard root CMake path must not expose the ESP-IDF HAL as a host subdirectory")
endif()

message(STATUS "ComputeCardOS architecture guardrails passed")
