cmake_minimum_required(VERSION 3.22)
project(VYRS_PLUGIN VERSION 0.1.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS")
# JUCE lives outside the repo (see README); override with -DJUCE_DIR=...
set(JUCE_DIR "$ENV{HOME}/Projects/sdks/JUCE" CACHE PATH "Path to JUCE 8")
add_subdirectory(${JUCE_DIR} JUCE)

juce_add_plugin(VYRS
    COMPANY_NAME "CloudFreaks"
    COMPANY_WEBSITE "https://cloudfreaks.nl"
    COMPANY_EMAIL "support@cloudfreaks.nl"
    BUNDLE_ID nl.cloudfreaks.vyrs.plugin
    PLUGIN_MANUFACTURER_CODE Clfr
    PLUGIN_CODE Vyrs
    PRODUCT_NAME "VYRS"
    DESCRIPTION "Your VYRS songs, takes and stems inside your DAW"
    FORMATS AU VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Tools
    AU_MAIN_TYPE kAudioUnitType_Effect
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "VYRS records your vocal so you can put it on a take.")

target_sources(VYRS PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/VyrsApi.cpp)

target_compile_definitions(VYRS PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:VYRS,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:VYRS,JUCE_VERSION>")

target_link_libraries(VYRS
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_cryptography
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
