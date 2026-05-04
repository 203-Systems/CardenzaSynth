ccos_collect_computecardsim_device_audio_cpp_sources(${PROJECT_SOURCE_DIR} computecard_sim_device_audio_sources)
target_sources(ComputeCardDeviceAudio PRIVATE
    ${computecard_sim_device_audio_sources}
)

if(WIN32)
    target_link_libraries(ComputeCardDeviceAudio PUBLIC
        winmm
    )
endif()

add_library(ComputeCardSimDevice)

ccos_collect_computecardsim_device_cpp_sources(${PROJECT_SOURCE_DIR} computecard_sim_device_sources)
target_sources(ComputeCardSimDevice PRIVATE
    ${computecard_sim_device_sources}
)

target_include_directories(ComputeCardSimDevice PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(ComputeCardSimDevice PUBLIC
    ComputeCardDeviceInterface
)

add_executable(ComputeCardSimHost)

ccos_collect_computecardsim_host_cpp_sources(${PROJECT_SOURCE_DIR} computecard_sim_host_sources)
target_sources(ComputeCardSimHost PRIVATE
    ${computecard_sim_host_sources}
)

target_include_directories(ComputeCardSimHost PRIVATE
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(ComputeCardSimHost PRIVATE
    ComputeCardSimDevice
    ComputeCardSystem
)

if(WIN32)
    add_executable(ComputeCardSimDesktop WIN32)

    ccos_collect_computecardsim_desktop_cpp_sources(${PROJECT_SOURCE_DIR} computecard_sim_desktop_sources)
    target_sources(ComputeCardSimDesktop PRIVATE
        ${computecard_sim_desktop_sources}
    )

    target_include_directories(ComputeCardSimDesktop PRIVATE
        ${PROJECT_SOURCE_DIR}
    )

    target_link_libraries(ComputeCardSimDesktop PRIVATE
        ComputeCardSimDevice
        ComputeCardSystem
        gdi32
        user32
    )
endif()
