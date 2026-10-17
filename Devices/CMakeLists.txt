add_library(CardenzaDeviceInterface INTERFACE)
target_include_directories(CardenzaDeviceInterface INTERFACE
    ${PROJECT_SOURCE_DIR}
)

get_filename_component(DEVICE_DIR_NAME "${FAMILY_PATH}" NAME)
if(NOT EXISTS "${FAMILY_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "Device family is missing CMakeLists.txt: ${FAMILY_PATH}")
endif()

add_subdirectory("${DEVICE_DIR_NAME}")
