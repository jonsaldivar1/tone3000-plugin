# TONE3000 DSP test suite.
# A GTest console app exercising the chain's DSP invariants (oversampler,
# NamEngine phase interleaving, IR base-rate islands) against the real model
# and IR assets in test/files. Build and run locally:
#
#   ./script/test-dsp.sh                     # configure/build/run in one go
#   ./script/test-dsp.sh 'IrConvolutionTest.*'   # gtest filter
#
# or by hand: cmake --build build --target DspTests, then run the binary in
# build/test/DspTests_artefacts (also registered with ctest).

# The root project only enables CXX; the JUCE modules this app pulls in
# (juce_graphics via juce_audio_processors) include a C source.
enable_language(C)

# Touch gesture state needs neither the DSP nor a GUI/display server.
add_executable(EqPinchTests ui/eq_pinch_tests.cpp)
target_include_directories(EqPinchTests PRIVATE ../plugin/ui)
add_test(NAME EqPinchTests COMMAND EqPinchTests)

juce_add_console_app(DspTests PRODUCT_NAME "DspTests")

target_sources(DspTests PRIVATE
    src/auto_offset_tests.cpp
    src/branch_tests.cpp
    src/clipboard_tests.cpp
    src/dsp_tests.cpp
    src/dual_mono_tests.cpp
    src/duplicate_tests.cpp
    src/eq_chain_rate_tests.cpp
    src/eq_post_routing_tests.cpp
    src/factory_preset_tests.cpp
    src/gate_tests.cpp
    src/host_program_tests.cpp
    src/ir_block_size_tests.cpp
    src/ir_classification_tests.cpp
    src/ir_temp_file_tests.cpp
    src/local_load_tests.cpp
    src/midi_map_tests.cpp
    src/multicore_tests.cpp
    src/outgain_tests.cpp
    src/padless_wav_tests.cpp
    src/pitch_shift_tests.cpp
    src/preset_tests.cpp
    src/processor_tests.cpp
    src/scene_tests.cpp
    src/split_strip_tests.cpp
    src/spread_tests.cpp
    src/state_cache_tests.cpp
    src/swap_fade_tests.cpp
    src/tone_refresh_tests.cpp
    src/tuner_tests.cpp
    src/worker_pool_tests.cpp
    # Compiled straight from the plugin tree so the tests exercise the real
    # processor, not a copy. GUI sources are excluded; HEADLESS=1 below
    # compiles the processor exactly like the headless plugin build.
    ../t3kj/src/AutoOffset.cpp
    ../t3kj/src/BlockEq.cpp
    ../t3kj/src/BlockSpectrum.cpp
    ../t3kj/src/LegacyParamIds.cpp
    ../t3kj/src/MidiMapper.cpp
    ../t3kj/src/NamEngine.cpp
    ../t3kj/src/PresetFile.cpp
    ../t3kj/src/PresetManager.cpp
    ../t3kj/src/Processor.cpp
    ../t3kj/src/ProcessorChain.cpp
    ../t3kj/src/ProcessorHistory.cpp
    ../t3kj/src/ProcessorModelLoader.cpp
    ../t3kj/src/ProcessorPresets.cpp
    ../t3kj/src/ProcessorScenes.cpp
    ../t3kj/src/ProcessorState.cpp
    ../t3kj/src/Spread.cpp
    # Processor.cpp's constructor calls StandaloneStateAutosave::install().
    # Always the portable no-op stub here, never the .mm: the real observer
    # is iOS-standalone-only and this suite never runs there (the stub is
    # also what the plugin itself compiles to everywhere but iOS).
    ../t3kj/src/StandaloneStateAutosave.cpp
    ../t3kj/src/PitchShift.cpp
    ../t3kj/src/StereoOffset.cpp
    ../t3kj/src/TunerDetector.cpp
)

target_include_directories(DspTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../t3kj/include
    # ChainDomain.h pulls in the AudioDSPTools resampling boundary.
    ${CMAKE_CURRENT_SOURCE_DIR}/../t3kj/AudioDSPTools
    ${CMAKE_CURRENT_SOURCE_DIR}/../t3kj/AudioDSPTools/dsp/ResamplingContainer/Dependencies
    # NAM core + deps. Added explicitly (not via the NAM target) because on
    # macOS/Windows NAM is linked as a whole-archive *flag*, which doesn't
    # propagate the target's include directories.
    ${CMAKE_CURRENT_SOURCE_DIR}/../t3kj/NeuralAmpModelerCore
    ${CMAKE_CURRENT_SOURCE_DIR}/../t3kj/NeuralAmpModelerCore/Dependencies/eigen
    ${CMAKE_CURRENT_SOURCE_DIR}/../t3kj/NeuralAmpModelerCore/Dependencies/nlohmann
)

target_compile_definitions(DspTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_WEBP=0
    JUCE_USE_CURL=0
    # Prevent windows.h min/max macros from breaking std::min/std::max in
    # AudioDSPTools and plugin code (same as t3kj/CMakeLists.txt).
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
    # The processor compiles in its headless configuration (no editor /
    # standalone-window code); the DSP under test is identical.
    HEADLESS=1
    # midi_map_tests pumps the dispatch loop (runDispatchLoopUntil) to
    # deliver the MidiMapper's AsyncUpdater work; the plugin itself never
    # needs modal loops.
    JUCE_MODAL_LOOPS_PERMITTED=1
    # Absolute path to the test assets, so the binary runs from anywhere.
    T3K_TEST_FILES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/files"
    # Exposes is_a2_shape, which ProcessorModelLoader.cpp uses to gate
    # drop-loaded local NAM files (same as t3kj/CMakeLists.txt).
    NAM_ENABLE_A2_FAST
)

target_link_libraries(DspTests
    PRIVATE
        juce::juce_dsp
        juce::juce_audio_formats
        juce::juce_audio_processors
        # NAM must be linked whole-archive so its self-registering config
        # parsers (WaveNet/ConvNet/LSTM static initializers) survive the
        # linker (same recipe as t3kj/CMakeLists.txt).
        $<$<PLATFORM_ID:Darwin>:-Wl,-force_load,$<TARGET_FILE:NAM>>
        $<$<PLATFORM_ID:Linux>:-Wl,--whole-archive>
        $<$<PLATFORM_ID:Linux>:NAM>
        $<$<PLATFORM_ID:Linux>:-Wl,--no-whole-archive>
        $<$<PLATFORM_ID:Windows>:NAM>
        $<$<PLATFORM_ID:Windows>:-WHOLEARCHIVE:$<TARGET_FILE:NAM>>
        # dsp_tests.cpp provides main() (JUCE init + InitGoogleTest).
        GTest::gtest
    PUBLIC
        juce::juce_recommended_config_flags
)

add_dependencies(DspTests NAM)

include(GoogleTest)
gtest_discover_tests(DspTests)
