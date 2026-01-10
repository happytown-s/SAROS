/*
  ==============================================================================

    InputManager.cpp
    Created: 8 Oct 2025 3:22:59am
    Author:  mt sh

  ==============================================================================
*/

#include "InputManager.h"

void InputManager::prepare(double newSampleRate, int bufferSize)
{
	sampleRate = newSampleRate;
	triggered  = false;
	recording = false;
	triggerEvent.reset();
	smoothedEnergy = 0.0f;

	// Prepare ring buffer (2 seconds)
    inputBuffer.prepare(sampleRate, 2);

	DBG("InputManager::prepare sampleRate = " << sampleRate << "bufferSize = " << bufferSize);
    DBG("AudioInputBuffer initialized.");
}

void InputManager::reset()
{
	triggerEvent.reset();
	recording = false;
	triggerEvent.sampleInBlock = -1;
	triggerEvent.absIndex = -1;
    // inputBuffer.clear(); // Optional: clear buffer on reset
	DBG("InputManager::reset()");
}

//==============================================================================
// メイン処理（マルチチャンネル対応版）
//==============================================================================

void InputManager::analyze(const juce::AudioBuffer<float>& input)
{
    const int numSamples = input.getNumSamples();
    const int numChannels = input.getNumChannels();
    if (numSamples == 0) return;

    // チャンネル数は固定配列のため調整不要（最大8ch）

    // Use channel 0 for trigger analysis and buffering (Mono support for now)
    const float* readPtr = input.getReadPointer(0);
    
    // 1. Write to Ring Buffer
    inputBuffer.write(readPtr, numSamples);

    // Calculate level for each channel
    float maxAmp = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float chMax = 0.0f;
        const float* chPtr = input.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            float absSamp = std::abs(chPtr[i]);
            if (absSamp > chMax) chMax = absSamp;
        }
        
        // ステレオリンクOFF（モノラルモード）の場合、ゲインブーストを適用
        if (ch < channelManager.getNumChannels() && !channelManager.getSettings(ch).isStereoLinked)
        {
            chMax *= ChannelTriggerSettings::getMonoGainBoostLinear();
        }
        
        if (ch < MAX_CHANNELS)
            channelLevels[static_cast<size_t>(ch)].store(chMax);
        
        if (chMax > maxAmp) maxAmp = chMax;
    }
    currentLevel.store(maxAmp); // Update atomic level
    
    // キャリブレーション中はピーク値を記録
    if (calibrating)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (ch < static_cast<int>(calibrationPeaks.size()))
            {
                float chLevel = (ch < MAX_CHANNELS) 
                                ? channelLevels[static_cast<size_t>(ch)].load() : 0.0f;
                if (chLevel > calibrationPeaks[static_cast<size_t>(ch)])
                    calibrationPeaks[static_cast<size_t>(ch)] = chLevel;
            }
        }
        calibrationSampleCount += numSamples;
        
        // 2秒間測定したら自動停止
        if (calibrationSampleCount >= static_cast<int>(sampleRate * 2.0))
        {
            stopCalibration();
        }
        return;  // キャリブレーション中はトリガー処理をスキップ
    }

	// 2. Process Triggers (2-stage logic / Multi-channel)
    bool trig = detectMultiChannelTrigger(input);

	// 3. State Update
	if (trig && !triggered)
	{
		triggered = true;
        
        // AudioInputBuffer から正確なサンプルインデックスを取得
        int sampleIdx = inputBuffer.getLastTriggerIndexInBlock();
        
        // Fire trigger! AbsIndex is managed by AudioInputBuffer internally mostly,
        // but we signal capability via the event.
		triggerEvent.fire(sampleIdx, -1);
		DBG("🔥 Trigger Fired! High threshold exceeded (sampleIdx: " << sampleIdx << ").");
	}
	else if (triggered)
	{
        // 🔄 Auto-Reset logic:
        // We only reset 'triggered' when the input signal drops below silence threshold
        // (i.e. Low Threshold) for a sufficient time.
        // AudioInputBuffer handles this logic via 'inPreRoll' state.
        
        if (!inputBuffer.isInPreRoll())
        {
            triggered = false;
            triggerEvent.reset();
            DBG("🔄 Trigger Re-armed (Silence detected)");
        }
	}
}

//==============================================================================
// マルチチャンネル対応のトリガー検出
// いずれか1チャンネルでも閾値を超えたらトリガー発火（One-shot）
//==============================================================================

bool InputManager::detectMultiChannelTrigger(const juce::AudioBuffer<float>& input)
{
    const int numChannels = input.getNumChannels();
    const int numSamples = input.getNumSamples();
    
    if (numChannels == 0 || numSamples == 0) return false;
    
    // チャンネルマネージャーの設定がなければ従来のロジックを使用
    if (channelManager.getNumChannels() == 0)
    {
        const float* readPtr = input.getReadPointer(0);
        return inputBuffer.processTriggers(readPtr, numSamples, 
                                           config.silenceThreshold, 
                                           config.userThreshold);
    }
    
    // 全体のピークレベルを計測（鎮火判定用）
    float maxLevelOverall = 0.0f;
    float lowestThreshold = 1.0f;  // 最も低い閾値を記録
    
    // 各チャンネルをチェック（いずれか1つでも閾値を超えたらトリガー）
    for (int ch = 0; ch < juce::jmin(numChannels, channelManager.getNumChannels()); ++ch)
    {
        const auto& chSettings = channelManager.getSettings(ch);
        
        // 無効なチャンネルはスキップ
        if (!chSettings.isActive) continue;
        
        // ステレオリンク時は偶数チャンネル（L）のみ処理、奇数チャンネル（R）はスキップ
        if (chSettings.isStereoLinked && (ch % 2 == 1)) continue;
        
        float effectiveThreshold = chSettings.getEffectiveThreshold();
        lowestThreshold = juce::jmin(lowestThreshold, effectiveThreshold);
        
        const float* chPtr = input.getReadPointer(ch);
        
        // ステレオリンクの場合、L+R両方をチェック
        if (chSettings.isStereoLinked && ch + 1 < numChannels)
        {
            const float* chPtrR = input.getReadPointer(ch + 1);
            for (int i = 0; i < numSamples; ++i)
            {
                float maxSample = juce::jmax(std::abs(chPtr[i]), std::abs(chPtrR[i]));
                maxLevelOverall = juce::jmax(maxLevelOverall, maxSample);
                
                if (maxSample > effectiveThreshold)
                {
                    return inputBuffer.processTriggers(chPtr, numSamples, 
                                                       config.silenceThreshold, 
                                                       effectiveThreshold);
                }
            }
        }
        else
        {
            // モノラルまたは単独チャンネル
            float gainBoost = chSettings.isStereoLinked ? 1.0f : ChannelTriggerSettings::getMonoGainBoostLinear();
            
            for (int i = 0; i < numSamples; ++i)
            {
                float sampleLevel = std::abs(chPtr[i]) * gainBoost;
                maxLevelOverall = juce::jmax(maxLevelOverall, sampleLevel);
                
                if (sampleLevel > effectiveThreshold)
                {
                    return inputBuffer.processTriggers(chPtr, numSamples, 
                                                       config.silenceThreshold, 
                                                       effectiveThreshold);
                }
            }
        }
    }
    
    // 🔥 鎮火ロジック: 録音中でなく、全チャンネルが閾値の半分未満なら PreRoll をリセット
    // （次のトリガーを受け付けられるようにする）
    if (!recordingActive && inputBuffer.isInPreRoll() && maxLevelOverall < lowestThreshold * 0.5f)
    {
        inputBuffer.resetPreRoll();
        DBG("🔄 PreRoll reset (silence detected, level: " << maxLevelOverall << ")");
    }
    
    return false;
}

//==============================================================================
// チャンネルごとのエネルギー計算
//==============================================================================

float InputManager::computeChannelEnergy(const juce::AudioBuffer<float>& input, int channel)
{
    if (channel < 0 || channel >= input.getNumChannels()) return 0.0f;
    
    const int numSamples = input.getNumSamples();
    const float* data = input.getReadPointer(channel);
    float total = 0.0f;
    
    for (int i = 0; i < numSamples; ++i)
    {
        total += data[i] * data[i];
    }
    
    return std::sqrt(total / static_cast<float>(numSamples));
}

//==============================================================================
// エネルギー（RMS）を計算
//==============================================================================

float InputManager::computeEnergy(const juce::AudioBuffer<float>& input)
{
	const int numChannels = input.getNumChannels();
	const int numSamples = input.getNumSamples();
	float total = 0.0f;

	for (int ch = 0; ch < numChannels; ++ch)
	{
		const float* data = input.getReadPointer(ch);
		for (int i = 0; i < numSamples; ++i)
		{
			total += data[i] * data[i];
		}
	}

	const float mean = total / (float)(numChannels * numSamples);
	return std::sqrt(mean);
}

//==============================================================================
// キャリブレーション（ノイズフロア測定）
//==============================================================================

void InputManager::startCalibration()
{
    int numChannels = channelManager.getNumChannels();
    if (numChannels == 0) numChannels = 2;  // デフォルト2ch
    
    calibrationPeaks.clear();
    calibrationPeaks.resize(static_cast<size_t>(numChannels), 0.0f);
    calibrationSampleCount = 0;
    calibrating = true;
    
    DBG("📊 Calibration started (" << numChannels << " channels)");
}

void InputManager::stopCalibration()
{
    calibrating = false;
    
    // 測定結果をチャンネル設定に反映
    int numChannels = juce::jmin(static_cast<int>(calibrationPeaks.size()), 
                                  channelManager.getNumChannels());
    
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float noiseFloor = calibrationPeaks[static_cast<size_t>(ch)];
        // ノイズフロアに少しマージンを追加（1.5倍）
        channelManager.getSettings(ch).calibratedNoiseFloor = noiseFloor * 1.5f;
        
        DBG("📊 Channel " << ch << " noise floor: " << noiseFloor 
            << " -> calibrated: " << (noiseFloor * 1.5f));
    }
    
    DBG("📊 Calibration complete!");
}

//==============================================================================
// 状態遷移（仮）
//==============================================================================
void InputManager::updateStateMachine()
{
	// 将来的に「録音中→停止判定」などをここに追加
}

//==============================================================================
// Getter / Setter
//==============================================================================
juce::TriggerEvent& InputManager::getTriggerEvent() noexcept
{
	return triggerEvent;

}

void InputManager::setConfig(const SmartRecConfig& newConfig) noexcept
{
	config = newConfig;
}

const SmartRecConfig& InputManager::getConfig() const noexcept
{
	return config;
}

//==============================================================================
// 閾値検知：ブロック内の最大音量を確認
//==============================================================================

bool InputManager::detectTriggerSample(const juce::AudioBuffer<float>& input)
{
	const int numChannels = input.getNumChannels();
	const int numSamples = input.getNumSamples();
	const float threshold = config.userThreshold;

	for(int s = 0; s < numSamples; ++s)
	{
		float frameAmp = 0.0f;
		for ( int ch = 0; ch < numChannels; ++ch)
		{
			frameAmp += std::abs(input.getReadPointer(ch)[s]);
		}

		frameAmp /= (float)numChannels;

		if(frameAmp > threshold)
		{
			triggerEvent.sampleInBlock = s;
			triggerEvent.channel = 0;
			return true;
		}
	}
	return false;
}


