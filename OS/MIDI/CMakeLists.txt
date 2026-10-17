add_library(CardenzaMIDI)

cardenza_collect_midi_cpp_sources(${PROJECT_SOURCE_DIR} cardenza_midi_sources)
target_sources(CardenzaMIDI PRIVATE
    ${cardenza_midi_sources}
)

target_include_directories(CardenzaMIDI PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(CardenzaMIDI PUBLIC
    CardenzaMidiFramework
)
