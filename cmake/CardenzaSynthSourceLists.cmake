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
            ${root_dir}/Applications/Synth/GrooveAppGuide.cpp
            ${root_dir}/Applications/Synth/StressTest.cpp
            ${root_dir}/Applications/Synth/Runtime/SessionStore.cpp
            ${root_dir}/Applications/Synth/UI/Groove/GrooveController.cpp
            ${root_dir}/Applications/Synth/UI/Groove/GrooveGuide.cpp
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
        ${root_dir}/Library/Synth/Audio/SampleStream.cpp
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
        ${root_dir}/OS/System/Log.cpp
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

# CardenzaHAL (Devices/Cardenza/cardenza) is the upstream submodule, a clean consumer copy (AGENT.md):
# the build refuses one with local changes, whatever started it (make, idf.py, an IDE).
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
            message(FATAL_ERROR "CardenzaHAL (Devices/Cardenza/cardenza) has local changes, but it is a clean consumer "
                                "copy (AGENT.md). Change the HAL in its clone ~/projects/CardenzaHAL and restore this one:\n"
                                "  git -C Devices/Cardenza/cardenza checkout -- . && git -C Devices/Cardenza/cardenza clean -fd\n"
                                "${hal_changes}")
        endif()
    endif()
endfunction()

# CardenzaHAL builds as its own ESP-IDF component (Devices/Cardenza/CMakeLists.txt): its sources and
# dependencies are its business. This only refuses a modified submodule.
function(cardenza_collect_firmware_hal_cpp_sources root_dir out_var)
    if(DEFINED COMPONENT_DIR)
        cardenza_check_hal_pristine(${root_dir})
    endif()
    set(${out_var} "" PARENT_SCOPE)
endfunction()

function(cardenza_collect_firmware_device_cpp_sources root_dir out_var)
    set(sources
        ${root_dir}/Devices/Cardenza/Device.cpp
        ${root_dir}/Devices/Cardenza/DeviceLog.cpp
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

    # hold_and_modify, AMY's per-oscillator control step, ran once per oscillator and block and took a
    # third of all audio time on the board (DEMO-2, 2026-11-04): amp_combine_controls put every one of
    # its coefficients through log2f, most of them zero, and in double precision ("- 1.0"). A zero
    # coefficient adds nothing (the constant term is swapped to 1 first), so skip it; same result.
    set(amp_original "        if (i != COEF_MOD) {\n            val = map_60dB_to_01f(MAX(0, val)) - 1.0;")
    set(amp_patched "        if (coef == 0.0f) continue;  // Cardenza: adds nothing; saves a log2f\n        if (i != COEF_MOD) {\n            val = cardenza_map_60dB_to_01f(MAX(0, val)) - 1.0f;  // Cardenza: AmyFastMath.inc")
    string(FIND "${content}" "${amp_original}" match_index)
    if(match_index EQUAL -1)
        message(FATAL_ERROR "AMY amp_combine_controls patch no longer applies in ${source}")
    endif()
    string(REPLACE "${amp_original}" "${amp_patched}" content "${content}")

    # The amplitudes' log2f and powf(10, x) as polynomials (Library/Synth/Audio/Amy/AmyFastMath.inc):
    # twice an oscillator and block, they were library calls of hundreds of cycles each.
    get_filename_component(cardenza_root "${amy_src_dir}/../../../.." ABSOLUTE)
    set(note_original "    return (midi_note - ZERO_MIDI_NOTE) / 12.0f;")
    set(note_patched "    return (midi_note - ZERO_MIDI_NOTE) * (1.0f / 12.0f);  // Cardenza: no divide (a ROM call here), every control step")
    set(pow_original "    result = powf(10.0f, result);\n    if (result <= AMP_THRESH_PLUS)  result = 0; ")
    set(pow_patched "    result = cardenza_exp2f(3.321928095f * result);  // Cardenza: 10^x\n    if (result <= AMP_THRESH_PLUS)  result = 0; ")
    set(math_original "float combine_controls(float *controls, float *coefs) {")
    set(math_patched "#include \"${cardenza_root}/Library/Synth/Audio/Amy/AmyFastMath.inc\"  // Cardenza\n\nfloat combine_controls(float *controls, float *coefs) {")
    foreach(pair "pow" "math" "note")
        string(FIND "${content}" "${${pair}_original}" match_index)
        if(match_index EQUAL -1)
            message(FATAL_ERROR "AMY amy.c patch (${pair}) no longer applies in ${source}")
        endif()
        string(REPLACE "${${pair}_original}" "${${pair}_patched}" content "${content}")
    endforeach()

    # Oscillator state through our allocator (AmyEmbeddedStubs.c): internal RAM while there is room,
    # PSRAM after; patches.c gives it back when the voice is released (cardenza_amy_patches_source).
    set(osc_original "    uint8_t *ptr = malloc_caps(sizeof(struct synthinfo) + sizeof(struct mod_synthinfo)\n                               + total_num_breakpoints * (sizeof(float) + sizeof(uint32_t)),\n                               amy_global.config.ram_caps_events);")
    set(osc_patched "    extern void *cardenza_amy_osc_malloc(uint32_t size);  // Cardenza: internal RAM, PSRAM when short\n    uint8_t *ptr = cardenza_amy_osc_malloc(sizeof(struct synthinfo) + sizeof(struct mod_synthinfo)\n                               + total_num_breakpoints * (sizeof(float) + sizeof(uint32_t)));")
    string(FIND "${content}" "${osc_original}" match_index)
    if(match_index EQUAL -1)
        message(FATAL_ERROR "AMY alloc_osc patch no longer applies in ${source}")
    endif()
    string(REPLACE "${osc_original}" "${osc_patched}" content "${content}")

    # The event pool's blocks through our allocator too (PSRAM first, AmyEmbeddedStubs.c).
    set(pool_original "    struct delta *new_pool = (struct delta *)malloc_caps(max_delta_pool_size * sizeof(struct delta),\n                                                         amy_global.config.ram_caps_synth);")
    set(pool_patched "    extern void *cardenza_amy_pool_malloc(uint32_t size);  // Cardenza: PSRAM first\n    struct delta *new_pool = (struct delta *)cardenza_amy_pool_malloc(max_delta_pool_size * sizeof(struct delta));")
    string(FIND "${content}" "${pool_original}" match_index)
    if(match_index EQUAL -1)
        message(FATAL_ERROR "AMY deltas_pool_alloc patch no longer applies in ${source}")
    endif()
    string(REPLACE "${pool_original}" "${pool_patched}" content "${content}")

    set(output_dir ${CMAKE_BINARY_DIR}/cardenza_generated/amy)
    file(WRITE ${output_dir}/amy.c.in "${content}")
    # configure_file only touches amy.c when the content changes, so rebuilds stay incremental.
    configure_file(${output_dir}/amy.c.in ${output_dir}/amy.c COPYONLY)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${source})
    set(${out_var} ${output_dir}/amy.c PARENT_SCOPE)
endfunction()

# AMY keeps an oscillator's memory for good once a voice has used it: loading project after project
# (or switching engines) left four DX7 tracks' worth, about 56 KB, held in internal RAM, and the next
# project could not load (NO MEMORY, the stress test 2026-11-04). Releasing a voice frees its
# oscillators instead of only resetting them; AMY allocates them again for the next voice that uses
# them (ensure_osc_allocd), and every loop over oscillators already skips unallocated ones.
function(cardenza_amy_patches_source amy_src_dir out_var)
    set(source ${amy_src_dir}/patches.c)
    if(CMAKE_SCRIPT_MODE_FILE)
        set(${out_var} ${source} PARENT_SCOPE)
        return()
    endif()
    file(READ ${source} content)
    set(release_original "                AMY_UNSET(osc_to_voice[i]);\n                // Make sure the osc is cleared.\n                reset_osc(i);")
    set(release_patched "                AMY_UNSET(osc_to_voice[i]);\n                // Cardenza: give its memory back; the next voice that uses it allocates it again.\n                free_osc(i);")
    string(FIND "${content}" "${release_original}" match_index)
    if(match_index EQUAL -1)
        message(FATAL_ERROR "AMY release_voice_oscs patch no longer applies in ${source}")
    endif()
    string(REPLACE "${release_original}" "${release_patched}" content "${content}")
    # patches_deinit forgets the voice and patch tables (one block from patches_init) without freeing
    # them: amy_stop leaked it every time.
    set(deinit_original "void patches_deinit() {\n    memory_patch_deltas = NULL;")
    set(deinit_patched "void patches_deinit() {\n    free(memory_patch_deltas);  // Cardenza: the block patches_init allocated\n    memory_patch_deltas = NULL;")
    string(FIND "${content}" "${deinit_original}" match_index)
    if(match_index EQUAL -1)
        message(FATAL_ERROR "AMY patches_deinit patch no longer applies in ${source}")
    endif()
    string(REPLACE "${deinit_original}" "${deinit_patched}" content "${content}")
    # The ROM patches' text (static here), for keeping one as a memory patch: AMY parses a ROM patch
    # once per voice it builds, a memory patch once (SynthAudioRuntimeAmy.cpp, CachedAmyPatchNoLock).
    string(APPEND content "\n// Cardenza: a ROM patch's text, null past the table.\nconst char *cardenza_patch_command(uint16_t patch_number) {\n    return patch_number < sizeof(patch_commands) / sizeof(patch_commands[0]) ? patch_commands[patch_number] : NULL;\n}\n")
    set(output_dir ${CMAKE_BINARY_DIR}/cardenza_generated/amy)
    file(WRITE ${output_dir}/patches.c.in "${content}")
    configure_file(${output_dir}/patches.c.in ${output_dir}/patches.c COPYONLY)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${source})
    set(${out_var} ${output_dir}/patches.c PARENT_SCOPE)
endfunction()

# The Juno's 24 dB lowpass with exact 64-bit products (Library/Synth/Audio/Amy/AmyFastFilter.inc):
# AMY's form took 167 instructions a sample, a quarter of a Juno voice. filter_process calls ours;
# the copy keeps the object name, so the IRAM placement (audio_hot.lf) still applies.
function(cardenza_amy_filters_source root_dir amy_src_dir out_var)
    set(source ${amy_src_dir}/filters.c)
    if(CMAKE_SCRIPT_MODE_FILE)
        set(${out_var} ${source} PARENT_SCOPE)
        return()
    endif()
    file(READ ${source} content)
    set(call_original "max_val = dsps_biquad_f32_ansi_split_fb_twice(block, block, AMY_BLOCK_SIZE, coeffs, synth[osc]->filter_delay, max_val);")
    set(call_patched "max_val = cardenza_biquad_lpf24(block, block, AMY_BLOCK_SIZE, coeffs, synth[osc]->filter_delay);  // Cardenza")
    set(define_original "SAMPLE dsps_biquad_f32_ansi_split_fb_once(const SAMPLE *input")
    set(define_patched "#include \"${root_dir}/Library/Synth/Audio/Amy/AmyFastFilter.inc\"  // Cardenza\n\nSAMPLE dsps_biquad_f32_ansi_split_fb_once(const SAMPLE *input")
    foreach(pair "call" "define")
        string(FIND "${content}" "${${pair}_original}" match_index)
        if(match_index EQUAL -1)
            message(FATAL_ERROR "AMY filters.c patch (${pair}) no longer applies in ${source}")
        endif()
        string(REPLACE "${${pair}_original}" "${${pair}_patched}" content "${content}")
    endforeach()
    set(output_dir ${CMAKE_BINARY_DIR}/cardenza_generated/amy)
    file(WRITE ${output_dir}/filters.c.in "${content}")
    configure_file(${output_dir}/filters.c.in ${output_dir}/filters.c COPYONLY)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${source})
    set(${out_var} ${output_dir}/filters.c PARENT_SCOPE)
endfunction()

# The oscillators' table loops without shifts by a variable amount (Library/Synth/Audio/Amy/AmyFastOsc.inc):
# the Juno's pulse, saw and sub (cubic) and the DX7's operators (linear) to the bit, and a pulse at
# half duty, the Juno's sub, in one pass instead of two. The copy keeps the object name, so the IRAM
# placement (audio_hot.lf) still applies.
function(cardenza_amy_oscillators_source root_dir amy_src_dir out_var)
    set(source ${amy_src_dir}/oscillators.c)
    if(CMAKE_SCRIPT_MODE_FILE)
        set(${out_var} ${source} PARENT_SCOPE)
        return()
    endif()
    file(READ ${source} content)
    set(define_original "SAMPLE render_lpf_lut(SAMPLE* buf, uint16_t osc, int8_t is_square, int8_t direction, SAMPLE dc_offset) {")
    set(define_patched "#include \"${root_dir}/Library/Synth/Audio/Amy/AmyFastOsc.inc\"  // Cardenza\n\n${define_original}")
    set(first_original "    synth[osc]->phase = render_lut_cub(buf, synth[osc]->phase, step, last_amp, amp, synth[osc]->lut, &max_value);\n    if (is_square) {")
    set(first_patched "    // Cardenza: a pulse that stays at half duty in one pass.\n    const int cardenza_square = is_square && msynth[osc]->duty == 0.5f && msynth[osc]->last_duty == 0.5f;\n    synth[osc]->phase = cardenza_render_lut_cub(buf, synth[osc]->phase, step, last_amp, amp, synth[osc]->lut, cardenza_square, &max_value);\n    if (is_square && !cardenza_square) {")
    set(second_original "        render_lut_cub(buf, pwm_phase, step + delta_phase_per_sample, -last_amp, -amp, synth[osc]->lut, &max_value);")
    set(second_patched "        cardenza_render_lut_cub(buf, pwm_phase, step + delta_phase_per_sample, -last_amp, -amp, synth[osc]->lut, 0, &max_value);  // Cardenza")
    set(fm_original "    if (feedback_level > 0 && mod)\n        synth[osc]->phase = render_lut_fm_fb(buf, synth[osc]->phase, step,")
    set(fm_patched "    if (1)  // Cardenza: the same choice of loop\n        synth[osc]->phase = cardenza_render_lut_lin(buf, synth[osc]->phase, step, last_amp, amp, synth[osc]->lut, mod, feedback_level, synth[osc]->last_two, &max_value);\n    else if (feedback_level > 0 && mod)\n        synth[osc]->phase = render_lut_fm_fb(buf, synth[osc]->phase, step,")
    # render_sine's and render_triangle's call (the same line).
    set(lin_original "    synth[osc]->phase = render_lut(buf, synth[osc]->phase, step, last_amp, amp, synth[osc]->lut, &max_value);")
    set(lin_patched "    synth[osc]->phase = cardenza_render_lut_lin(buf, synth[osc]->phase, step, last_amp, amp, synth[osc]->lut, NULL, 0, NULL, &max_value);  // Cardenza")
    foreach(pair "define" "first" "second" "fm" "lin")
        string(FIND "${content}" "${${pair}_original}" match_index)
        if(match_index EQUAL -1)
            message(FATAL_ERROR "AMY oscillators.c patch (${pair}) no longer applies in ${source}")
        endif()
        string(REPLACE "${${pair}_original}" "${${pair}_patched}" content "${content}")
    endforeach()
    set(output_dir ${CMAKE_BINARY_DIR}/cardenza_generated/amy)
    file(WRITE ${output_dir}/oscillators.c.in "${content}")
    configure_file(${output_dir}/oscillators.c.in ${output_dir}/oscillators.c COPYONLY)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${source} ${root_dir}/Library/Synth/Audio/Amy/AmyFastOsc.inc)
    set(${out_var} ${output_dir}/oscillators.c PARENT_SCOPE)
endfunction()

# A modulation source's control step once a block, not once for every oscillator it modulates:
# compute_mod_scale ran hold_and_modify on the source each time it was asked (8 us on the board), and
# a Juno voice asks five times a block for its LFO, whose value compute_mod_value keeps for the block
# anyway; repeated, it also moved the source's portamento on several steps a block.
function(cardenza_amy_envelope_source amy_src_dir out_var)
    set(source ${amy_src_dir}/envelope.c)
    if(CMAKE_SCRIPT_MODE_FILE)
        set(${out_var} ${source} PARENT_SCOPE)
        return()
    endif()
    file(READ ${source} content)
    set(mod_original "        if(source != osc) {  // that would be weird\n            hold_and_modify(source);")
    set(mod_patched "        if(source != osc) {  // that would be weird\n            // Cardenza: its value is the block's already (compute_mod_value), which is all this step feeds.\n            if (synth[source]->mod_value_clock != amy_global.total_blocks*AMY_BLOCK_SIZE) hold_and_modify(source);")
    string(FIND "${content}" "${mod_original}" match_index)
    if(match_index EQUAL -1)
        message(FATAL_ERROR "AMY envelope.c patch no longer applies in ${source}")
    endif()
    string(REPLACE "${mod_original}" "${mod_patched}" content "${content}")
    set(output_dir ${CMAKE_BINARY_DIR}/cardenza_generated/amy)
    file(WRITE ${output_dir}/envelope.c.in "${content}")
    configure_file(${output_dir}/envelope.c.in ${output_dir}/envelope.c COPYONLY)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${source})
    set(${out_var} ${output_dir}/envelope.c PARENT_SCOPE)
endfunction()

function(cardenza_collect_amy_c_sources root_dir out_var)
    set(amy_src_dir ${root_dir}/Library/third_party/amy/src)
    cardenza_amy_core_source(${amy_src_dir} amy_core_source)
    cardenza_amy_patches_source(${amy_src_dir} amy_patches_source)
    cardenza_amy_filters_source(${root_dir} ${amy_src_dir} amy_filters_source)
    cardenza_amy_envelope_source(${amy_src_dir} amy_envelope_source)
    cardenza_amy_oscillators_source(${root_dir} ${amy_src_dir} amy_oscillators_source)
    set(sources
        ${amy_src_dir}/algorithms.c
        ${amy_core_source}
        ${amy_src_dir}/api.c
        ${amy_src_dir}/custom.c
        ${amy_src_dir}/delay.c
        ${amy_envelope_source}
        ${amy_src_dir}/examples.c
        ${amy_filters_source}
        ${amy_src_dir}/instrument.c
        ${amy_src_dir}/interp_partials.c
        ${amy_src_dir}/log2_exp2.c
        ${amy_src_dir}/midi_mappings.c
        ${amy_oscillators_source}
        ${amy_src_dir}/parse.c
        ${amy_patches_source}
        ${amy_src_dir}/pcm.c
        ${amy_src_dir}/sequencer.c
        ${amy_src_dir}/transfer.c
        ${root_dir}/Library/Synth/Audio/Amy/AmyEmbeddedStubs.c
    )
    # AMY's floating-point literals as single precision: written 1.0 or 0.5 they turned every block's
    # control math (hold_and_modify, the envelopes, note-on) into software double precision on the
    # ESP32-S3, whose FPU has single precision only. Host GCC builds here, so the tests hear the same;
    # the firmware component sets it on its C sources itself (Devices/Cardenza/CMakeLists.txt). Clang
    # has no such option. Source properties belong to the directory that builds the target.
    if(NOT CMAKE_SCRIPT_MODE_FILE AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
        set_source_files_properties(${sources} PROPERTIES COMPILE_OPTIONS "-fsingle-precision-constant")
    endif()
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()
