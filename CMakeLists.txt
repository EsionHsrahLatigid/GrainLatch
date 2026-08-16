cmake_minimum_required(VERSION 3.22)

project(GrainLatch VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GRAINLATCH_BUILD_PLUGIN "Build the JUCE plug-in targets" ON)
option(GRAINLATCH_BUILD_TESTS "Build tests" ON)
set(GRAINLATCH_ARTIFACT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/artifacts" CACHE PATH
    "Directory for final VST3, AU, and Standalone products")
set(EHL_JUCE_SOURCE_DIR "" CACHE PATH "Existing JUCE source checkout; avoids network when set")

add_library(grainlatch_dsp STATIC
    Source/dsp/GranularCore.cpp)

target_include_directories(grainlatch_dsp PUBLIC Source)
target_compile_features(grainlatch_dsp PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(grainlatch_dsp PRIVATE /W4)
else()
    target_compile_options(grainlatch_dsp PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(GRAINLATCH_BUILD_TESTS)
    enable_testing()
    add_executable(GrainLatchTests Tests/TestMain.cpp)
    target_link_libraries(GrainLatchTests PRIVATE grainlatch_dsp)
    add_test(NAME GrainLatchDSPTests COMMAND GrainLatchTests)
endif()

if(GRAINLATCH_BUILD_PLUGIN)
    include(FetchContent)

    if(EHL_JUCE_SOURCE_DIR)
        if(NOT EXISTS "${EHL_JUCE_SOURCE_DIR}/CMakeLists.txt")
            message(FATAL_ERROR "EHL_JUCE_SOURCE_DIR does not contain JUCE: ${EHL_JUCE_SOURCE_DIR}")
        endif()
        add_subdirectory("${EHL_JUCE_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/juce")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
        add_subdirectory(JUCE)
    else()
        FetchContent_Declare(
            JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.15
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()

    add_subdirectory(modules/juce-ehl-design-module)

    set(GRAINLATCH_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND GRAINLATCH_FORMATS AU)
    endif()

    set(EHL_COPY_PLUGIN_AFTER_BUILD_DEFAULT OFF)
    if(APPLE AND NOT DEFINED ENV{CI})
        set(EHL_COPY_PLUGIN_AFTER_BUILD_DEFAULT ON)
    endif()
    option(EHL_COPY_PLUGIN_AFTER_BUILD
        "Copy built plug-in formats to the current user's standard plug-in folders"
        ${EHL_COPY_PLUGIN_AFTER_BUILD_DEFAULT})

    set(EHL_PLUGIN_COPY_ARGS
        COPY_PLUGIN_AFTER_BUILD ${EHL_COPY_PLUGIN_AFTER_BUILD})
    if(APPLE)
        list(APPEND EHL_PLUGIN_COPY_ARGS
            VST3_COPY_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3"
            AU_COPY_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/Components")
    endif()

    juce_add_plugin(GrainLatch
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "EsionHsrahLatigid"
        BUNDLE_ID "jp.ehl.grainlatch"
        PRODUCT_NAME "GrainLatch"
        DESCRIPTION "Live granular latch and freeze effect"
        PLUGIN_MANUFACTURER_CODE EHL_
        PLUGIN_CODE GrLt
        AU_SANDBOX_SAFE TRUE
        FORMATS ${GRAINLATCH_FORMATS}
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        MICROPHONE_PERMISSION_ENABLED TRUE
        MICROPHONE_PERMISSION_TEXT "GrainLatch requires microphone access to process live audio input."
        ${EHL_PLUGIN_COPY_ARGS})

    target_sources(GrainLatch PRIVATE
        Source/ParameterIDs.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h)

    target_compile_definitions(GrainLatch PUBLIC
        JUCE_REPORT_APP_USAGE=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0)

    target_link_libraries(GrainLatch
        PRIVATE
            grainlatch_dsp
            EHL::JuceDesign
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    target_sources(GrainLatch_Standalone PRIVATE Source/StandaloneApp.cpp)
    target_compile_definitions(GrainLatch_Standalone PRIVATE JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)

    set(grainlatch_stage_platform "linux-x64")
    if(APPLE)
        set(grainlatch_stage_platform "macos-arm64")
    elseif(WIN32)
        set(grainlatch_stage_platform "windows-x64")
    endif()
    set(grainlatch_juce_products "${CMAKE_CURRENT_BINARY_DIR}/GrainLatch_artefacts/$<CONFIG>")
    set(grainlatch_staged_products "${GRAINLATCH_ARTIFACT_DIR}/plugin-release/${grainlatch_stage_platform}")
    set(grainlatch_artifact_dependencies GrainLatch_VST3 GrainLatch_Standalone)
    set(grainlatch_artifact_commands
        COMMAND ${CMAKE_COMMAND} -E make_directory "${grainlatch_staged_products}/vst3"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${grainlatch_staged_products}/standalone"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${grainlatch_staged_products}/au")

    if(APPLE)
        list(APPEND grainlatch_artifact_commands
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${grainlatch_staged_products}/vst3/grainlatch_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${grainlatch_staged_products}/standalone/grainlatch_standalone_plugin.app"
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${grainlatch_staged_products}/au/grainlatch_au_plugin.component"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${grainlatch_juce_products}/VST3/GrainLatch.vst3" "${grainlatch_staged_products}/vst3/grainlatch_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${grainlatch_juce_products}/Standalone/GrainLatch.app" "${grainlatch_staged_products}/standalone/grainlatch_standalone_plugin.app")
    elseif(WIN32)
        list(APPEND grainlatch_artifact_commands
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${grainlatch_staged_products}/vst3/grainlatch_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${grainlatch_juce_products}/VST3/GrainLatch.vst3" "${grainlatch_staged_products}/vst3/grainlatch_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy
                    "${grainlatch_juce_products}/Standalone/GrainLatch.exe" "${grainlatch_staged_products}/standalone/grainlatch_standalone_plugin.exe")
    else()
        list(APPEND grainlatch_artifact_commands
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${grainlatch_staged_products}/vst3/grainlatch_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${grainlatch_juce_products}/VST3/GrainLatch.vst3" "${grainlatch_staged_products}/vst3/grainlatch_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy
                    "${grainlatch_juce_products}/Standalone/GrainLatch" "${grainlatch_staged_products}/standalone/grainlatch_standalone_plugin")
    endif()

    if(TARGET GrainLatch_AU)
        list(APPEND grainlatch_artifact_dependencies GrainLatch_AU)
        list(APPEND grainlatch_artifact_commands
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${grainlatch_juce_products}/AU/GrainLatch.component" "${grainlatch_staged_products}/au/grainlatch_au_plugin.component")
    endif()

    if(APPLE)
        list(APPEND grainlatch_artifact_commands
            COMMAND codesign --force --deep --sign -
                    "${grainlatch_staged_products}/vst3/grainlatch_vst3_plugin.vst3"
            COMMAND codesign --force --deep --sign -
                    "${grainlatch_staged_products}/standalone/grainlatch_standalone_plugin.app"
            COMMAND codesign --force --deep --sign -
                    "${grainlatch_staged_products}/au/grainlatch_au_plugin.component")
    endif()

    list(APPEND grainlatch_artifact_commands
        COMMAND ${CMAKE_COMMAND}
            -DPRODUCT_NAME=GrainLatch
            -DSLUG=grainlatch
            -DBUNDLE_ID=jp.ehl.grainlatch
            -DPLUGIN_CODE=GrLt
            -DSTAGE_DIR=${grainlatch_staged_products}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/WriteArtifactsManifest.cmake")

    add_custom_target(ehl_stage_products
        ${grainlatch_artifact_commands}
        DEPENDS ${grainlatch_artifact_dependencies}
        COMMENT "Staging GrainLatch products under ${GRAINLATCH_ARTIFACT_DIR}/plugin-release")

    add_custom_target(GrainLatch_Artifacts DEPENDS ehl_stage_products)

    if(GRAINLATCH_BUILD_TESTS)
        add_executable(GrainLatchIntegrationTests Tests/PluginIntegrationTest.cpp)
        target_include_directories(GrainLatchIntegrationTests PRIVATE Source)
        target_link_libraries(GrainLatchIntegrationTests PRIVATE GrainLatch juce::juce_audio_utils)
        add_dependencies(GrainLatchIntegrationTests GrainLatch_Artifacts)

        juce_add_console_app(GrainLatchPluginLoadTest
            PRODUCT_NAME "GrainLatch Plugin Load Test")
        target_sources(GrainLatchPluginLoadTest PRIVATE Tests/PluginLoadTest.cpp)
        target_include_directories(GrainLatchPluginLoadTest PRIVATE Source)
        target_link_libraries(GrainLatchPluginLoadTest PRIVATE
            juce::juce_audio_processors_headless
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
        target_compile_definitions(GrainLatchPluginLoadTest PRIVATE
            JUCE_PLUGINHOST_VST3=1
            JUCE_USE_CURL=0
            JUCE_WEB_BROWSER=0)
        add_dependencies(GrainLatchPluginLoadTest GrainLatch_VST3)

        add_test(NAME GrainLatchPluginIntegrationTests COMMAND GrainLatchIntegrationTests)
        add_test(NAME GrainLatchHostVST3
            COMMAND GrainLatchPluginLoadTest
                    "${grainlatch_juce_products}/VST3/GrainLatch.vst3"
                    "GrainLatch")
        add_test(
            NAME GrainLatchArtifactChecks
            COMMAND ${CMAKE_COMMAND}
                    -DSTAGE_DIR=${grainlatch_staged_products}
                    -DSLUG=grainlatch
                    -DEXPECT_AU=$<BOOL:${APPLE}>
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/Tests/VerifyArtifacts.cmake)
    endif()
endif()
