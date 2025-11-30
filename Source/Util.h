/*
  ==============================================================================

    Util.h
    Created: 18 Oct 2025 6:17:51am
    Author:  mt sh

  ==============================================================================
*/

#pragma once
#include <juce_events/juce_events.h>        // MessageManager
#include <juce_audio_basics/juce_audio_basics.h> // Decibels などのオーディオ基本機能

namespace util
{

	//オーディオスレッド内でUI操作しても大丈夫なヘルパー
	inline void safeUi(std::function<void()> fn)
	{
		if(juce::MessageManager::getInstance()->isThisTheMessageThread())
			fn();
		else
			juce::MessageManager::callAsync(std::move(fn));
	}

// 💡 （今後追加予定）
// inline float dBToGain(float dB) { return juce::Decibels::decibelsToGain(dB); }
// inline float gainToDB(float gain) { return juce::Decibels::gainToDecibels(gain); }
// inline float lerp(float a, float b, float t) { return a + (b - a) * t; } // フェーダー補間など
}
