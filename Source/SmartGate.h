/*
  ==============================================================================

     SmartGate.h
    Created: 11 Oct 2025 8:20:39am
    Author:  mt sh

  ==============================================================================
*/

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
//------------------------------------------------------------
// 音の振幅＆勾配を元にゲート制御を行うクラス
// 無音区間やブレスを自動でミュートし、
// 発声のみをスムーズに通すためのフィルタ。
//------------------------------------------------------------

class SmartGate
{
public:

	SmartGate() = default;

	//==============================================
	// メイン処理
	//==============================================

	void processBlock(juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output)
	{
		const int numSamples = input.getNumSamples();
		const float* readPtr = input.getReadPointer(0);
		float* writePtr = input.getWritePointer(0);

		for(int i = 0 ; i < numSamples; ++i)
		{
			float amp = std::abs(readPtr[i]);
			float slope = std::abs(readPtr[i] - prevSample);//勾配（変化量）

			//トリガー判定 : 音量か勾配のいずれかが閾値を超えたら通す
			bool triggerd = (amp > ampThreshold) || (slope > slopeThreshold);

			gateLevel += (triggerd ? openSpeed : -closeSpeed);
			gateLevel  = juce::jlimit(0.0f, 1.0f, gateLevel);

			writePtr[i] = readPtr[i] * gateLevel;
			prevSample = readPtr[i];
		}
	}


//==============================================
// 各種設定
//==============================================

	void setThresholds(float amp, float slope)
	{
		ampThreshold = amp;
		slopeThreshold = slope;
	}

	void setSpeeds(float open, float close)
	{
		openSpeed = open;
		closeSpeed = close;
	}

	float getGateLevel() const noexcept {return gateLevel;}


private:


	float prevSample = 0.0f;
	float gateLevel = 0.0f;

	//パラメータ
	float ampThreshold = 0.01f;
	float slopeThreshold = 0.1;
	float openSpeed = 0.05f;
	float closeSpeed = 0.05f;
};

