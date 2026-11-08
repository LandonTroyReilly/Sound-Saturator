cmake_minimum_required(VERSION 3.22)

# Apple Silicon only. Must be set before project(). Windows builds ignore this.
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Target macOS architecture" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

project(SoundSaturator VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.5
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(SoundSaturator
    COMPANY_NAME "Sound Saturator"
    BUNDLE_ID com.soundsaturator.prototype
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    # After a successful build, copy the VST3 to ~/Library/Audio/Plug-Ins/VST3.
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE SSat
    PLUGIN_CODE Ss01
    FORMATS VST3 Standalone
    PRODUCT_NAME "Sound Saturator")

target_sources(SoundSaturator
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_include_directories(SoundSaturator PRIVATE Source)

target_compile_definitions(SoundSaturator
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SoundSaturator
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

juce_add_console_app(SoundSaturatorTests PRODUCT_NAME "SoundSaturatorTests")

target_sources(SoundSaturatorTests
    PRIVATE
        tests/DspTests.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_include_directories(SoundSaturatorTests PRIVATE Source)

target_compile_definitions(SoundSaturatorTests
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

target_link_libraries(SoundSaturatorTests
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

if(APPLE)
    add_custom_target(apple-silicon-installer
        COMMAND ${CMAKE_COMMAND} -E env SOUND_SATURATOR_SKIP_BUILD=1 /bin/bash
                "${CMAKE_SOURCE_DIR}/installer/create-apple-silicon-pkg.sh"
        DEPENDS SoundSaturator_VST3
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Package and install the Apple Silicon VST3"
        VERBATIM)
endif()
