add_subdirectory(Framework)
add_subdirectory(MIDI)
add_subdirectory(Storage)
add_subdirectory(System)
add_subdirectory(UI)

add_library(CardenzaSynthCore INTERFACE)
target_include_directories(CardenzaSynthCore INTERFACE
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(CardenzaSynthCore INTERFACE
    CardenzaMidiFramework
    CardenzaMIDI
    CardenzaStorage
    CardenzaUI
)
