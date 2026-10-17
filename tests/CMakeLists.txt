add_executable(CardenzaCoreTests
  core_test.cpp
  ${PROJECT_SOURCE_DIR}/Applications/Synth/Runtime/ProjectStorage.cpp
  ${PROJECT_SOURCE_DIR}/Applications/Synth/Runtime/Sequencer/SynthSequencer.cpp
  ${PROJECT_SOURCE_DIR}/OS/Framework/Midi/MidiPacket.cpp
)
target_include_directories(CardenzaCoreTests PRIVATE ${PROJECT_SOURCE_DIR})
add_test(NAME CardenzaCoreTests COMMAND CardenzaCoreTests)

add_executable(CardenzaSamplerTests
  sampler_test.cpp
  ${PROJECT_SOURCE_DIR}/Applications/Synth/Runtime/Sampler/SamplerManager.cpp
)
target_include_directories(CardenzaSamplerTests PRIVATE ${PROJECT_SOURCE_DIR})
add_test(NAME CardenzaSamplerTests COMMAND CardenzaSamplerTests)

add_executable(CardenzaUiTests
  ui_workflow_test.cpp
  ${PROJECT_SOURCE_DIR}/OS/UI/Canvas.cpp
  ${PROJECT_SOURCE_DIR}/OS/UI/BuiltInFonts.cpp
  ${PROJECT_SOURCE_DIR}/Applications/Synth/UI/Render/SynthUiRenderPrimitives.cpp
  ${PROJECT_SOURCE_DIR}/Applications/Synth/UI/SynthUiModel.cpp
  ${PROJECT_SOURCE_DIR}/Applications/Synth/UI/Menu/SynthUiMenu.cpp
  ${PROJECT_SOURCE_DIR}/Applications/Synth/UI/Dx7/Dx7UiModel.cpp
  ${PROJECT_SOURCE_DIR}/Applications/Synth/UI/Juno/JunoUiModel.cpp
  ${PROJECT_SOURCE_DIR}/Applications/Synth/UI/Sampler/SamplerUiModel.cpp
)
target_include_directories(CardenzaUiTests PRIVATE ${PROJECT_SOURCE_DIR})
add_test(NAME CardenzaUiTests COMMAND CardenzaUiTests)

cardenza_collect_synth_app_cpp_sources(${PROJECT_SOURCE_DIR} project_workflow_app_sources)
list(FILTER project_workflow_app_sources EXCLUDE REGEX "Renderer\\.cpp$|/UI/Render/")
cardenza_collect_synth_core_cpp_sources(${PROJECT_SOURCE_DIR} project_workflow_engine_sources)
cardenza_collect_midi_framework_cpp_sources(${PROJECT_SOURCE_DIR} project_workflow_midi_sources)
add_executable(CardenzaProjectTests
  project_workflow_test.cpp
  ${project_workflow_app_sources}
  ${project_workflow_engine_sources}
  ${project_workflow_midi_sources}
  ${PROJECT_SOURCE_DIR}/OS/MIDI/MIDI.cpp
)
target_include_directories(CardenzaProjectTests PRIVATE ${PROJECT_SOURCE_DIR})
add_test(NAME CardenzaProjectTests COMMAND CardenzaProjectTests)

foreach(target CardenzaCoreTests CardenzaSamplerTests CardenzaUiTests CardenzaProjectTests)
  if(MSVC)
    target_compile_options(${target} PRIVATE /UNDEBUG)
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
  else()
    target_compile_options(${target} PRIVATE -UNDEBUG)
    target_link_libraries(${target} PRIVATE pthread)
  endif()
endforeach()
