# Hush: the zero-latency DI de-noiser from dJONt and Parabola Bass Z as its
# own small plugin, for any guitar or bass DI (in front of an amp sim, or
# before a Quad Cortex chain). Its own project:
#
#   cmake -B build-hush -S hush -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-hush --config Release
#
# The engine and panel live in shared/ (Hush.h, HushPanel.*).
cmake_minimum_required(VERSION 3.22)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" HUSH_VERSION LIMIT_COUNT 1)
project(Hush VERSION ${HUSH_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(REPO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(LIB_DIR "${REPO_DIR}/libs")
include("${REPO_DIR}/cmake/cpm.cmake")
CPMAddPackage(
    NAME JUCE
    GIT_TAG 9.0.3
    VERSION 9.0.3
    GITHUB_REPOSITORY juce-framework/JUCE
    SOURCE_DIR ${LIB_DIR}/juce
)

option(HUSH_TESTS "Build the HushTests console app" OFF)

juce_add_binary_data(HushAssets
    NAMESPACE HushBinary
    HEADER_NAME HushBinary.h
    SOURCES assets/Anton-Regular.ttf
)
set_target_properties(HushAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(HUSH_SOURCES src/HushProcessor.cpp src/HushEditor.cpp ${REPO_DIR}/shared/HushPanel.cpp)
set(HUSH_INCLUDES src "${REPO_DIR}/shared")

juce_add_plugin(Hush
    COMPANY_NAME "dJONt"
    PLUGIN_NAME "Hush"
    PRODUCT_NAME "Hush"
    BUNDLE_ID "com.jonsaldivar.hush"
    PLUGIN_MANUFACTURER_CODE Djnt
    PLUGIN_CODE Hsh1
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Restoration
    FORMATS VST3 Standalone
    COPY_PLUGIN_AFTER_BUILD FALSE
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/icon/hush.png"
)
target_sources(Hush PRIVATE ${HUSH_SOURCES})
target_include_directories(Hush PRIVATE ${HUSH_INCLUDES})
target_compile_definitions(Hush PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(Hush
    PRIVATE HushAssets juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

if (WIN32)
    target_compile_definitions(Hush PUBLIC NOMINMAX)
    set(ASIO_SDK_PATH "" CACHE PATH "Path to the Steinberg ASIO SDK (enables JUCE_ASIO)")
    if (ASIO_SDK_PATH AND EXISTS "${ASIO_SDK_PATH}/common/iasiodrv.h")
        target_compile_definitions(Hush PUBLIC JUCE_ASIO=1)
        target_include_directories(Hush PUBLIC "${ASIO_SDK_PATH}/common")
    endif()
endif()

if (HUSH_TESTS)
    juce_add_console_app(HushTests PRODUCT_NAME "HushTests")
    target_sources(HushTests PRIVATE tests/HushTests.cpp ${HUSH_SOURCES})
    target_include_directories(HushTests PRIVATE ${HUSH_INCLUDES})
    target_compile_definitions(HushTests PRIVATE
        HUSH_NO_PLUGIN_ENTRY=1 NOMINMAX=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(HushTests
        PRIVATE HushAssets juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
