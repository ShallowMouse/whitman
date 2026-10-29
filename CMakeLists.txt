cmake_minimum_required(VERSION 3.22)

# The build sets WHITMAN_VERSION (1.0.<build number>); WHITMAN compares it with the latest release to offer updates.
set(WHITMAN_VERSION "1.0.0" CACHE STRING "Version of this build")
project(WHITMAN VERSION ${WHITMAN_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime: the .exe runs without installing the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(WHITMAN_BUILD_APP   "Build the WHITMAN application" ON)
option(WHITMAN_BUILD_TESTS "Build the headless engine tests" OFF)

# ---------------------------------------------------------------------------
# JUCE: use a local checkout if JUCE_DIR is given, otherwise fetch it.
# ---------------------------------------------------------------------------
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")

if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.15
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(ENGINE_SOURCES
    Source/MixEngine.cpp
    Source/MixState.cpp
    Source/RouteStatus.cpp)

set(COMMON_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

juce_add_binary_data(WhitmanFonts SOURCES
    Resources/Fonts/CourierPrime-Regular.ttf
    Resources/Fonts/CourierPrime-Bold.ttf
    Resources/Fonts/Roboto-Regular.ttf
    Resources/Fonts/Roboto-Bold.ttf)

if(WHITMAN_BUILD_APP)
    juce_add_gui_app(Whitman
        PRODUCT_NAME "WHITMAN"
        COMPANY_NAME "PEQUOD"
        BUNDLE_ID    "com.pequod.whitman"
        ICON_BIG     "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Icon/WHITMAN.png"
        ICON_SMALL   "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Icon/WHITMAN-small.png"
        VERSION      ${PROJECT_VERSION})

    juce_generate_juce_header(Whitman)

    target_sources(Whitman PRIVATE
        ${ENGINE_SOURCES}
        Source/Main.cpp
        Source/MainComponent.cpp
        Source/Console.cpp
        Source/Updater.cpp)

    target_compile_definitions(Whitman PRIVATE ${COMMON_DEFS}
        JUCE_ASIO=1
        JUCE_WASAPI=1
        JUCE_DIRECTSOUND=1
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Whitman,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Whitman,JUCE_VERSION>")

    target_link_libraries(Whitman
        PRIVATE
            WhitmanFonts
            juce::juce_audio_utils
            juce::juce_audio_devices
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()

# ---------------------------------------------------------------------------
# Headless tests: routing from Hawthorne Route's channel names, the mix maths,
# mix files, and (on Windows) reading Hawthorne Route's shared memory.
# ---------------------------------------------------------------------------
if(WHITMAN_BUILD_TESTS)
    juce_add_console_app(WhitmanTests PRODUCT_NAME "WhitmanTests")
    juce_generate_juce_header(WhitmanTests)
    target_sources(WhitmanTests PRIVATE ${ENGINE_SOURCES} Tests/WhitmanTests.cpp)
    target_compile_definitions(WhitmanTests PRIVATE ${COMMON_DEFS})
    target_link_libraries(WhitmanTests PRIVATE
        juce::juce_audio_basics
        juce::juce_core
        juce::juce_recommended_config_flags)
endif()
