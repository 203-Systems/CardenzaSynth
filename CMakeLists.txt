cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED DEVICE)
  message(FATAL_ERROR "You must specify -DDEVICE=<device>")
endif()

set(FAMILY_PATH "${CMAKE_SOURCE_DIR}/Devices/${DEVICE}")
if(NOT EXISTS "${FAMILY_PATH}")
  message(FATAL_ERROR "Unknown device family: ${DEVICE}")
endif()

project(ComputeCardOS LANGUAGES C CXX)

include(${CMAKE_SOURCE_DIR}/cmake/ComputeCardOSSourceLists.cmake)

add_custom_target(ValidateArchitecture
  COMMAND ${CMAKE_COMMAND} -DCCOS_ROOT=${CMAKE_SOURCE_DIR} -P ${CMAKE_SOURCE_DIR}/cmake/ValidateArchitecture.cmake
  VERBATIM
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
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
target_link_libraries(ComputeCardDeviceAudio PUBLIC
  ComputeCardSynthRuntime
)
add_subdirectory(Applications)
add_subdirectory(Devices)
