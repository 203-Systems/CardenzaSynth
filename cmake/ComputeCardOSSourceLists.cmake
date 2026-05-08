function(ccos_collect_builtin_application_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/Applications/BuiltInApplications.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(ccos_collect_synth_app_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/Applications/Synth/SynthApp.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(ccos_collect_synth_runtime_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/Library/Synth/Audio/Amy/SynthAudioRuntimeAmy.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(ccos_collect_synth_core_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/Library/Synth/Core/Engines/Dx7/Dx7SynthEngine.cpp
        ${root_dir}/Library/Synth/Core/Engines/Juno/JunoSynthEngine.cpp
        ${root_dir}/Library/Synth/Core/PagedSynthEngine.cpp
        ${root_dir}/Library/Synth/Core/Engines/Sampler/SamplerSynthEngine.cpp
        ${root_dir}/Library/Synth/Core/SynthEngineFactory.cpp
        ${root_dir}/Library/Synth/Core/SynthRack.cpp
        ${root_dir}/Library/Synth/SynthUiRenderer.cpp
        ${root_dir}/Library/Synth/UI/Dx7/Dx7UiModel.cpp
        ${root_dir}/Library/Synth/UI/Dx7/Dx7UiRenderer.cpp
        ${root_dir}/Library/Synth/UI/Juno/JunoUiModel.cpp
        ${root_dir}/Library/Synth/UI/Juno/JunoUiRenderer.cpp
        ${root_dir}/Library/Synth/UI/Menu/SynthUiMenu.cpp
        ${root_dir}/Library/Synth/UI/Render/SynthUiRenderPrimitives.cpp
        ${root_dir}/Library/Synth/SynthUiModel.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(ccos_collect_midi_framework_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/OS/Framework/Midi/MidiPacket.cpp
        ${root_dir}/OS/Framework/Midi/MidiPort.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(ccos_collect_midi_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/OS/MIDI/MIDI.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(ccos_collect_system_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/OS/System/System.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(ccos_collect_ui_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/OS/UI/BuiltInFonts.cpp
        ${root_dir}/OS/UI/Canvas.cpp
        ${root_dir}/OS/UI/UI.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(ccos_collect_computecard_hal_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/Devices/ComputeCard/computecard/computecardHAL.cpp
        ${root_dir}/Devices/ComputeCard/computecard/audio/audio.cpp
        ${root_dir}/Devices/ComputeCard/computecard/audio/codec/es8156.cpp
        ${root_dir}/Devices/ComputeCard/computecard/display/display.cpp
        ${root_dir}/Devices/ComputeCard/computecard/ir/ir.cpp
        ${root_dir}/Devices/ComputeCard/computecard/mic/mic.cpp
        ${root_dir}/Devices/ComputeCard/computecard/system/system.cpp
    )

    if(DEFINED COMPONENT_DIR)
        list(APPEND sources
            ${root_dir}/Devices/ComputeCard/computecard/keyboard/keyboard.cpp
            ${root_dir}/Devices/ComputeCard/computecard/storage/storage.cpp
        )
    endif()

    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(ccos_collect_computecard_device_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/Devices/ComputeCard/Device.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(ccos_collect_computecard_device_audio_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/Devices/ComputeCard/DeviceAudio.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(ccos_collect_computecardsim_device_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/Devices/ComputeCardSim/Device.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(ccos_collect_computecardsim_device_audio_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/Devices/ComputeCardSim/DeviceAudio.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(ccos_collect_computecardsim_host_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/Devices/ComputeCardSim/main.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(ccos_collect_computecardsim_desktop_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/Devices/ComputeCardSim/DesktopHost.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(ccos_collect_common_cpp_sources root_dir out_var)
    ccos_collect_builtin_application_cpp_sources(${root_dir} builtin_application_sources)
    ccos_collect_synth_app_cpp_sources(${root_dir} synth_app_sources)
    ccos_collect_synth_runtime_cpp_sources(${root_dir} synth_runtime_sources)
    ccos_collect_synth_core_cpp_sources(${root_dir} synth_core_sources)
    ccos_collect_midi_framework_cpp_sources(${root_dir} midi_framework_sources)
    ccos_collect_midi_cpp_sources(${root_dir} midi_sources)
    ccos_collect_system_cpp_sources(${root_dir} system_sources)
    ccos_collect_ui_cpp_sources(${root_dir} ui_sources)
    set(sources
        ${builtin_application_sources}
        ${synth_app_sources}
        ${synth_runtime_sources}
        ${synth_core_sources}
        ${midi_framework_sources}
        ${midi_sources}
        ${system_sources}
        ${ui_sources}
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(ccos_collect_computecard_firmware_cpp_sources root_dir out_var)
    ccos_collect_computecard_device_cpp_sources(${root_dir} device_sources)
    ccos_collect_computecard_device_audio_cpp_sources(${root_dir} device_audio_sources)
    ccos_collect_computecard_hal_cpp_sources(${root_dir} hal_sources)
    set(sources
        ${device_sources}
        ${device_audio_sources}
        ${hal_sources}
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(ccos_collect_amy_c_sources root_dir out_var)
    set(amy_src_dir ${root_dir}/third_party/amy/src)
    set(sources
        ${amy_src_dir}/algorithms.c
        ${amy_src_dir}/amy.c
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