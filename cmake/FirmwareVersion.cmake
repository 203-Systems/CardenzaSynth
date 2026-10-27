include_guard(GLOBAL)

function(cardenza_read_firmware_version root_dir out_var)
    file(READ "${root_dir}/version.txt" version)
    string(STRIP "${version}" version)
    if(NOT version MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
        message(FATAL_ERROR "version.txt must contain a major.minor.patch firmware version")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${root_dir}/version.txt")
    set(${out_var} "${version}" PARENT_SCOPE)
endfunction()

function(cardenza_target_build_info target root_dir)
    set(info_dir "${CMAKE_CURRENT_BINARY_DIR}/build-info")
    set(info_header "${info_dir}/CardenzaBuildInfo.h")
    # Run for every build, including incremental builds after a commit or local edit.
    add_custom_target(${target}BuildInfo
        COMMAND ${CMAKE_COMMAND}
            "-DCARDENZA_ROOT=${root_dir}"
            "-DCARDENZA_BUILD_INFO_DIR=${info_dir}"
            -P "${root_dir}/cmake/GenerateBuildInfo.cmake"
        BYPRODUCTS "${info_header}"
        VERBATIM
    )
    add_dependencies(${target} ${target}BuildInfo)
    target_sources(${target} PRIVATE "${info_header}")
    target_include_directories(${target} PRIVATE "${info_dir}")
endfunction()
