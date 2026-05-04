target_sources(ComputeCardDeviceAudio PRIVATE
    DeviceAudio.cpp
)

if(WIN32)
    target_link_libraries(ComputeCardDeviceAudio PUBLIC
        winmm
    )
endif()

add_library(ComputeCardSimDevice
    Device.cpp
)

target_include_directories(ComputeCardSimDevice PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(ComputeCardSimDevice PUBLIC
    ComputeCardDeviceInterface
)

add_executable(ComputeCardSimHost
    main.cpp
)

target_include_directories(ComputeCardSimHost PRIVATE
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(ComputeCardSimHost PRIVATE
    ComputeCardSimDevice
    ComputeCardSystem
)

if(WIN32)
    add_executable(ComputeCardSimDesktop WIN32
        DesktopHost.cpp
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
