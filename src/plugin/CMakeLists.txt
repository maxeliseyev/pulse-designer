juce_add_plugin(PulseDesigner
    VERSION ${PROJECT_VERSION}
    HARDENED_RUNTIME_ENABLED TRUE
    COMPANY_NAME "Pulse Designer"
    BUNDLE_ID com.pulsedesigner.instrument
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${PULSE_DESIGNER_COPY_PLUGINS}
    PLUGIN_MANUFACTURER_CODE Puls
    PLUGIN_CODE Pdsn
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Pulse Designer"
    VST3_CATEGORIES Instrument
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

target_sources(PulseDesigner
    PRIVATE
        Parameters.cpp
        PluginProcessor.cpp
        PluginEditor.cpp)

target_compile_definitions(PulseDesigner
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_REPORT_APP_USAGE=0)

target_link_libraries(PulseDesigner
    PRIVATE
        pulse_dsp
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

target_include_directories(PulseDesigner
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src)

target_compile_features(PulseDesigner PRIVATE cxx_std_20)
