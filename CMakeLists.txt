cmake_minimum_required(VERSION 4.2)
project(VBANPlug VERSION 0.3.0 LANGUAGES C CXX)
# Keep the Windows release self-contained (no separate VC++ runtime installer).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(VBAND_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE" CACHE PATH "JUCE 8.0.13 source directory")
set(VBAND_JUCE_REVISION "7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2")
if(EXISTS "${VBAND_JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${VBAND_JUCE_DIR}" "${CMAKE_BINARY_DIR}/juce" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(juce
        URL "https://github.com/juce-framework/JUCE/archive/${VBAND_JUCE_REVISION}.zip"
        URL_HASH SHA256=6f438c9ee4559dffcc600dfee60adba37bc137bdad63a75ab7b09f4e9b1a647b)
    FetchContent_MakeAvailable(juce)
endif()

set(vban_formats VST3 Standalone)
if(APPLE)
    list(APPEND vban_formats AU)
endif()

foreach(direction IN ITEMS TX RX)
    if(direction STREQUAL "RX")
        set(plugin_code Vbrx)
        set(is_rx 1)
    else()
        set(plugin_code Vbtx)
        set(is_rx 0)
    endif()
    set(plugin_target VBANPlug${direction})
    juce_add_plugin(${plugin_target}
        COMPANY_NAME "ELKA"
        BUNDLE_ID "com.elka.vbanplug.${direction}"
        PLUGIN_MANUFACTURER_CODE Elka
        PLUGIN_CODE ${plugin_code}
        PRODUCT_NAME "VBAN Plug ${direction}"
        DESCRIPTION "VBAN audio ${direction}"
        FORMATS ${vban_formats}
        LOCAL_NETWORK_PERMISSION_ENABLED TRUE
        LOCAL_NETWORK_PERMISSION_TEXT "VBAN Plug sends and receives audio on your local network."
        MICROPHONE_PERMISSION_ENABLED TRUE
        MICROPHONE_PERMISSION_TEXT "VBAN Plug TX can send audio from your selected input device."
        IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE VST3_CATEGORIES Fx Network COPY_PLUGIN_AFTER_BUILD FALSE)
    target_sources(${plugin_target} PRIVATE
        Source/VbanProtocol.cpp Source/VbanProtocol.h Source/SpscQueue.h
        Source/NetworkEngine.cpp Source/NetworkEngine.h Source/ReceiveBuffer.h
        Source/PluginProcessor.cpp Source/PluginProcessor.h
        Source/PluginEditor.cpp Source/PluginEditor.h Source/PluginEntry.cpp
        Source/ChannelRouting.h Source/Metering.h Source/StereoMeter.h Source/StereoMeter.cpp)
    target_include_directories(${plugin_target} PUBLIC Source)
    target_compile_definitions(${plugin_target} PRIVATE VBAND_IS_RX=${is_rx})
    target_compile_definitions(${plugin_target} PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(${plugin_target} PRIVATE juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endforeach()
include(CTest)
if(BUILD_TESTING)
    juce_add_console_app(VBANPlugTests PRODUCT_NAME "VBAN Plug Tests")
    target_sources(VBANPlugTests PRIVATE Tests/Tests.cpp Tests/MultichannelTests.h)
    target_compile_definitions(VBANPlugTests PRIVATE JUCE_PLUGINHOST_VST3=1)
    target_link_libraries(VBANPlugTests PRIVATE VBANPlugTX juce::juce_audio_utils)
    add_dependencies(VBANPlugTests VBANPlugTX_VST3 VBANPlugRX_VST3)
    add_test(NAME VBANPlugTests COMMAND VBANPlugTests
        "${CMAKE_BINARY_DIR}/editor-previews"
        "${CMAKE_BINARY_DIR}/VBANPlugTX_artefacts/$<CONFIG>/VST3/VBAN Plug TX.vst3"
        "${CMAKE_BINARY_DIR}/VBANPlugRX_artefacts/$<CONFIG>/VST3/VBAN Plug RX.vst3")
endif()
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT VBANPlugTX_Standalone)
