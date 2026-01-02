/*
  ==============================================================================

    InputManager.h
    Created: 8 Oct 2025 3:22:45am
    Author:  mt sh

  ==============================================================================
*/

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "TriggerEvent.h"
#include "AudioInputBuffer.h"
#include "ChannelTriggerSettings.h"

struct SmartRecConfig
{
	float userThreshold = 0.005f;     //録音開始のしきい値
	float silenceThreshold = 0.005f; //無音とみなすしきい値
	int minSilenceMs = 10;		   //無音が続く時間(停止判定などに使用予定)
	int maxPreRollMs = 25;		   //遡り記録の最大時間
	int attackWindowMs = 25;		   //勾配検知の探索窓
	int slopeSmoothN = 5;		   //フェード時間
	int fadeMs = 8;
};



// ===============================================
// SmartRecの中心：InputManager
// マルチチャンネル対応版
// ===============================================

class InputManager
{
	public:
	InputManager(){};

	//初期化、リセット
	void prepare(double sampleRate, int bufferSize);
	void reset();

	// チャンネル数を設定（デバイス変更時に呼び出す）
	void setNumChannels(int numChannels)
	{
		channelManager.setNumChannels(numChannels);
	}
	
	int getNumChannels() const { return channelManager.getNumChannels(); }

	//メイン解析処理
	void analyze(const juce::AudioBuffer<float>& input) ;
	juce::TriggerEvent& getTriggerEvent() noexcept;
	void setTriggerEvent(){triggerEvent.triggerd = false;}

	//TriggerEvent& getTriggerEvent() {return triggerEvent;}
	void processInput (const juce::AudioBuffer<float>& input)
	{}

	//設定
	void setConfig(const SmartRecConfig& newConfig) noexcept;
	const SmartRecConfig& getConfig() const noexcept;
	
	// マルチチャンネル設定アクセス
	MultiChannelTriggerManager& getChannelManager() { return channelManager; }
	const MultiChannelTriggerManager& getChannelManager() const { return channelManager; }
	
	// ステレオリンク設定
	void setStereoLinked(bool linked) { channelManager.setStereoLinked(linked); }
	bool isStereoLinked() const { return channelManager.isStereoLinked(); }
	
	// キャリブレーション
	void setCalibrationEnabled(bool enabled) { channelManager.setCalibrationEnabled(enabled); }
	bool isCalibrationEnabled() const { return channelManager.isCalibrationEnabled(); }
	
	// キャリブレーション実行（ノイズフロア測定開始）
	void startCalibration();
	void stopCalibration();
	bool isCalibrating() const { return calibrating; }
	
	// 録音状態（鎮火抑制用）
	void setRecordingActive(bool active) { recordingActive = active; }
	bool isRecordingActive() const { return recordingActive; }

private:

	float computeEnergy(const juce::AudioBuffer<float>& input);
	
	// マルチチャンネル対応のエネルギー計算
	float computeChannelEnergy(const juce::AudioBuffer<float>& input, int channel);

	//内部ロジック
	bool detectTriggerSample(const juce::AudioBuffer<float>& input);
	
	// マルチチャンネル対応のトリガー検出
	bool detectMultiChannelTrigger(const juce::AudioBuffer<float>& input);
	
	long findSilenceStartAbs(long triggerAbsIndex);
	long findAttackStartAbs(long triggerAbsIndex);
	void updateStateMachine();

	//===内部データ===
    AudioInputBuffer inputBuffer; // Ring Buffer for 2-stage trigger
    MultiChannelTriggerManager channelManager;  // マルチチャンネル設定

	SmartRecConfig config;
	juce::TriggerEvent triggerEvent;

	double sampleRate = 44100.0;
	bool triggered = false;
	bool recording = false;
	
	float smoothedEnergy = 0.0f;
	
	// キャリブレーション用
	bool calibrating = false;
	int calibrationSampleCount = 0;
	std::vector<float> calibrationPeaks;  // チャンネルごとのピーク値
	
	// 録音状態（鎮火抑制用）
	bool recordingActive = false;

public:
    // Lookback wrapper
    void getLookbackData(juce::AudioBuffer<float>& dest) { inputBuffer.getLookbackData(dest); }
    AudioInputBuffer& getInputBuffer() { return inputBuffer; }
    
    float getCurrentLevel() const { return currentLevel.load(); }
    
    // チャンネルごとのレベル取得（最大16ch）
    float getChannelLevel(int channel) const
    {
        if (channel >= 0 && channel < MAX_CHANNELS)
            return channelLevels[static_cast<size_t>(channel)].load();
        return 0.0f;
    }

private:
    std::atomic<float> currentLevel { 0.0f };
    std::array<std::atomic<float>, MAX_CHANNELS> channelLevels {};  // チャンネルごとのレベル
};

