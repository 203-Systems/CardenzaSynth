add_library(CardenzaStorage)

cardenza_collect_storage_cpp_sources(${PROJECT_SOURCE_DIR} cardenza_storage_sources)
target_sources(CardenzaStorage PRIVATE
    ${cardenza_storage_sources}
)

target_include_directories(CardenzaStorage PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(CardenzaStorage PUBLIC
    CardenzaDeviceInterface
)

if(MSVC)
    target_compile_definitions(CardenzaStorage PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
