cmake_minimum_required(VERSION 3.15)
project(Looper VERSION 1.0.0)

# 🍏 Apple Silicon 向けビルド設定
set(CMAKE_OSX_ARCHITECTURES "arm64")

# JUCEの実体フォルダを指定
set(JUCE_DIR "/Users/mtsh/JUCE")

add_subdirectory(${JUCE_DIR} "${CMAKE_CURRENT_BINARY_DIR}/JUCE_build")

# GUIアプリの定義
juce_add_gui_app(Looper
    COMPANY_NAME "BabyLeaf"
    PRODUCT_NAME "Looper"
    OUTPUT_NAME "Looper"
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/LooperApp"
)

# バイナリデータ（フォントなど）の定義
juce_add_binary_data(Assets
    SOURCES
        Source/Assets/Orbitron-Bold.ttf
)

set_target_properties(Looper PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/LooperApp")

# ソースファイル
set(SOURCE_FILES
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/LooperAudio.cpp
    Source/InputManager.cpp
    Source/TransportPanel.cpp
    Source/LooperTrackUi.cpp
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
    Source/TriggerEvent.h
    Source/SmartGate.h
    Source/MainComponent.h
    Source/RoundButtonLookAndFeel.h
    Source/CircularVisualizer.h
)

target_sources(Looper PRIVATE ${SOURCE_FILES} ${HEADER_FILES})

# 使用モジュール
target_link_libraries(Looper PRIVATE
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
