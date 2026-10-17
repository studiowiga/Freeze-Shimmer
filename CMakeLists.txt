cmake_minimum_required(VERSION 3.22)
# macOS : binaire universel (Apple Silicon + Intel). Doit être défini AVANT project().
if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Architectures macOS")
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "macOS minimum")
endif()

project(FreezeShimmer VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(FS_COPY_AFTER_BUILD "Copie le VST3 dans le dossier VST3 système après la compilation (droits admin sous Windows ; ~/Library/Audio/Plug-Ins/VST3 sous macOS)" OFF)
option(FS_BUILD_TESTS      "Compile le test DSP hors JUCE" OFF)

# --- JUCE (téléchargé automatiquement, ou -DFETCHCONTENT_SOURCE_DIR_JUCE=<chemin local>) ---
include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG        8.0.12
  GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

if(WIN32 OR APPLE)
  set(FS_FORMATS VST3 Standalone)
else()
  set(FS_FORMATS VST3)
endif()

juce_add_plugin(FreezeShimmer
  COMPANY_NAME               "Studio Wiga"
  BUNDLE_ID                  com.studiowiga.freezeshimmer
  PRODUCT_NAME               "Freeze Shimmer"
  PLUGIN_MANUFACTURER_CODE   Wiga
  PLUGIN_CODE                Fshm
  FORMATS                    ${FS_FORMATS}
  VST3_CATEGORIES            Fx Reverb
  IS_SYNTH                   FALSE
  NEEDS_MIDI_INPUT           TRUE
  NEEDS_MIDI_OUTPUT          FALSE
  IS_MIDI_EFFECT             FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD    ${FS_COPY_AFTER_BUILD})

juce_add_binary_data(FreezeShimmerAssets SOURCES Assets/background.jpg)

target_sources(FreezeShimmer PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginProcessor.h
  Source/PluginEditor.cpp
  Source/PluginEditor.h
  Source/Params.h
  Source/dsp/Util.h
  Source/dsp/FFT.h
  Source/dsp/PitchShifter.h
  Source/dsp/Reverb.h
  Source/dsp/ShimmerEngine.h
  Source/dsp/FreezeEngine.h)

target_compile_definitions(FreezeShimmer PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_REPORT_APP_USAGE=0
  JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(FreezeShimmer
  PRIVATE
    FreezeShimmerAssets
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

if(FS_BUILD_TESTS)
  add_executable(fs_dsp_test tests/dsp_test.cpp)
  target_include_directories(fs_dsp_test PRIVATE Source/dsp)
endif()
