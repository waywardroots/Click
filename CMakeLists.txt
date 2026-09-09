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
# plugin target
# ---------------------------------------------------------------------------
# Audio Units are macOS only; VST3 and the standalone app build everywhere.
set(CLICK_FORMATS VST3 Standalone)

if(APPLE)
    list(APPEND CLICK_FORMATS AU)
endif()

juce_add_plugin(Click
    COMPANY_NAME            "Wayward Roots"
    BUNDLE_ID               "com.waywardroots.click"
    PLUGIN_MANUFACTURER_CODE Wayr
    PLUGIN_CODE             Clik
    FORMATS                 ${CLICK_FORMATS}
    PRODUCT_NAME            "Click"
    # An audio effect, not an instrument: hosts allow only one instrument per
    # track, so an effect is the only form that can be inserted after an
    # existing instrument. Click passes audio through and mixes itself on top.
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES         "Fx" "Tools")

target_sources(Click PRIVATE
    Source/ClickLibrary.cpp
    Source/ClickSlot.cpp
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

juce_add_binary_data(ClickAssets SOURCES ${CLICK_STOCK_SAMPLES})
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
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
