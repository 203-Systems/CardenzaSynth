cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED DEVICE)
  message(FATAL_ERROR "You must specify -DDEVICE=<device>")
endif()

set(FAMILY_PATH "${CMAKE_SOURCE_DIR}/Devices/${DEVICE}")
if(NOT EXISTS "${FAMILY_PATH}")
  message(FATAL_ERROR "Unknown device family: ${DEVICE}")
endif()

project(ComputeCardOS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(ComputeCardDeviceAudio)
target_include_directories(ComputeCardDeviceAudio PUBLIC
  ${PROJECT_SOURCE_DIR}
)

include("${FAMILY_PATH}/Config.cmake")

add_subdirectory(OS)
add_subdirectory(Library)
add_subdirectory(Applications)
add_subdirectory(Devices)
