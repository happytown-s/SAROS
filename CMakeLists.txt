cmake_minimum_required(VERSION 3.15)
project(SAROS VERSION 1.0.0)

# 🍏 Apple Silicon 向けビルド設定 (macOSのみ)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
endif()

# JUCEのパス設定
# CI環境では -DCI_BUILD=ON を指定してプロジェクトルートのJUCEを使用
# ローカル環境ではユーザーのJUCEパスを使用
option(CI_BUILD "CI環境でのビルド" OFF)

if(CI_BUILD)
    # CI環境: プロジェクトルートにダウンロードされたJUCEを使用
    set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
else()
    # ローカル環境: ユーザーのJUCEパスを使用
    set(JUCE_DIR "/Users/mtsh/JUCE")
endif()

message(STATUS "Using JUCE from: ${JUCE_DIR}")

add_subdirectory(${JUCE_DIR} "${CMAKE_CURRENT_BINARY_DIR}/JUCE_build")

# GUIアプリの定義
juce_add_gui_app(SAROS
    COMPANY_NAME "BabyLeaf"
    PRODUCT_NAME "SAROS"
    OUTPUT_NAME "SAROS"
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/SAROSApp"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Source/Assets/icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Source/Assets/icon.png"
)

# 🆔 Bundle ID の設定 (TCC権限の安定化に必須)
set_target_properties(SAROS PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.babyleaf.saros"
    MACOSX_BUNDLE_BUNDLE_NAME "SAROS"
    MACOSX_BUNDLE_DISPLAY_NAME "SAROS"
    MACOSX_BUNDLE_INFO_STRING "SAROS Looper"
    MACOSX_BUNDLE_COPYRIGHT "Copyright (c) 2026 BabyLeaf"
)

# バイナリデータ（フォントなど）の定義
juce_add_binary_data(Assets
    SOURCES
        Source/Assets/Orbitron-Bold.ttf
)

set_target_properties(SAROS PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/SAROSApp")

# ソースファイル
set(SOURCE_FILES
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/LooperAudio.cpp
    Source/InputManager.cpp
    Source/TransportPanel.cpp
    Source/LooperTrackUi.cpp
    Source/FXPanel.cpp
    Source/PlanetKnobLookAndFeel.cpp
    Source/MidiLearnManager.cpp
    Source/MidiLearnManager.cpp
    Source/MidiTabContent.cpp
    Source/OscDataSender.cpp
    Source/SystemAudioCapturer.h
    Source/SystemAudioCapturer.mm
    Source/AppSelectionPanel.h
    Source/AppSelectionPanel.cpp
)

set(HEADER_FILES
    Source/ThemeColours.h
    Source/Util.h
    Source/TrackUtils.h
    Source/InputManager.h
    Source/InputTap.h
    Source/LooperAudio.h
    Source/LooperTrackUi.h
    Source/TransportPanel.h
    Source/FXPanel.h
    Source/PlanetKnobLookAndFeel.h
    Source/TriggerEvent.h
    Source/SmartGate.h
    Source/MainComponent.h
    Source/RoundButtonLookAndFeel.h
    Source/CircularVisualizer.h
    Source/FilterSpectrumVisualizer.h
    Source/MidiMapping.h
    Source/MidiLearnManager.h
    Source/MidiLearnManager.h
    Source/MidiTabContent.h
    Source/OscDataSender.h
)


# MSVC対応: ソースコードをUTF-8として扱う
if (MSVC)
    target_compile_options(SAROS PRIVATE /utf-8)
endif()

target_sources(SAROS PRIVATE ${SOURCE_FILES} ${HEADER_FILES})

# 使用モジュール
target_link_libraries(SAROS PRIVATE
    Assets
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_audio_basics
    juce::juce_audio_utils
    juce::juce_audio_devices
    juce::juce_graphics
    juce::juce_dsp
    juce::juce_core
    juce::juce_events
    juce::juce_osc
    juce::juce_opengl
    "-framework ScreenCaptureKit"
    "-framework CoreMedia"
)

# 🔏 ビルド後に自動コード署名 (macOSのみ)
# 署名IDはSHA-1ハッシュで指定（環境変数 CODESIGN_SHA1 を使用）
# ハッシュは `security find-identity -v -p codesigning` で確認可能
if(APPLE AND DEFINED ENV{CODESIGN_SHA1})
    add_custom_command(TARGET SAROS POST_BUILD
        COMMAND codesign --force --deep --sign $ENV{CODESIGN_SHA1} "$<TARGET_BUNDLE_DIR:SAROS>"
        COMMENT "🔏 Code signing SAROS.app..."
    )
elseif(APPLE)
    message(STATUS "⚠️ CODESIGN_SHA1 not set. Skipping code signing.")
endif()

# 🚀 CMake から直接アプリを起動するためのカスタムターゲット
# 使用方法: cmake --build . --target run
if(APPLE)
    add_custom_target(run
        COMMAND open "$<TARGET_BUNDLE_DIR:SAROS>"
        DEPENDS SAROS
        COMMENT "🚀 Launching SAROS.app..."
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
else()
    add_custom_target(run
        COMMAND $<TARGET_FILE:SAROS>
        DEPENDS SAROS
        COMMENT "🚀 Launching SAROS..."
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

