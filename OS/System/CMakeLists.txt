add_library(CardenzaSystem)

cardenza_collect_system_cpp_sources(${PROJECT_SOURCE_DIR} cardenza_system_sources)
target_sources(CardenzaSystem PRIVATE
    ${cardenza_system_sources}
)

target_include_directories(CardenzaSystem PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(CardenzaSystem PUBLIC
    CardenzaApplications
    CardenzaDeviceInterface
    CardenzaSynthCore
)