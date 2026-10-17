cardenza_collect_sim_device_audio_cpp_sources(${PROJECT_SOURCE_DIR} cardenza_sim_device_audio_sources)
target_sources(CardenzaDeviceAudio PRIVATE
    ${cardenza_sim_device_audio_sources}
)

if(WIN32)
    target_link_libraries(CardenzaDeviceAudio PUBLIC
        winmm
    )
endif()

add_library(CardenzaSimDevice)

cardenza_collect_sim_device_cpp_sources(${PROJECT_SOURCE_DIR} cardenza_sim_device_sources)
target_sources(CardenzaSimDevice PRIVATE
    ${cardenza_sim_device_sources}
)

target_include_directories(CardenzaSimDevice PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(CardenzaSimDevice PUBLIC
    CardenzaDeviceInterface
)

add_executable(CardenzaSimHost)

cardenza_collect_sim_host_cpp_sources(${PROJECT_SOURCE_DIR} cardenza_sim_host_sources)
target_sources(CardenzaSimHost PRIVATE
    ${cardenza_sim_host_sources}
)

target_include_directories(CardenzaSimHost PRIVATE
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(CardenzaSimHost PRIVATE
    CardenzaSimDevice
    CardenzaSystem
)

if(WIN32)
    add_executable(CardenzaSimDesktop WIN32)

    cardenza_collect_sim_desktop_cpp_sources(${PROJECT_SOURCE_DIR} cardenza_sim_desktop_sources)
    target_sources(CardenzaSimDesktop PRIVATE
        ${cardenza_sim_desktop_sources}
    )

    target_include_directories(CardenzaSimDesktop PRIVATE
        ${PROJECT_SOURCE_DIR}
    )

    target_link_libraries(CardenzaSimDesktop PRIVATE
        CardenzaSimDevice
        CardenzaSystem
        gdi32
        user32
    )
endif()
