set(AMY_SRC_DIR ${PROJECT_SOURCE_DIR}/Library/third_party/amy/src)

add_library(ComputeCardAmy)

ccos_collect_amy_c_sources(${PROJECT_SOURCE_DIR} computecard_amy_sources)
target_sources(ComputeCardAmy PRIVATE
    ${computecard_amy_sources}
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

add_library(ComputeCardSynthRuntime)

ccos_collect_synth_runtime_cpp_sources(${PROJECT_SOURCE_DIR} computecard_synth_runtime_sources)
target_sources(ComputeCardSynthRuntime PRIVATE
    ${computecard_synth_runtime_sources}
)

target_include_directories(ComputeCardSynthRuntime PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(ComputeCardSynthRuntime PUBLIC
    ComputeCardAmy
)

add_library(ComputeCardSynth)

ccos_collect_synth_core_cpp_sources(${PROJECT_SOURCE_DIR} computecard_synth_core_sources)
target_sources(ComputeCardSynth PRIVATE
    ${computecard_synth_core_sources}
)

target_include_directories(ComputeCardSynth PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(ComputeCardSynth PUBLIC
    ComputeCardDeviceAudio
    ComputeCardMIDI
    ComputeCardSynthRuntime
)
