add_library(pulse_dsp STATIC
    DcBlocker.cpp
    Envelope.cpp
    NonlinearStage.cpp
    OneShotRenderer.cpp
    NoiseGenerator.cpp
    Oscillator.cpp
    SynthEngine.cpp
    TptStateVariableFilter.cpp)

target_include_directories(pulse_dsp
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src)

target_compile_features(pulse_dsp PUBLIC cxx_std_20)

target_compile_options(pulse_dsp
    PRIVATE
        $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>)
