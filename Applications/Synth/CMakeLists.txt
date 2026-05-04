add_library(ComputeCardSynthApp
    SynthApp.cpp
)

target_include_directories(ComputeCardSynthApp PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(ComputeCardSynthApp PUBLIC
    ComputeCardAppInterface
    ComputeCardSynth
)
