add_subdirectory(main/hal)

ccos_collect_computecard_device_audio_cpp_sources(${PROJECT_SOURCE_DIR} computecard_device_audio_sources)
target_sources(ComputeCardDeviceAudio PRIVATE
    ${computecard_device_audio_sources}
)

target_link_libraries(ComputeCardDeviceAudio PUBLIC
    ComputeCardHAL
)

add_library(ComputeCardDevice)

ccos_collect_computecard_device_cpp_sources(${PROJECT_SOURCE_DIR} computecard_device_sources)
target_sources(ComputeCardDevice PRIVATE
    ${computecard_device_sources}
)

target_include_directories(ComputeCardDevice PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(ComputeCardDevice PUBLIC
    ComputeCardDeviceInterface
    ComputeCardHAL
)
