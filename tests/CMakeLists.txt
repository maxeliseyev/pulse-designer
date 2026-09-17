add_executable(pulse_tests
    test_synth_engine.cpp)

target_link_libraries(pulse_tests
    PRIVATE
        pulse_dsp
        Catch2::Catch2WithMain)

target_include_directories(pulse_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src)

target_compile_features(pulse_tests PRIVATE cxx_std_20)

add_test(NAME pulse_tests COMMAND pulse_tests)

add_executable(pulse_plugin_tests
    test_plugin.cpp)

target_link_libraries(pulse_plugin_tests
    PRIVATE
        PulseDesigner
        juce::juce_audio_utils
        Catch2::Catch2WithMain)

target_include_directories(pulse_plugin_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/plugin)

target_compile_features(pulse_plugin_tests PRIVATE cxx_std_20)

add_test(NAME pulse_plugin_tests COMMAND pulse_plugin_tests)
