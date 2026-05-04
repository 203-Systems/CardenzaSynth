add_library(ComputeCardSynthApp)

ccos_collect_synth_app_cpp_sources(${PROJECT_SOURCE_DIR} computecard_synth_app_sources)
target_sources(ComputeCardSynthApp PRIVATE
    ${computecard_synth_app_sources}
)

target_include_directories(ComputeCardSynthApp PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(ComputeCardSynthApp PUBLIC
    ComputeCardAppInterface
    ComputeCardSynth
)
