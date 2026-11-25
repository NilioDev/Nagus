cmake_minimum_required(VERSION 3.22)

project(Nagus
    VERSION 0.1.0
    DESCRIPTION "Clean-room engine that plays the 1999 space strategy game from your own files"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(NGS_BUILD_APP "Build the game executable (needs SDL3)" ON)
option(NGS_BUILD_TESTS "Build the unit tests" ${PROJECT_IS_TOP_LEVEL})
option(NGS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(NgsWarnings)

# Engine core: platform independent, no SDL. Everything that can be tested
# without a screen lives here.
add_subdirectory(src/core)

if(NGS_BUILD_APP)
    include(NgsSDL3)
    add_subdirectory(src/app)
endif()

if(NGS_BUILD_TESTS AND NOT ANDROID)
    enable_testing()
    add_subdirectory(tests)
endif()
