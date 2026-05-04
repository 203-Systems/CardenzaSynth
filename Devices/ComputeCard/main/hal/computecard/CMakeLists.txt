add_library(ComputeCardHAL
    computecardHAL.cpp
    ../keyboard/keyboard.cpp
    audio/audio.cpp
    display/display.cpp
    ir/ir.cpp
    keyboard/keyboard.cpp
    mic/mic.cpp
    storage/storage.cpp
    system/system.cpp
)

target_include_directories(ComputeCardHAL PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_compile_features(ComputeCardHAL PUBLIC cxx_std_17)
