cmake_minimum_required(VERSION 3.22)

project(YokokoOnonoPitchDelayV4 VERSION 4.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use a local JUCE checkout if one is given (cmake -DJUCE_DIR=...),
# otherwise download JUCE from GitHub so there is nothing to install by hand.
if(DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_DIR}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.5
        GIT_SHALLOW    ON
    )
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(YokokoOnonoPitchDelayV4
    COMPANY_NAME              "Yokoko Onono"
    PLUGIN_MANUFACTURER_CODE  Ykko
    PLUGIN_CODE               Ykp4
    FORMATS                   VST3 Standalone
    PRODUCT_NAME              "Yokoko Onono Pitch Delay v4"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           Fx Delay Pitch Shift
)

juce_add_binary_data(YokokokoFonts SOURCES
    Resources/CourierPrime-Regular.ttf
    Resources/CourierPrime-Bold.ttf
    Resources/CourierPrime-Italic.ttf
)

set(YOKO_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_sources(YokokoOnonoPitchDelayV4 PRIVATE ${YOKO_SOURCES})

target_compile_definitions(YokokoOnonoPitchDelayV4 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(YokokoOnonoPitchDelayV4
    PRIVATE
        YokokokoFonts
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Optional offline test tool (renders test audio and a screenshot of the GUI).
option(YOKO_BUILD_TESTS "Build the offline render test" OFF)
if(YOKO_BUILD_TESTS)
    juce_add_console_app(YokoRenderTest PRODUCT_NAME "YokoRenderTest")
    target_sources(YokoRenderTest PRIVATE tools/RenderTest.cpp ${YOKO_SOURCES})
    target_include_directories(YokoRenderTest PRIVATE Source)
    target_compile_definitions(YokoRenderTest PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 YOKO_TEST_BUILD=1)
    target_link_libraries(YokoRenderTest PRIVATE
        YokokokoFonts juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()

if(YOKO_BUILD_TESTS)
    juce_add_console_app(YokoIdleTest PRODUCT_NAME "YokoIdleTest")
    target_sources(YokoIdleTest PRIVATE tools/IdleTest.cpp ${YOKO_SOURCES})
    target_include_directories(YokoIdleTest PRIVATE Source)
    target_compile_definitions(YokoIdleTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 YOKO_TEST_BUILD=1)
    target_link_libraries(YokoIdleTest PRIVATE
        YokokokoFonts juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
