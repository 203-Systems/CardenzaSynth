add_library(ComputeCardMIDI
    MIDI.cpp
)

target_include_directories(ComputeCardMIDI PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(ComputeCardMIDI PUBLIC
    ComputeCardMidiFramework
)
