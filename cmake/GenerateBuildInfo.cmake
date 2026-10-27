if(NOT DEFINED CARDENZA_ROOT OR NOT DEFINED CARDENZA_BUILD_INFO_DIR)
    message(FATAL_ERROR "CARDENZA_ROOT and CARDENZA_BUILD_INFO_DIR are required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/FirmwareVersion.cmake")
cardenza_read_firmware_version("${CARDENZA_ROOT}" CARDENZA_VERSION)

set(CARDENZA_BUILD_HASH "unknown")
set(CARDENZA_BUILD_DIRTY false)
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CARDENZA_ROOT}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${CARDENZA_ROOT}" rev-parse --short=8 HEAD
        RESULT_VARIABLE hash_result
        OUTPUT_VARIABLE CARDENZA_BUILD_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${CARDENZA_ROOT}" status --porcelain=v1
                --untracked-files=normal --ignore-submodules=none
        RESULT_VARIABLE status_result
        OUTPUT_VARIABLE git_status
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT hash_result EQUAL 0 OR NOT status_result EQUAL 0)
        message(FATAL_ERROR "Cannot read firmware Git build information")
    endif()
    if(NOT git_status STREQUAL "")
        set(CARDENZA_BUILD_DIRTY true)
    endif()
endif()

string(TIMESTAMP CARDENZA_BUILD_TIME "%Y-%m-%d %H:%M:%S" UTC)
file(MAKE_DIRECTORY "${CARDENZA_BUILD_INFO_DIR}")
configure_file("${CMAKE_CURRENT_LIST_DIR}/FirmwareBuildInfo.h.in"
               "${CARDENZA_BUILD_INFO_DIR}/CardenzaBuildInfo.h" @ONLY)
