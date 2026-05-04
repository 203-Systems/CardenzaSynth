add_library(ComputeCardMIDI)

ccos_collect_midi_cpp_sources(${PROJECT_SOURCE_DIR} computecard_midi_sources)
target_sources(ComputeCardMIDI PRIVATE
    ${computecard_midi_sources}
)

target_include_directories(ComputeCardMIDI PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(ComputeCardMIDI PUBLIC
    ComputeCardMidiFramework
)
