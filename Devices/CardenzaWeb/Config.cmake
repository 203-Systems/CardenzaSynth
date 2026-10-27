# CardenzaWeb: the CardenzaSim device facade compiled to WebAssembly with Emscripten.
# Configure with: emcmake cmake -S . -B build/web -DDEVICE=CardenzaWeb
if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "DEVICE=CardenzaWeb needs the Emscripten toolchain: emcmake cmake -S . -B build/web -DDEVICE=CardenzaWeb")
endif()

set(DEVICE_SIMULATOR 1)

# An unoptimized wasm build cannot render audio in real time.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Same device audio facade as the desktop simulator (no native output; the page pulls frames).
cardenza_collect_sim_device_audio_cpp_sources(${CMAKE_SOURCE_DIR} cardenza_web_device_audio_sources)
target_sources(CardenzaDeviceAudio PRIVATE
    ${cardenza_web_device_audio_sources}
)
