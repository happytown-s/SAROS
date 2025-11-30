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

	//あとでRingBufferの準備を追加
	//ringBuffer.prepare(numChannels, bufferSize * 8);

	DBG("InputManager::prepare sampleRate = " << sampleRate << "bufferSize = " << bufferSize);


}

void InputManager::reset()
{
	triggerEvent.reset();
	recording = false;
	triggerEvent.sampleInBlock = -1;
	triggerEvent.absIndex = -1;
	recording = false;
	DBG("InputManager::reset()");
}

//==============================================================================
// メイン処理
//==============================================================================

void InputManager::analyze(const juce::AudioBuffer<float>& input)
{
	//float energy = computeEnergy(input);

	// (1) ブロック内でしきい値検知を実行
	bool trig = detectTriggerSample(input);

	// (2) 状態更新
	if (trig && !triggered)
	{
		triggered = true;
		//トリガー値を更新(仮、　absIndexはあとでリングバッファ実装時に設定 )
		triggered = true;
		triggerEvent.fire(0, -1);
		//DBG("Trigger ON | energy = " << smoothedEnergy);
	}
	else if(!trig && triggered)
	{
		triggered = false;
		triggerEvent.reset();

		//DBG("Trigger OFF | energy = " << smoothedEnergy);
	}
	updateStateMachine();
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

