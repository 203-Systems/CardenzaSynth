add_library(ComputeCardMidiFramework)

ccos_collect_midi_framework_cpp_sources(${PROJECT_SOURCE_DIR} computecard_midi_framework_sources)
target_sources(ComputeCardMidiFramework PRIVATE
    ${computecard_midi_framework_sources}
)

target_include_directories(ComputeCardMidiFramework PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_compile_features(ComputeCardMidiFramework PUBLIC cxx_std_17)
