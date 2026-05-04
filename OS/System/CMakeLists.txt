add_library(ComputeCardSystem)

ccos_collect_system_cpp_sources(${PROJECT_SOURCE_DIR} computecard_system_sources)
target_sources(ComputeCardSystem PRIVATE
    ${computecard_system_sources}
)

target_include_directories(ComputeCardSystem PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(ComputeCardSystem PUBLIC
    ComputeCardApplications
    ComputeCardDeviceInterface
    ComputeCardOSCore
)