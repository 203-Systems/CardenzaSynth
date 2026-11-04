function(cardenza_collect_builtin_application_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/Applications/BuiltInApplications.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

# The groovebox UI is the default. -DCARDENZA_GROOVE_UI=OFF still builds the previous UI, which stays
# in the tree until it is removed.
function(cardenza_groove_ui_enabled out_var)
    if(NOT DEFINED CARDENZA_GROOVE_UI OR CARDENZA_GROOVE_UI)
        set(${out_var} ON PARENT_SCOPE)
    else()
        set(${out_var} OFF PARENT_SCOPE)
    endif()
endfunction()

function(cardenza_collect_synth_app_cpp_sources root_dir out_var)
    set(runtime_sources
        ${root_dir}/Applications/Synth/Runtime/Sampler/SamplerManager.cpp
        ${root_dir}/Applications/Synth/Runtime/ProjectStorage.cpp
        ${root_dir}/Applications/Synth/Runtime/Sequencer/SynthSequencer.cpp
        ${root_dir}/Applications/Synth/Runtime/SynthRack.cpp
        ${root_dir}/Applications/Synth/Runtime/SynthStorage.cpp
    )
    cardenza_groove_ui_enabled(groove_ui)
    if(groove_ui)
        set(${out_var}
            ${runtime_sources}
            ${root_dir}/Applications/Synth/GrooveApp.cpp
            ${root_dir}/Applications/Synth/UI/Groove/GrooveController.cpp
            ${root_dir}/Applications/Synth/UI/Groove/GrooveModals.cpp
            ${root_dir}/Applications/Synth/UI/Groove/GrooveRecord.cpp
            ${root_dir}/Applications/Synth/UI/Groove/GrooveScenes.cpp
            ${root_dir}/Applications/Synth/UI/Groove/GrooveTypes.cpp
            ${root_dir}/Applications/Synth/UI/Groove/InstrumentPages.cpp
            ${root_dir}/Applications/Synth/UI/Groove/Render/Animator.cpp
            ${root_dir}/Applications/Synth/UI/Groove/Render/Effects.cpp
            ${root_dir}/Applications/Synth/UI/Groove/Render/Gfx.cpp
            ${root_dir}/Applications/Synth/UI/Groove/Render/GrooveRenderer.cpp
            ${root_dir}/Applications/Synth/UI/Groove/Render/ModalRender.cpp
            ${root_dir}/Applications/Synth/UI/Groove/Render/SceneRender.cpp
            ${root_dir}/Applications/Synth/UI/Groove/Render/Widgets.cpp
            PARENT_SCOPE)
        return()
    endif()
    set(sources
        ${runtime_sources}
        ${root_dir}/Applications/Synth/SynthApp.cpp
        ${root_dir}/Applications/Synth/UI/Drums/DrumsUiRenderer.cpp
        ${root_dir}/Applications/Synth/UI/Dx7/Dx7UiModel.cpp
        ${root_dir}/Applications/Synth/UI/Dx7/Dx7UiRenderer.cpp
        ${root_dir}/Applications/Synth/UI/Juno/JunoUiModel.cpp
        ${root_dir}/Applications/Synth/UI/Juno/JunoUiRenderer.cpp
        ${root_dir}/Applications/Synth/UI/Menu/SynthUiMenu.cpp
        ${root_dir}/Applications/Synth/UI/Render/SynthUiRenderPrimitives.cpp
        ${root_dir}/Applications/Synth/UI/Sampler/SamplerUiModel.cpp
        ${root_dir}/Applications/Synth/UI/Sequencer/SequencerUiRenderer.cpp
        ${root_dir}/Applications/Synth/UI/SynthUiModel.cpp
        ${root_dir}/Applications/Synth/UI/SynthUiRenderer.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

# Effects DSP (no AMY dependency). Listed on its own for builds that compile
# SynthAudioRuntimeAmy.cpp directly, such as the synth regression test.
function(cardenza_collect_synth_fx_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/Library/Synth/Audio/Fx/SynthFxDsp.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(cardenza_collect_synth_runtime_cpp_sources root_dir out_var)
    cardenza_collect_synth_fx_cpp_sources(${root_dir} fx_sources)
    set(sources
        ${root_dir}/Library/Synth/Audio/Amy/SynthAudioRuntimeAmy.cpp
        ${fx_sources}
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(cardenza_collect_synth_core_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/Library/Synth/Core/Engines/Drums/DrumsSynthEngine.cpp
        ${root_dir}/Library/Synth/Core/Engines/Dx7/Dx7FactoryPresets.cpp
        ${root_dir}/Library/Synth/Core/Engines/Dx7/Dx7SynthEngine.cpp
        ${root_dir}/Library/Synth/Core/Engines/Juno/JunoFactoryPresets.cpp
        ${root_dir}/Library/Synth/Core/Engines/Juno/JunoSynthEngine.cpp
        ${root_dir}/Library/Synth/Core/PagedSynthEngine.cpp
        ${root_dir}/Library/Synth/Core/Engines/Sampler/SamplerSynthEngine.cpp
        ${root_dir}/Library/Synth/Core/SynthEngineFactory.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(cardenza_collect_midi_framework_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/OS/Framework/Midi/MidiPacket.cpp
        ${root_dir}/OS/Framework/Midi/MidiPort.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(cardenza_collect_midi_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/OS/MIDI/MIDI.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(cardenza_collect_system_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/OS/System/System.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

# The app storage protocol (docs/app-storage-protocol.md). Portable: it reaches the card only through
# Device::Storage.
function(cardenza_collect_storage_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/OS/Storage/DeviceSettings.cpp
        ${root_dir}/OS/Storage/Migration.cpp
        ${root_dir}/OS/Storage/Settings.cpp
        ${root_dir}/OS/Storage/Storage.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(cardenza_collect_ui_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/OS/UI/BuiltInFonts.cpp
        ${root_dir}/OS/UI/Canvas.cpp
        ${root_dir}/OS/UI/UI.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

# CardenzaHAL (Devices/Cardenza/cardenza) is the read-only upstream submodule (AGENT.md): the build
# refuses a HAL with local changes, whatever started it (make, idf.py, an IDE). Its sources are listed
# here like Cardenza-MPE does; keep the list in step with the HAL's own CMakeLists.txt.
function(cardenza_check_hal_pristine root_dir)
    set(hal_dir ${root_dir}/Devices/Cardenza/cardenza)
    if(NOT EXISTS ${hal_dir}/cardenzaHAL.h)
        message(FATAL_ERROR "CardenzaHAL submodule missing: git submodule update --init Devices/Cardenza/cardenza")
    endif()
    find_package(Git QUIET)
    if(GIT_FOUND AND EXISTS ${hal_dir}/.git)
        execute_process(COMMAND ${GIT_EXECUTABLE} -C ${hal_dir} config remote.origin.pushurl
                                no-push://CardenzaHAL-is-a-read-only-dependency)
        execute_process(COMMAND ${GIT_EXECUTABLE} -C ${hal_dir} status --porcelain
                        OUTPUT_VARIABLE hal_changes OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NOT hal_changes STREQUAL "")
            message(FATAL_ERROR "CardenzaHAL (Devices/Cardenza/cardenza) has local changes, but it is a read-only "
                                "dependency (AGENT.md). Work above it in Devices/Cardenza and restore it:\n"
                                "  git -C Devices/Cardenza/cardenza checkout -- . && git -C Devices/Cardenza/cardenza clean -fd\n"
                                "${hal_changes}")
        endif()
    endif()
endfunction()

function(cardenza_collect_firmware_hal_cpp_sources root_dir out_var)
    set(sources)
    if(DEFINED COMPONENT_DIR)
        cardenza_check_hal_pristine(${root_dir})
        list(APPEND sources
            ${root_dir}/Devices/Cardenza/cardenza/cardenzaHAL.cpp
            ${root_dir}/Devices/Cardenza/cardenza/audio/audio.cpp
            ${root_dir}/Devices/Cardenza/cardenza/audio/codec/es8156.cpp
            ${root_dir}/Devices/Cardenza/cardenza/display/display.cpp
            ${root_dir}/Devices/Cardenza/cardenza/ir/ir.cpp
            ${root_dir}/Devices/Cardenza/cardenza/keyboard/keyboard.cpp
            ${root_dir}/Devices/Cardenza/cardenza/keyboard/internal/keyboard_hardware.cpp
            ${root_dir}/Devices/Cardenza/cardenza/keyboard/internal/keyboard_key_scan.cpp
            ${root_dir}/Devices/Cardenza/cardenza/keyboard/internal/keyboard_led_scan.cpp
            ${root_dir}/Devices/Cardenza/cardenza/mic/mic.cpp
            ${root_dir}/Devices/Cardenza/cardenza/storage/storage.cpp
            ${root_dir}/Devices/Cardenza/cardenza/system/system.cpp
        )
    endif()

    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(cardenza_collect_firmware_device_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/Devices/Cardenza/Device.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(cardenza_collect_firmware_device_audio_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/Devices/Cardenza/DeviceAudio.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(cardenza_collect_sim_device_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/Devices/CardenzaSim/Device.cpp
        ${root_dir}/Devices/CardenzaSim/Storage.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(cardenza_collect_sim_device_audio_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/Devices/CardenzaSim/DeviceAudio.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(cardenza_collect_sim_host_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/Devices/CardenzaSim/main.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(cardenza_collect_sim_headless_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/Devices/CardenzaSim/Headless/HeadlessScript.cpp
        ${root_dir}/Devices/CardenzaSim/Headless/HeadlessShot.cpp
        ${root_dir}/Devices/CardenzaSim/Headless/PngWriter.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(cardenza_collect_sim_desktop_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/Devices/CardenzaSim/DesktopHost.cpp
        ${root_dir}/Devices/CardenzaSim/DesktopHost/DesktopHostDrawing.cpp
        ${root_dir}/Devices/CardenzaSim/DesktopHost/DesktopHostLayout.cpp
        ${root_dir}/Devices/CardenzaSim/DesktopHost/DesktopSimulator.cpp
        ${root_dir}/Devices/CardenzaSim/DesktopHost/DesktopSimulatorDebug.cpp
        ${root_dir}/Devices/CardenzaSim/DesktopHost/DesktopSimulatorInput.cpp
        ${root_dir}/Devices/CardenzaSim/DesktopHost/DesktopSimulatorLayout.cpp
        ${root_dir}/Devices/CardenzaSim/DesktopHost/DesktopSimulatorRender.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(cardenza_collect_common_cpp_sources root_dir out_var)
    cardenza_collect_builtin_application_cpp_sources(${root_dir} builtin_application_sources)
    cardenza_collect_synth_app_cpp_sources(${root_dir} synth_app_sources)
    cardenza_collect_synth_runtime_cpp_sources(${root_dir} synth_runtime_sources)
    cardenza_collect_synth_core_cpp_sources(${root_dir} synth_core_sources)
    cardenza_collect_midi_framework_cpp_sources(${root_dir} midi_framework_sources)
    cardenza_collect_midi_cpp_sources(${root_dir} midi_sources)
    cardenza_collect_system_cpp_sources(${root_dir} system_sources)
    cardenza_collect_storage_cpp_sources(${root_dir} storage_sources)
    cardenza_collect_ui_cpp_sources(${root_dir} ui_sources)
    set(sources
        ${builtin_application_sources}
        ${synth_app_sources}
        ${synth_runtime_sources}
        ${synth_core_sources}
        ${midi_framework_sources}
        ${midi_sources}
        ${system_sources}
        ${storage_sources}
        ${ui_sources}
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(cardenza_collect_firmware_cpp_sources root_dir out_var)
    cardenza_collect_firmware_device_cpp_sources(${root_dir} device_sources)
    cardenza_collect_firmware_device_audio_cpp_sources(${root_dir} device_audio_sources)
    cardenza_collect_firmware_hal_cpp_sources(${root_dir} hal_sources)
    set(sources
        ${device_sources}
        ${device_audio_sources}
        ${hal_sources}
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

# AMY reserves its event pool in 2048-delta blocks (40 KB of internal RAM on ESP32-S3), but the
# synth keeps only about 640 deltas live. Compile a copy of amy.c with 256-delta blocks instead;
# the pool still grows on demand up to the same 32768-delta capacity. The submodule stays untouched.
function(cardenza_amy_core_source amy_src_dir out_var)
    set(source ${amy_src_dir}/amy.c)
    if(CMAKE_SCRIPT_MODE_FILE)
        # ESP-IDF's requirement scan runs component CMake in script mode; it only needs the list.
        set(${out_var} ${source} PARENT_SCOPE)
        return()
    endif()

    file(READ ${source} content)
    foreach(replacement
            "#define MAX_DELTA_BLOCKS 16|#define MAX_DELTA_BLOCKS 128"
            "#define DELTA_BLOCK_SIZE 2048|#define DELTA_BLOCK_SIZE 256")
        string(REPLACE "|" ";" replacement "${replacement}")
        list(GET replacement 0 original)
        list(GET replacement 1 patched)
        string(FIND "${content}" "${original}" match_index)
        if(match_index EQUAL -1)
            message(FATAL_ERROR "AMY delta pool patch no longer applies: '${original}' not found in ${source}")
        endif()
        string(REPLACE "${original}" "${patched}" content "${content}")
    endforeach()

    set(output_dir ${CMAKE_BINARY_DIR}/cardenza_generated/amy)
    file(WRITE ${output_dir}/amy.c.in "${content}")
    # configure_file only touches amy.c when the content changes, so rebuilds stay incremental.
    configure_file(${output_dir}/amy.c.in ${output_dir}/amy.c COPYONLY)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${source})
    set(${out_var} ${output_dir}/amy.c PARENT_SCOPE)
endfunction()

function(cardenza_collect_amy_c_sources root_dir out_var)
    set(amy_src_dir ${root_dir}/Library/third_party/amy/src)
    cardenza_amy_core_source(${amy_src_dir} amy_core_source)
    set(sources
        ${amy_src_dir}/algorithms.c
        ${amy_core_source}
        ${amy_src_dir}/api.c
        ${amy_src_dir}/custom.c
        ${amy_src_dir}/delay.c
        ${amy_src_dir}/envelope.c
        ${amy_src_dir}/examples.c
        ${amy_src_dir}/filters.c
        ${amy_src_dir}/instrument.c
        ${amy_src_dir}/interp_partials.c
        ${amy_src_dir}/log2_exp2.c
        ${amy_src_dir}/midi_mappings.c
        ${amy_src_dir}/oscillators.c
        ${amy_src_dir}/parse.c
        ${amy_src_dir}/patches.c
        ${amy_src_dir}/pcm.c
        ${amy_src_dir}/sequencer.c
        ${amy_src_dir}/transfer.c
        ${root_dir}/Library/Synth/Audio/Amy/AmyEmbeddedStubs.c
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()
