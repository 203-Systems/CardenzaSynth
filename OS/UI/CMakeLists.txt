add_library(CardenzaUI)

cardenza_collect_ui_cpp_sources(${PROJECT_SOURCE_DIR} cardenza_ui_sources)
target_sources(CardenzaUI PRIVATE
    ${cardenza_ui_sources}
)

target_include_directories(CardenzaUI PUBLIC
    ${PROJECT_SOURCE_DIR}
)
