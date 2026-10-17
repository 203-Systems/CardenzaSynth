add_library(CardenzaSynthApp)

cardenza_collect_synth_app_cpp_sources(${PROJECT_SOURCE_DIR} cardenza_synth_app_sources)
target_sources(CardenzaSynthApp PRIVATE
    ${cardenza_synth_app_sources}
)

target_include_directories(CardenzaSynthApp PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(CardenzaSynthApp PUBLIC
    CardenzaAppInterface
    CardenzaSynth
    CardenzaUI
)
