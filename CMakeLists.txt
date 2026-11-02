cmake_minimum_required(VERSION 3.22)

project(MasterLens VERSION 2.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

if (NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

# JUCE is vendored in .tools/JUCE (see README for the exact tag)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.tools/JUCE ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)

juce_add_plugin(MasterLens
    VERSION 2.1.0
    COMPANY_NAME "BerryStudio"
    BUNDLE_ID "com.berrystudio.masterlens"
    PLUGIN_MANUFACTURER_CODE OrcA
    PLUGIN_CODE Maln
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "MasterLens"
    DESCRIPTION "Mastering loudness and mix quality inspector"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    MICROPHONE_PERMISSION_ENABLED FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Analyzer
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png"
)

set(MASTERLENS_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/album/AlbumSession.cpp
    Source/audio/AuditionEngine.cpp
    Source/audio/ReferencePlayer.cpp
    Source/file/FileAnalyzer.cpp
    Source/file/FileForensics.cpp
    Source/file/FileReport.cpp
    Source/EditorControls.cpp
    Source/EditorFiles.cpp
    Source/EditorPaint.cpp
    Source/EditorLayout.cpp
    Source/EditorTimer.cpp
    Source/EditorRail.cpp
    Source/EditorTexts.cpp
    Source/EditorPresets.cpp
    Source/EditorMarkers.cpp
    Source/EditorReportTxt.cpp
    Source/EditorJson.cpp
    Source/EditorCsv.cpp
    Source/EditorPdf.cpp
    Source/EditorSeries.cpp
    Source/dsp/AnalysisEngine.cpp
    Source/dsp/SpectrumAnalyzer.cpp
    Source/dsp/ReferenceSpectrum.cpp
    Source/dsp/DefectAnalyzer.cpp
    Source/ui/Theme.cpp
    Source/ui/Logo.cpp
    Source/ui/Panels.cpp
    Source/ui/Meters.cpp
    Source/ui/Stereo.cpp
    Source/ui/Spectra.cpp
    Source/ui/History.cpp
    Source/ui/IssueList.cpp
    Source/ui/About.cpp
    Source/ui/GenreInfo.cpp
    Source/ui/Checklist.cpp
    Source/ui/Rail.cpp
    Source/ui/Timeline.cpp
    Source/ui/Detail.cpp
    Source/ui/DetailPages.cpp
    Source/ui/DetailPeak.cpp
    Source/ui/DetailMix.cpp
    Source/ui/DetailAlbum.cpp
    Source/ui/DetailSignal.cpp
    Source/ui/DetailTarget.cpp
    Source/ui/DetailFiles.cpp
    Source/ui/DetailDiag.cpp
    Source/ui/DetailTime.cpp
    Source/ui/EventMerge.cpp
    Source/ui/Advisor.cpp
    Source/ui/Settings.cpp
)

target_sources(MasterLens PRIVATE ${MASTERLENS_SOURCES})

target_compile_definitions(MasterLens PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=0
)

target_link_libraries(MasterLens
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_cryptography
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
)

# Keep the build output tidy: build/MasterLens_artefacts/<CONFIG>/VST3/MasterLens.vst3 etc.
set_target_properties(MasterLens PROPERTIES XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-")

# ---------------------------------------------------------------------------
# Offline DSP self-test (not part of the shipped bundles)
# ---------------------------------------------------------------------------
juce_add_console_app(MasterLensDspTest PRODUCT_NAME "MasterLensDspTest")

target_sources(MasterLensDspTest PRIVATE
    Tests/DspTest.cpp
    Source/album/AlbumSession.cpp
    Source/dsp/AnalysisEngine.cpp
    Source/dsp/SpectrumAnalyzer.cpp
    Source/dsp/DefectAnalyzer.cpp
    Source/dsp/ReferenceSpectrum.cpp
    Source/audio/AuditionEngine.cpp
    Source/audio/ReferencePlayer.cpp
    Source/file/FileAnalyzer.cpp
    Source/file/FileForensics.cpp
    Source/file/FileReport.cpp
)

target_include_directories(MasterLensDspTest PRIVATE Source)

target_compile_definitions(MasterLensDspTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1
)

target_link_libraries(MasterLensDspTest
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_cryptography
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
)

# ---------------------------------------------------------------------------
# Headless CLI (batch / CI / scriptable file QC)
# ---------------------------------------------------------------------------
juce_add_console_app(MasterLensCli PRODUCT_NAME "MasterLensCli")

target_sources(MasterLensCli PRIVATE
    Source/cli/main.cpp
    Source/dsp/AnalysisEngine.cpp
    Source/dsp/SpectrumAnalyzer.cpp
    Source/dsp/DefectAnalyzer.cpp
    Source/file/FileAnalyzer.cpp
    Source/file/FileForensics.cpp
    Source/file/FileReport.cpp
)

target_include_directories(MasterLensCli PRIVATE Source)

target_compile_definitions(MasterLensCli PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1
)

target_link_libraries(MasterLensCli
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_core
        juce::juce_cryptography
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
)

# ---------------------------------------------------------------------------
# Offscreen editor renderer (layout verification helper)
# ---------------------------------------------------------------------------
juce_add_gui_app(MasterLensUiShot PRODUCT_NAME "MasterLensUiShot")

target_sources(MasterLensUiShot PRIVATE
    Tools/UiShot.cpp
    ${MASTERLENS_SOURCES}
)

target_include_directories(MasterLensUiShot PRIVATE Source)

target_compile_definitions(MasterLensUiShot PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(MasterLensUiShot
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_cryptography
    PUBLIC
        juce::juce_recommended_config_flags
)


