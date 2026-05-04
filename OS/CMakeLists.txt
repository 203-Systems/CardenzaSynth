add_subdirectory(Framework)
add_subdirectory(MIDI)
add_subdirectory(UI)

add_library(ComputeCardOSCore INTERFACE)
target_include_directories(ComputeCardOSCore INTERFACE
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(ComputeCardOSCore INTERFACE
    ComputeCardMidiFramework
    ComputeCardMIDI
    ComputeCardUI
)
