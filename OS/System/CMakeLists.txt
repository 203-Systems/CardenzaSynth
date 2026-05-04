add_library(ComputeCardSystem
    System.cpp
)

target_include_directories(ComputeCardSystem PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(ComputeCardSystem PUBLIC
    ComputeCardApplications
    ComputeCardDeviceInterface
    ComputeCardOSCore
)