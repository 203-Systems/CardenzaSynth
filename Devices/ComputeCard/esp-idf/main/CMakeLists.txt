set(CCOS_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../../..)
set(AMY_SRC_DIR ${CCOS_ROOT}/third_party/amy/src)
include(${CCOS_ROOT}/cmake/ComputeCardOSSourceLists.cmake)

ccos_collect_common_cpp_sources(${CCOS_ROOT} CCOS_COMMON_CPP_SOURCES)
ccos_collect_computecard_device_cpp_sources(${CCOS_ROOT} CCOS_DEVICE_CPP_SOURCES)
ccos_collect_amy_c_sources(${CCOS_ROOT} AMY_C_SOURCES)

idf_component_register(
    SRCS
        app_main.cpp
        ${CCOS_COMMON_CPP_SOURCES}
        ${CCOS_DEVICE_CPP_SOURCES}
        ${AMY_C_SOURCES}
    INCLUDE_DIRS
        ${CCOS_ROOT}
        ${AMY_SRC_DIR}
    REQUIRES
        driver
        esp_lcd
        esp_rom
        esp_timer
        fatfs
        freertos
        log
        pthread
        sdmmc
        vfs
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
set_property(TARGET ${COMPONENT_LIB} PROPERTY C_STANDARD 11)
set_property(TARGET ${COMPONENT_LIB} PROPERTY C_STANDARD_REQUIRED ON)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    AMY_WAVETABLE
    AMY_NO_MINIAUDIO
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-Wno-unused-parameter>
    $<$<COMPILE_LANGUAGE:C>:-Wno-sign-compare>
    $<$<COMPILE_LANGUAGE:C>:-Wno-missing-field-initializers>
    $<$<COMPILE_LANGUAGE:C>:-Wno-float-conversion>
    $<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>
    $<$<COMPILE_LANGUAGE:C>:-Wno-pointer-to-int-cast>
)