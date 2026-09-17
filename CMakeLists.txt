cmake_minimum_required(VERSION 3.22)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" PULSE_DESIGNER_VERSION)
string(STRIP "${PULSE_DESIGNER_VERSION}" PULSE_DESIGNER_VERSION)
if(NOT PULSE_DESIGNER_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR
        "VERSION must be major.minor.patch, got '${PULSE_DESIGNER_VERSION}'")
endif()

project(PulseDesigner
    VERSION ${PULSE_DESIGNER_VERSION}
    DESCRIPTION "MIDI drum synthesizer plugin"
    LANGUAGES C CXX)

option(PULSE_DESIGNER_COPY_PLUGINS
    "Copy built plugins into the current user's standard plugin folders"
    ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

include(cmake/Dependencies.cmake)

add_subdirectory(src/dsp)
add_subdirectory(src/plugin)

enable_testing()
add_subdirectory(tests)
