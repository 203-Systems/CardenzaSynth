add_subdirectory(main/hal)

target_sources(ComputeCardDeviceAudio PRIVATE
    DeviceAudio.cpp
)

target_link_libraries(ComputeCardDeviceAudio PUBLIC
    ComputeCardHAL
)

add_library(ComputeCardDevice
    Device.cpp
)

target_include_directories(ComputeCardDevice PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(ComputeCardDevice PUBLIC
    ComputeCardDeviceInterface
    ComputeCardHAL
)
