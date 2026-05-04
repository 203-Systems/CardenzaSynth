add_library(ComputeCardAppInterface INTERFACE)
target_include_directories(ComputeCardAppInterface INTERFACE
    ${PROJECT_SOURCE_DIR}
)

add_library(ComputeCardApplications
    BuiltInApplications.cpp
)

target_include_directories(ComputeCardApplications PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(ComputeCardApplications PUBLIC
    ComputeCardAppInterface
)

function(add_enabled_applications config_file)
    if(NOT EXISTS "${config_file}")
        message(WARNING "Application list file not found at ${config_file}")
        return()
    endif()

    file(STRINGS "${config_file}" CONFIG_LINES)
    foreach(LINE ${CONFIG_LINES})
        string(STRIP "${LINE}" LINE_TRIMMED)
        if(LINE_TRIMMED STREQUAL "" OR LINE_TRIMMED MATCHES "^#")
            continue()
        endif()

        string(REGEX REPLACE "^\\[System\\]" "" APP_PATH "${LINE_TRIMMED}")

        if(APP_PATH MATCHES "\\{FAMILY_PATH\\}")
            string(REPLACE "{FAMILY_PATH}" "${FAMILY_PATH}" APP_DIR "${APP_PATH}")
            get_filename_component(APP_NAME "${APP_DIR}" NAME)
            add_subdirectory("${APP_DIR}" "${CMAKE_BINARY_DIR}/applications/${APP_NAME}")
        else()
            add_subdirectory("${APP_PATH}")
        endif()
    endforeach()
endfunction()

add_enabled_applications("${FAMILY_PATH}/ApplicationList.txt")

target_link_libraries(ComputeCardApplications PUBLIC
    ComputeCardSynthApp
)
