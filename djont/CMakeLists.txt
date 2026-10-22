# dJONt: a low-end toolkit plugin (Tight / Grit / Sub / Lo-Fi) for the TK3J
# fork. Its own project, built separately from the TONE3000/TK3J plugin:
#
#   cmake -B build -S djont -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --config Release
#
# Shares the repo's JUCE checkout (libs/juce, same version as the main
# build) and T3KJ's pitch shifter (plugin/src/PitchShift.cpp) for the Sub
# module's CLEAN mode.
cmake_minimum_required(VERSION 3.22)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" DJONT_VERSION LIMIT_COUNT 1)
project(dJONt VERSION ${DJONT_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(REPO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(LIB_DIR "${REPO_DIR}/libs")
include("${REPO_DIR}/cmake/cpm.cmake")  # fetches CPM into libs/cpm (gitignored) when missing
CPMAddPackage(
    NAME JUCE
    GIT_TAG 9.0.3
    VERSION 9.0.3
    GITHUB_REPOSITORY juce-framework/JUCE
    SOURCE_DIR ${LIB_DIR}/juce
)

option(DJONT_TESTS "Build the DjontTests console app" OFF)

juce_add_binary_data(DjontAssets
    NAMESPACE DjontBinary
    HEADER_NAME DjontBinary.h
    SOURCES assets/Anton-Regular.ttf assets/djont-chrome.jpg assets/djont-redfoil.jpg
)
set_target_properties(DjontAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(DJONT_SOURCES
    src/DjontProcessor.cpp
    src/DjontEditor.cpp
    ${REPO_DIR}/plugin/src/PitchShift.cpp
)

juce_add_plugin(dJONt
    COMPANY_NAME "dJONt"
    PLUGIN_NAME "dJONt"
    PRODUCT_NAME "dJONt"
    BUNDLE_ID "com.jonsaldivar.djont"
    PLUGIN_MANUFACTURER_CODE Djnt
    PLUGIN_CODE Djn1
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Distortion
    FORMATS VST3 Standalone
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(dJONt PRIVATE ${DJONT_SOURCES})
target_include_directories(dJONt PRIVATE src "${REPO_DIR}/plugin/include")
target_compile_definitions(dJONt PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)
target_link_libraries(dJONt
    PRIVATE DjontAssets juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags
)

# ASIO in the standalone's device list (Steinberg SDK headers, fetched by CI;
# without them the standalone still builds, just without ASIO).
if (WIN32)
    target_compile_definitions(dJONt PUBLIC NOMINMAX)
    set(ASIO_SDK_PATH "" CACHE PATH "Path to the Steinberg ASIO SDK (enables JUCE_ASIO)")
    if (ASIO_SDK_PATH AND EXISTS "${ASIO_SDK_PATH}/common/iasiodrv.h")
        target_compile_definitions(dJONt PUBLIC JUCE_ASIO=1)
        target_include_directories(dJONt PUBLIC "${ASIO_SDK_PATH}/common")
    endif()
endif()

if (DJONT_TESTS)
    juce_add_console_app(DjontTests PRODUCT_NAME "DjontTests")
    target_sources(DjontTests PRIVATE tests/DjontTests.cpp ${DJONT_SOURCES})
    target_include_directories(DjontTests PRIVATE src "${REPO_DIR}/plugin/include")
    target_compile_definitions(DjontTests PRIVATE
        DJONT_NO_PLUGIN_ENTRY=1 NOMINMAX=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(DjontTests
        PRIVATE DjontAssets juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
