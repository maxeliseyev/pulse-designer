add_library(pulse_dsp STATIC
    Envelope.cpp
    OneShotRenderer.cpp
    Oscillator.cpp
    SynthEngine.cpp)

target_include_directories(pulse_dsp
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src)

target_compile_features(pulse_dsp PUBLIC cxx_std_20)

target_compile_options(pulse_dsp
    PRIVATE
        $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>)
