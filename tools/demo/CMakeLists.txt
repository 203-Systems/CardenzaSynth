cmake_minimum_required(VERSION 3.20)
project(CardenzaDemo LANGUAGES C CXX)
get_filename_component(CARDENZA_ROOT ../.. ABSOLUTE)
include(${CARDENZA_ROOT}/cmake/CardenzaSynthSourceLists.cmake)
cardenza_collect_amy_c_sources(${CARDENZA_ROOT} amy_sources)
cardenza_collect_synth_core_cpp_sources(${CARDENZA_ROOT} synth_sources)
cardenza_collect_midi_framework_cpp_sources(${CARDENZA_ROOT} midi_sources)
cardenza_collect_synth_fx_cpp_sources(${CARDENZA_ROOT} fx_sources)
add_library(DemoAmy STATIC ${amy_sources})
target_include_directories(DemoAmy PUBLIC ${CARDENZA_ROOT}/Library/third_party/amy/src ${CARDENZA_ROOT})
target_compile_definitions(DemoAmy PUBLIC AMY_WAVETABLE AMY_NO_MINIAUDIO _CRT_SECURE_NO_WARNINGS)
add_executable(CardenzaDemo render_demo.cpp ${synth_sources} ${midi_sources} ${fx_sources}
  ${CARDENZA_ROOT}/Library/Synth/Audio/Amy/SynthAudioRuntimeAmy.cpp
  ${CARDENZA_ROOT}/Applications/Synth/Runtime/ProjectStorage.cpp
  ${CARDENZA_ROOT}/Applications/Synth/Runtime/Sequencer/SynthSequencer.cpp
  ${CARDENZA_ROOT}/Applications/Synth/Runtime/SynthStorage.cpp
  ${CARDENZA_ROOT}/OS/Storage/Storage.cpp
  ${CARDENZA_ROOT}/Devices/CardenzaSim/Device.cpp
  ${CARDENZA_ROOT}/Devices/CardenzaSim/Storage.cpp)
target_compile_features(CardenzaDemo PRIVATE cxx_std_17)
target_link_libraries(CardenzaDemo PRIVATE DemoAmy)
if(MSVC)
  target_compile_options(DemoAmy PRIVATE $<$<COMPILE_LANGUAGE:C>:/std:c11>)
else()
  target_link_libraries(DemoAmy PUBLIC m pthread)
endif()
