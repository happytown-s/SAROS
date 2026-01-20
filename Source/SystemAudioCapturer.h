/*
  ==============================================================================

    SystemAudioCapturer.h
    Created: 19 Jan 2026
    Author:  Antigravity

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_graphics/juce_graphics.h>

struct ShareableApp
{
    int processID;
    juce::String appName;
    juce::Image icon;
    bool isSelected = true; 
};

class SystemAudioCapturer
{
public:
    SystemAudioCapturer();
    ~SystemAudioCapturer();

    // サンプルレートとバッファサイズを設定（start前に呼ぶ）
    void prepare(double sampleRate, int bufferSize);

    // システムオーディオキャプチャの開始
    void start();

    // システムオーディオキャプチャの停止
    void stop();

    // キャプチャ中かどうか
    bool isCapturing() const;

    // 現在バッファにあるオーディオデータを取得（オーディオスレッドから呼び出し可）
    int getNextAudioBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    
    // 利用可能なアプリ一覧を取得（アイコン付き）
    std::vector<ShareableApp> getAvailableApps() const;
    
    // キャプチャ対象のアプリIDリストを更新
    void updateFilter(const std::vector<int>& includedPIDs);
    
    // 現在のオーディオレベルを取得（メーター表示用）
    float getCurrentLevel() const;

private:
    struct Pimpl;
    std::unique_ptr<Pimpl> pimpl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SystemAudioCapturer)
};
