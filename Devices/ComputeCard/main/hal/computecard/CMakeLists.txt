add_library(ComputeCardHAL)

ccos_collect_computecard_hal_cpp_sources(${PROJECT_SOURCE_DIR} computecard_hal_sources)
target_sources(ComputeCardHAL PRIVATE
    ${computecard_hal_sources}
)

target_include_directories(ComputeCardHAL PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_compile_features(ComputeCardHAL PUBLIC cxx_std_17)
