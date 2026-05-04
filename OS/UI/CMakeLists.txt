add_library(ComputeCardUI)

ccos_collect_ui_cpp_sources(${PROJECT_SOURCE_DIR} computecard_ui_sources)
target_sources(ComputeCardUI PRIVATE
    ${computecard_ui_sources}
)

target_include_directories(ComputeCardUI PUBLIC
    ${PROJECT_SOURCE_DIR}
)
