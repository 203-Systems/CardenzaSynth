add_library(ComputeCardMidiFramework
    MidiPacket.cpp
    MidiPort.cpp
)

target_include_directories(ComputeCardMidiFramework PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_compile_features(ComputeCardMidiFramework PUBLIC cxx_std_17)
