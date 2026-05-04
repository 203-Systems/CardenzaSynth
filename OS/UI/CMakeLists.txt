add_library(ComputeCardUI
    BuiltInFonts.cpp
    Canvas.cpp
    UI.cpp
)

target_include_directories(ComputeCardUI PUBLIC
    ${PROJECT_SOURCE_DIR}
)
