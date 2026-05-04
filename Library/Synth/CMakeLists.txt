set(AMY_SRC_DIR ${PROJECT_SOURCE_DIR}/third_party/amy/src)

add_library(ComputeCardAmy
    ${AMY_SRC_DIR}/algorithms.c
    ${AMY_SRC_DIR}/amy.c
    ${AMY_SRC_DIR}/api.c
    ${AMY_SRC_DIR}/custom.c
    ${AMY_SRC_DIR}/delay.c
    ${AMY_SRC_DIR}/envelope.c
    ${AMY_SRC_DIR}/examples.c
    ${AMY_SRC_DIR}/filters.c
    ${AMY_SRC_DIR}/instrument.c
    ${AMY_SRC_DIR}/interp_partials.c
    ${AMY_SRC_DIR}/log2_exp2.c
    ${AMY_SRC_DIR}/midi_mappings.c
    ${AMY_SRC_DIR}/oscillators.c
    ${AMY_SRC_DIR}/parse.c
    ${AMY_SRC_DIR}/patches.c
    ${AMY_SRC_DIR}/pcm.c
    ${AMY_SRC_DIR}/sequencer.c
    ${AMY_SRC_DIR}/transfer.c
    AmyEmbeddedStubs.c
)

target_include_directories(ComputeCardAmy PUBLIC
    ${AMY_SRC_DIR}
    ${PROJECT_SOURCE_DIR}
)

target_compile_definitions(ComputeCardAmy PUBLIC
    AMY_WAVETABLE
    AMY_NO_MINIAUDIO
    _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
    target_compile_options(ComputeCardAmy PRIVATE
        $<$<COMPILE_LANGUAGE:C>:/std:c11>
        $<$<COMPILE_LANGUAGE:C>:/wd4018>
        $<$<COMPILE_LANGUAGE:C>:/wd4100>
        $<$<COMPILE_LANGUAGE:C>:/wd4244>
        $<$<COMPILE_LANGUAGE:C>:/wd4267>
        $<$<COMPILE_LANGUAGE:C>:/wd4305>
        $<$<COMPILE_LANGUAGE:C>:/wd4311>
        $<$<COMPILE_LANGUAGE:C>:/wd4312>
        $<$<COMPILE_LANGUAGE:C>:/wd4996>
    )
else()
    target_compile_options(ComputeCardAmy PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-Wno-unused-parameter>
        $<$<COMPILE_LANGUAGE:C>:-Wno-sign-compare>
        $<$<COMPILE_LANGUAGE:C>:-Wno-missing-field-initializers>
        $<$<COMPILE_LANGUAGE:C>:-Wno-float-conversion>
    )
    target_link_libraries(ComputeCardAmy PUBLIC m pthread)
endif()

add_library(ComputeCardSynthRuntime
    SynthAudioRuntimeAmy.cpp
)

target_include_directories(ComputeCardSynthRuntime PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(ComputeCardSynthRuntime PUBLIC
    ComputeCardAmy
)

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
    ComputeCardSynthRuntime
    ComputeCardUI
)
