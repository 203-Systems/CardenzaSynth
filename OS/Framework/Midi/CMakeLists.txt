add_library(CardenzaMidiFramework)

cardenza_collect_midi_framework_cpp_sources(${PROJECT_SOURCE_DIR} cardenza_midi_framework_sources)
target_sources(CardenzaMidiFramework PRIVATE
    ${cardenza_midi_framework_sources}
)

target_include_directories(CardenzaMidiFramework PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_compile_features(CardenzaMidiFramework PUBLIC cxx_std_17)
