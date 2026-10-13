cmake_minimum_required(VERSION 3.22)

project(Click VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if(APPLE)
    # Ship a single binary that runs natively on both Apple silicon and Intel.
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS build architectures")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum supported macOS version")
endif()

# ---------------------------------------------------------------------------
# JUCE
#
# Point CLICK_JUCE_PATH at a local JUCE checkout to build without network
# access, otherwise the correct version is fetched automatically.
# ---------------------------------------------------------------------------
set(CLICK_JUCE_PATH "" CACHE PATH "Path to an existing JUCE checkout")

if(CLICK_JUCE_PATH)
    message(STATUS "Click: using JUCE from ${CLICK_JUCE_PATH}")
    add_subdirectory(${CLICK_JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# licensing
#
# Offline, paste-only key unlocking: the website signs a key with the private
# half of this product's RSA pair, the plugin verifies it with the public half
# embedded in PluginProcessor.cpp. Nothing is contacted at runtime.
# ---------------------------------------------------------------------------
juce_add_module("${CMAKE_CURRENT_SOURCE_DIR}/modules/fuzzyaudio_licensing")

# ---------------------------------------------------------------------------
# plugin target
# ---------------------------------------------------------------------------
# Audio Units are macOS only; VST3 and the standalone app build everywhere.
set(CLICK_FORMATS VST3 Standalone)

if(APPLE)
    list(APPEND CLICK_FORMATS AU)
endif()

juce_add_plugin(Click
    COMPANY_NAME            "Fuzzy Audio"
    BUNDLE_ID               "com.fuzzyaudio.click"
    PLUGIN_MANUFACTURER_CODE Fuzz
    PLUGIN_CODE             Clik
    FORMATS                 ${CLICK_FORMATS}
    PRODUCT_NAME            "Click"
    # Live only offers instruments on MIDI tracks, and refuses to instantiate
    # one that declares no event input bus. Click ignores incoming MIDI.
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES         "Instrument" "Tools")

target_sources(Click PRIVATE
    Source/ClickLibrary.cpp
    Source/ClickSlot.cpp
    Source/EnvelopeDisplay.cpp
    Source/Parameters.cpp
    Source/PluginEditor.cpp
    Source/PluginProcessor.cpp
    Source/StepGrid.cpp
    Source/WaveformDisplay.cpp)

# The ten stock clicks are compiled into the binary, so the plugin has no
# runtime file dependencies. Regenerate them with tools/generate_clicks.py.
file(GLOB CLICK_STOCK_SAMPLES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/resources/clicks/*.wav")

if(NOT CLICK_STOCK_SAMPLES)
    message(FATAL_ERROR
        "No stock clicks found in resources/clicks. "
        "Run: python3 tools/generate_clicks.py")
endif()

juce_add_binary_data(ClickAssets SOURCES
    ${CLICK_STOCK_SAMPLES}
    "${CMAKE_CURRENT_SOURCE_DIR}/logo.png")
set_target_properties(ClickAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_compile_definitions(Click PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(Click PRIVATE
    ClickAssets
    juce::juce_audio_utils
    fuzzyaudio_licensing
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# key generator
#
# Off by default: it is a seller's tool, not part of the plugin. Build it to
# make this product's key pair, and to sign and verify test keys.
#
#   cmake -B build -DBUILD_KEYGEN=ON && cmake --build build --target Keygen
# ---------------------------------------------------------------------------
option(BUILD_KEYGEN "Build the license key generator CLI" OFF)

if(BUILD_KEYGEN)
    juce_add_console_app(Keygen PRODUCT_NAME "keygen")
    target_sources(Keygen PRIVATE tools/keygen/keygen.cpp)
    target_compile_definitions(Keygen PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(Keygen PRIVATE
        juce::juce_product_unlocking
        juce::juce_data_structures   # OnlineUnlockStatus is compiled out without it
        juce::juce_recommended_config_flags)
endif()
