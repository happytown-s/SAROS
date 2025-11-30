/*
  ==============================================================================

    TriggerEvent.h
    Created: 12 Oct 2025 4:58:51pm
    Author:  mt sh

  ==============================================================================
*/

#pragma once
#include <atomic>

// ===============================================
// トリガーイベント情報
// ===============================================

namespace juce {
	struct TriggerEvent
	{
		std::atomic<bool> triggerd {false};
		long absIndex = -1; //リングバッファ上の絶対位置
		int sampleInBlock = -1;
		int channel = 0; //検知チャンネル

		//フラグ消費後リセット
		bool consume() noexcept
		{
			bool expected = true;
			return triggerd.compare_exchange_strong(expected, false);
		}

		//トリガー発火
		void fire(int sample = -1,long abs = -1) noexcept
		{
			sampleInBlock = sample;
			absIndex = abs;
			triggerd.store(true);
			//DBG("Trigger is Fire🔥");
		}

		//状態確認
		bool isTriggerd() const noexcept {return triggerd.load();}

		//手動リセット
		void reset() noexcept
		{
			triggerd.store(false);
			sampleInBlock = -1;
			absIndex = -1;
		}
	};
}
