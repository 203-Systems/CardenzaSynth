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

set(cardenza_ui_tests)
if(NOT CARDENZA_GROOVE_UI)
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

add_executable(CardenzaSequencerRenderTests sequencer_render_test.cpp)
target_link_libraries(CardenzaSequencerRenderTests PRIVATE CardenzaSystem CardenzaSimDevice)
add_test(NAME CardenzaSequencerRenderTests COMMAND CardenzaSequencerRenderTests)
set(cardenza_ui_tests CardenzaUiTests CardenzaProjectTests CardenzaSequencerRenderTests)
endif()

if(CARDENZA_GROOVE_UI)
  add_executable(CardenzaGrooveUiTests groove_ui_test.cpp)
  target_link_libraries(CardenzaGrooveUiTests PRIVATE CardenzaSystem CardenzaSimDevice)
  add_test(NAME CardenzaGrooveUiTests COMMAND CardenzaGrooveUiTests)
  list(APPEND cardenza_ui_tests CardenzaGrooveUiTests)
endif()

add_executable(CardenzaLedTests led_feedback_test.cpp)
target_include_directories(CardenzaLedTests PRIVATE ${PROJECT_SOURCE_DIR})
add_test(NAME CardenzaLedTests COMMAND CardenzaLedTests)

# Headless screenshot runner: replay a real script against a scratch sdcard, and reject an
# unknown key target with its line number.
add_test(NAME CardenzaSimShotSmoke
  COMMAND CardenzaSimShot --script ${CMAKE_CURRENT_SOURCE_DIR}/groove/tour.script
          --out ${CMAKE_CURRENT_BINARY_DIR}/shot-smoke --root ${CMAKE_CURRENT_BINARY_DIR}/shot-smoke-root --scale 2
)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/shot-unknown-key.script "wait 1\ntap nosuchkey\n")
add_test(NAME CardenzaSimShotRejectsUnknownKey
  COMMAND CardenzaSimShot --script ${CMAKE_CURRENT_BINARY_DIR}/shot-unknown-key.script
          --out ${CMAKE_CURRENT_BINARY_DIR}/shot-unknown-key
)
set_tests_properties(CardenzaSimShotRejectsUnknownKey PROPERTIES
  PASS_REGULAR_EXPRESSION "shot-unknown-key\\.script:2: unknown key target 'nosuchkey'"
)

foreach(target CardenzaCoreTests CardenzaSamplerTests ${cardenza_ui_tests} CardenzaLedTests)
  if(MSVC)
    target_compile_options(${target} PRIVATE /UNDEBUG)
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
  else()
    target_compile_options(${target} PRIVATE -UNDEBUG)
    target_link_libraries(${target} PRIVATE pthread)
  endif()
endforeach()
