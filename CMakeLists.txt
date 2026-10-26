cmake_minimum_required(VERSION 3.22)
project(ReSynth VERSION 0.5.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Install JUCE locally, then configure with -DJUCE_DIR=/path/to/JUCE
if (DEFINED JUCE_DIR)
  add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
else()
  find_package(JUCE CONFIG REQUIRED)
endif()
set(RESYNTH_FORMATS VST3)
if(APPLE)
  list(APPEND RESYNTH_FORMATS AU)
endif()
juce_add_plugin(ReSynth
  COMPANY_NAME "RE-SYNTH"
  BUNDLE_ID "com.resynth.instrument"
  PLUGIN_MANUFACTURER_CODE Rsyn
  PLUGIN_CODE Rs01
  FORMATS ${RESYNTH_FORMATS}
  PRODUCT_NAME "RE-SYNTH"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE
)
juce_generate_juce_header(ReSynth)
target_sources(ReSynth PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/SpectralModel.cpp
  Source/MasterColor.cpp
)
target_compile_definitions(ReSynth PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(ReSynth PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_audio_processors juce::juce_gui_extra PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags juce::juce_recommended_lto_flags)
