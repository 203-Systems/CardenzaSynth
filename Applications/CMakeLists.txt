add_library(CardenzaAppInterface INTERFACE)
target_include_directories(CardenzaAppInterface INTERFACE
    ${PROJECT_SOURCE_DIR}
)

add_library(CardenzaApplications
    
)

cardenza_collect_builtin_application_cpp_sources(${PROJECT_SOURCE_DIR} cardenza_builtin_application_sources)
target_sources(CardenzaApplications PRIVATE
    ${cardenza_builtin_application_sources}
)

target_include_directories(CardenzaApplications PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(CardenzaApplications PUBLIC
    CardenzaAppInterface
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

target_link_libraries(CardenzaApplications PUBLIC
    CardenzaSynthApp
)
