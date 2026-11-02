# Browser simulator. The device facade is the desktop simulator's (framebuffer, key matrix,
# LEDs, storage paths); only the host loop and the page are web-specific.
add_library(CardenzaWebDevice)

cardenza_collect_sim_device_cpp_sources(${PROJECT_SOURCE_DIR} cardenza_web_device_sources)
target_sources(CardenzaWebDevice PRIVATE
    ${cardenza_web_device_sources}
)

target_include_directories(CardenzaWebDevice PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(CardenzaWebDevice PUBLIC
    CardenzaDeviceInterface
)

# AMY switches to its own web build under __EMSCRIPTEN__ (48 kHz, wasm-worker locks, JS hooks).
# The groovebox mixes AMY with its own 44.1 kHz sampler and effects, so AMY is compiled as the
# plain host build the desktop simulator uses. Only AMY's C sources are affected.
target_compile_options(CardenzaAmy PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-U__EMSCRIPTEN__>
)

set(CARDENZA_WEB_SITE_DIR ${CMAKE_BINARY_DIR}/site CACHE PATH "Output directory of the CardenzaWeb page")

add_executable(CardenzaWeb
    ${CMAKE_CURRENT_SOURCE_DIR}/WebMain.cpp
)

target_include_directories(CardenzaWeb PRIVATE
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(CardenzaWeb PRIVATE
    CardenzaWebDevice
    CardenzaSystem
)

set_target_properties(CardenzaWeb PROPERTIES
    OUTPUT_NAME cardenza
    SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY ${CARDENZA_WEB_SITE_DIR}
)

# No pthreads: the page must run without COOP/COEP headers (for example inside an iframe).
target_link_options(CardenzaWeb PRIVATE
    -sMODULARIZE=1
    -sEXPORT_NAME=createCardenzaModule
    -sENVIRONMENT=web
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=33554432
    -sSTACK_SIZE=1048576
    -sFORCE_FILESYSTEM=1
    -lidbfs.js
    "-sEXPORTED_RUNTIME_METHODS=FS,IDBFS,HEAPU8,HEAPU16,HEAPF32,UTF8ToString,ccall,addRunDependency,removeRunDependency"
)

# The page itself: plain HTML / CSS / JS next to cardenza.js and cardenza.wasm.
set(cardenza_web_page_files
    index.html
    cardenza-web.css
    cardenza-web.js
    cardenza-audio-worklet.js
)
set(cardenza_web_page_outputs)
foreach(page_file IN LISTS cardenza_web_page_files)
    add_custom_command(
        OUTPUT ${CARDENZA_WEB_SITE_DIR}/${page_file}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/web/${page_file} ${CARDENZA_WEB_SITE_DIR}/${page_file}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/web/${page_file}
        VERBATIM
    )
    list(APPEND cardenza_web_page_outputs ${CARDENZA_WEB_SITE_DIR}/${page_file})
endforeach()
add_custom_target(CardenzaWebPage ALL DEPENDS ${cardenza_web_page_outputs})
add_dependencies(CardenzaWeb CardenzaWebPage)
