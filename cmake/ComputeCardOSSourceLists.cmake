function(ccos_collect_common_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/Applications/BuiltInApplications.cpp
        ${root_dir}/Applications/Synth/SynthApp.cpp
        ${root_dir}/Library/Synth/Dx7SynthEngine.cpp
        ${root_dir}/Library/Synth/JunoSynthEngine.cpp
        ${root_dir}/Library/Synth/PagedSynthEngine.cpp
        ${root_dir}/Library/Synth/SamplerSynthEngine.cpp
        ${root_dir}/Library/Synth/SynthEngineFactory.cpp
        ${root_dir}/Library/Synth/SynthRack.cpp
        ${root_dir}/Library/Synth/SynthUiRenderer.cpp
        ${root_dir}/Library/Synth/SynthUiModel.cpp
        ${root_dir}/Library/Synth/SynthAudioRuntimeAmy.cpp
        ${root_dir}/OS/Framework/Midi/MidiPacket.cpp
        ${root_dir}/OS/Framework/Midi/MidiPort.cpp
        ${root_dir}/OS/MIDI/MIDI.cpp
        ${root_dir}/OS/System/System.cpp
        ${root_dir}/OS/UI/BuiltInFonts.cpp
        ${root_dir}/OS/UI/Canvas.cpp
        ${root_dir}/OS/UI/UI.cpp
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(ccos_collect_computecard_device_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/Devices/ComputeCard/Device.cpp
        ${root_dir}/Devices/ComputeCard/DeviceAudio.cpp
        ${root_dir}/Devices/ComputeCard/main/hal/computecard/computecardHAL.cpp
        ${root_dir}/Devices/ComputeCard/main/hal/keyboard/keyboard.cpp
        ${root_dir}/Devices/ComputeCard/main/hal/computecard/audio/audio.cpp
        ${root_dir}/Devices/ComputeCard/main/hal/computecard/display/display.cpp
        ${root_dir}/Devices/ComputeCard/main/hal/computecard/ir/ir.cpp
        ${root_dir}/Devices/ComputeCard/main/hal/computecard/keyboard/keyboard.cpp
        ${root_dir}/Devices/ComputeCard/main/hal/computecard/mic/mic.cpp
        ${root_dir}/Devices/ComputeCard/main/hal/computecard/storage/storage.cpp
        ${root_dir}/Devices/ComputeCard/main/hal/computecard/system/system.cpp
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
        ${root_dir}/Library/Synth/AmyEmbeddedStubs.c
    )
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()