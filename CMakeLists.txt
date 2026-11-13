cmake_minimum_required(VERSION 3.22)
project(kojiverb VERSION 1.0.0 LANGUAGES C CXX)
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 9.0.5 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)
juce_add_plugin(kojiverb
  COMPANY_NAME "kojiverb Audio" BUNDLE_ID com.kojiverb.audio.kojiverb
  PLUGIN_MANUFACTURER_CODE KJvr PLUGIN_CODE Kjvb PRODUCT_NAME "kojiverb"
  FORMATS VST3 IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE EDITOR_WANTS_KEYBOARD_FOCUS FALSE COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Fx Reverb Delay Distortion)
juce_generate_juce_header(kojiverb)
target_sources(kojiverb PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_features(kojiverb PRIVATE cxx_std_17)
target_compile_definitions(kojiverb PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(kojiverb PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
