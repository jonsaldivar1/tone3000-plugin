# Parabola Bass Z: a 3-band parallel bass processor (a crossover with a NAM
# amp capture per band, a cab IR on the upper bands, a graphic EQ). Its own
# project, built separately from T3KJ and dJONt:
#
#   cmake -B build-parabola -S parabola -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-parabola --config Release
#
# Shares the repo's JUCE checkout (libs/juce), T3KJ's NeuralAmpModelerCore
# submodule (compiled here into its own static library, untouched) and the
# Lanczos resampler from AudioDSPTools.
cmake_minimum_required(VERSION 3.24)  # $<LINK_LIBRARY:WHOLE_ARCHIVE>

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" PARABOLA_VERSION LIMIT_COUNT 1)
project(ParabolaBassZ VERSION ${PARABOLA_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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

option(PARABOLA_TESTS "Build the ParabolaTests console app" OFF)

# ---- NAM (NeuralAmpModelerCore), same sources and flags T3KJ builds.
set(NAM_DIR "${REPO_DIR}/plugin/NeuralAmpModelerCore")
set(NAM_SRC "${NAM_DIR}/NAM")
add_library(ParabolaNAM STATIC
    ${NAM_SRC}/activations.cpp
    ${NAM_SRC}/conv1d.cpp
    ${NAM_SRC}/container.cpp
    ${NAM_SRC}/convnet.cpp
    ${NAM_SRC}/dsp.cpp
    ${NAM_SRC}/get_dsp.cpp
    ${NAM_SRC}/lstm.cpp
    ${NAM_SRC}/ring_buffer.cpp
    ${NAM_SRC}/util.cpp
    ${NAM_SRC}/wavenet/a2_fast.cpp
    ${NAM_SRC}/wavenet/model.cpp
    ${NAM_SRC}/wavenet/slimmable.cpp
)
target_include_directories(ParabolaNAM PUBLIC
    ${NAM_DIR}
    ${NAM_DIR}/Dependencies/eigen
    ${NAM_DIR}/Dependencies/nlohmann
)
target_compile_definitions(ParabolaNAM PRIVATE NAM_ENABLE_A2_FAST)
set_target_properties(ParabolaNAM PROPERTIES
    C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN TRUE)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ParabolaNAM PRIVATE -fno-gnu-unique)
endif()
if (MSVC)
    target_compile_options(ParabolaNAM PRIVATE /bigobj)
    target_compile_definitions(ParabolaNAM PUBLIC NOMINMAX _USE_MATH_DEFINES)
endif()
# The parser registrations (WaveNet, LSTM, SlimmableContainer, ...) are
# static initialisers in otherwise unreferenced objects: link it whole.
set(PARABOLA_NAM_LINK "$<LINK_LIBRARY:WHOLE_ARCHIVE,ParabolaNAM>")

juce_add_binary_data(ParabolaAssets
    NAMESPACE ParabolaBinary
    HEADER_NAME ParabolaBinary.h
    SOURCES
        assets/Anton-Regular.ttf
        assets/skin-foil.jpg assets/skin-inferno.jpg assets/skin-hologram.jpg assets/skin-ram.jpg assets/skin-ember.jpg
        manual/manual.json
)

set(PARABOLA_SOURCES
    src/ParabolaProcessor.cpp
    src/ParabolaEditor.cpp
    src/NamSlot.cpp
    src/Presets.cpp
    ${REPO_DIR}/shared/HushPanel.cpp
    ${REPO_DIR}/plugin/src/PitchShift.cpp
    ${REPO_DIR}/plugin/src/TunerDetector.cpp
)
set(PARABOLA_INCLUDES src "${REPO_DIR}/plugin/AudioDSPTools" "${REPO_DIR}/plugin/include" "${REPO_DIR}/shared")

juce_add_plugin(ParabolaBassZ
    COMPANY_NAME "Parabola Bass Z"
    PLUGIN_NAME "Parabola Bass Z"
    PRODUCT_NAME "Parabola Bass Z"
    BUNDLE_ID "com.jonsaldivar.parabolabassz"
    PLUGIN_MANUFACTURER_CODE Djnt
    PLUGIN_CODE Pbz1
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Distortion
    FORMATS VST3 Standalone
    COPY_PLUGIN_AFTER_BUILD FALSE
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/icon/parabola.png"
)

set(PARABOLA_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_sources(ParabolaBassZ PRIVATE ${PARABOLA_SOURCES})
target_include_directories(ParabolaBassZ PRIVATE ${PARABOLA_INCLUDES})
target_compile_definitions(ParabolaBassZ PUBLIC ${PARABOLA_DEFS})
target_link_libraries(ParabolaBassZ
    PRIVATE ParabolaAssets ${PARABOLA_NAM_LINK} juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags
)

if (WIN32)
    target_compile_definitions(ParabolaBassZ PUBLIC NOMINMAX)
    set(ASIO_SDK_PATH "" CACHE PATH "Path to the Steinberg ASIO SDK (enables JUCE_ASIO)")
    if (ASIO_SDK_PATH AND EXISTS "${ASIO_SDK_PATH}/common/iasiodrv.h")
        target_compile_definitions(ParabolaBassZ PUBLIC JUCE_ASIO=1)
        target_include_directories(ParabolaBassZ PUBLIC "${ASIO_SDK_PATH}/common")
    endif()
endif()

if (PARABOLA_TESTS)
    juce_add_console_app(ParabolaTests PRODUCT_NAME "ParabolaTests")
    target_sources(ParabolaTests PRIVATE tests/ParabolaTests.cpp ${PARABOLA_SOURCES})
    target_include_directories(ParabolaTests PRIVATE ${PARABOLA_INCLUDES})
    target_compile_definitions(ParabolaTests PRIVATE
        PARABOLA_NO_PLUGIN_ENTRY=1 NOMINMAX=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(ParabolaTests
        PRIVATE ParabolaAssets ${PARABOLA_NAM_LINK} juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
