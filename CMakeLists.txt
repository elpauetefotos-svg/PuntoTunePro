cmake_minimum_required(VERSION 3.22)

# macOS: binario universal (Apple Silicon + Intel). Debe ir antes de project().
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
endif()

# Windows: runtime estático (el usuario no necesita instalar el Visual C++ Redistributable)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "")
endif()

project(PuntoTunePro VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

set(PLUGIN_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PLUGIN_FORMATS AU)
endif()

juce_add_plugin(PuntoTunePro
    COMPANY_NAME                "Punto"
    PRODUCT_NAME                "Punto Tune Pro"
    BUNDLE_ID                   com.punto.puntotunepro
    PLUGIN_MANUFACTURER_CODE    Pnto
    PLUGIN_CODE                 Ptpr
    FORMATS                     ${PLUGIN_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx "Pitch Shift"
    AU_MAIN_TYPE                kAudioUnitType_Effect
    VERSION                     ${PROJECT_VERSION})

target_sources(PuntoTunePro PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(PuntoTunePro PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

if(MSVC)
    target_compile_options(PuntoTunePro PRIVATE /utf-8)
endif()

target_link_libraries(PuntoTunePro
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Prueba del motor DSP sin JUCE:  cmake --build build --target dsp_test
add_executable(dsp_test EXCLUDE_FROM_ALL tests/dsp_test.cpp)
target_include_directories(dsp_test PRIVATE Source/dsp)

# Test de humo del plugin completo + capturas de la interfaz (opcional):
#   cmake -B build -DPUNTO_BUILD_TESTS=ON && cmake --build build --target PuntoSmoke
option(PUNTO_BUILD_TESTS "Compilar tests" OFF)
if(PUNTO_BUILD_TESTS)
    juce_add_console_app(PuntoSmoke PRODUCT_NAME "smoke")
    target_sources(PuntoSmoke PRIVATE tests/smoke_test.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(PuntoSmoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    if(MSVC)
        target_compile_options(PuntoSmoke PRIVATE /utf-8)
    endif()
    target_link_libraries(PuntoSmoke PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
