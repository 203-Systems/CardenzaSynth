set(AMY_SRC_DIR ${PROJECT_SOURCE_DIR}/Library/third_party/amy/src)

add_library(CardenzaAmy)

cardenza_collect_amy_c_sources(${PROJECT_SOURCE_DIR} cardenza_amy_sources)
target_sources(CardenzaAmy PRIVATE
    ${cardenza_amy_sources}
)

target_include_directories(CardenzaAmy PUBLIC
    ${AMY_SRC_DIR}
    ${PROJECT_SOURCE_DIR}
)

target_compile_definitions(CardenzaAmy PUBLIC
    AMY_WAVETABLE
    AMY_NO_MINIAUDIO
    _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
    target_compile_options(CardenzaAmy PRIVATE
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
    target_compile_options(CardenzaAmy PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-Wno-unused-parameter>
        $<$<COMPILE_LANGUAGE:C>:-Wno-sign-compare>
        $<$<COMPILE_LANGUAGE:C>:-Wno-missing-field-initializers>
        $<$<COMPILE_LANGUAGE:C>:-Wno-float-conversion>
    )
    target_link_libraries(CardenzaAmy PUBLIC m pthread)
endif()

add_library(CardenzaSynthRuntime)

cardenza_collect_synth_runtime_cpp_sources(${PROJECT_SOURCE_DIR} cardenza_synth_runtime_sources)
target_sources(CardenzaSynthRuntime PRIVATE
    ${cardenza_synth_runtime_sources}
)

target_include_directories(CardenzaSynthRuntime PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(CardenzaSynthRuntime PUBLIC
    CardenzaAmy
)

add_library(CardenzaSynth)

cardenza_collect_synth_core_cpp_sources(${PROJECT_SOURCE_DIR} cardenza_synth_core_sources)
target_sources(CardenzaSynth PRIVATE
    ${cardenza_synth_core_sources}
)

target_include_directories(CardenzaSynth PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(CardenzaSynth PUBLIC
    CardenzaDeviceAudio
    CardenzaMIDI
    CardenzaSynthRuntime
)
