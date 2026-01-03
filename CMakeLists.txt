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
    juce::juce_graphics
    juce::juce_dsp
    juce::juce_core
    juce::juce_events
)
