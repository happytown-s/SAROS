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
// ===============================================

class InputManager
{
	public:
	InputManager(){};

	//初期化、リセット
	void prepare(double sampleRate, int bufferSize);
	void reset();


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

private:

	float computeEnergy(const juce::AudioBuffer<float>& input);

	//内部ロジック
	bool detectTriggerSample(const juce::AudioBuffer<float>& input);
	long findSilenceStartAbs(long triggerAbsIndex);
	long findAttackStartAbs(long triggerAbsIndex);
	void updateStateMachine();

	//===内部データ===
//	RingBuffer ringBuffer;

	SmartRecConfig config;
	juce::TriggerEvent triggerEvent;

	double sampleRate = 44100.0;
	bool triggered = false;
	bool recording = false;
	
	float smoothedEnergy = 0.0f;

};
