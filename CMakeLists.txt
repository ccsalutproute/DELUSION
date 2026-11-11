cmake_minimum_required(VERSION 3.22)
project(Delusion VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

if(DEFINED ENV{GITHUB_ACTIONS})
    set(COPY_AFTER FALSE)
else()
    set(COPY_AFTER TRUE)
endif()

juce_add_plugin(Delusion
    COMPANY_NAME "MonStudio"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Mstu
    PLUGIN_CODE Dlsn
    FORMATS VST3 Standalone
    COPY_PLUGIN_AFTER_BUILD ${COPY_AFTER}
    PRODUCT_NAME "Delusion"
    BUNDLE_ID com.monstudio.delusion)

target_sources(Delusion PRIVATE
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp)

target_compile_definitions(Delusion PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Delusion
    PRIVATE juce::juce_audio_utils
            juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags)
