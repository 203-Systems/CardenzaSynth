add_library(ComputeCardSynth
    Dx7SynthEngine.cpp
    JunoSynthEngine.cpp
    PagedSynthEngine.cpp
    SamplerSynthEngine.cpp
    SynthEngineFactory.cpp
    SynthRack.cpp
    SynthUiRenderer.cpp
    SynthUiModel.cpp
)

target_include_directories(ComputeCardSynth PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(ComputeCardSynth PUBLIC
    ComputeCardDeviceAudio
    ComputeCardMIDI
    ComputeCardUI
)
