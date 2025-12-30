#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "InputManager.h"
#include "SmartGate.h"

//------------------------------------------------------------
// 入力オーディオデータをキャプチャして保持するユーティリティ
//------------------------------------------------------------
class InputTap : public juce::AudioIODeviceCallback
{
	public:

	std::atomic<bool> triggerFlag = false;

	InputTap() {	}

	void prepare(double newSampleRate, int bufferSize)
	{
		sampleRate = newSampleRate;
		buffer.setSize(2, bufferSize);
		buffer.clear();

		inputManager.prepare(sampleRate, bufferSize);

	}
	void process(const juce::AudioBuffer<float>& input);

	void audioDeviceAboutToStart(juce::AudioIODevice* device) override
	{
		const int inCh = device ? device->getActiveInputChannels().countNumberOfSetBits() : 0;
		const int bufSize = device ? device->getCurrentBufferSizeSamples() : 512;
		buffer.setSize(juce::jmax(1, inCh), bufSize);
		buffer.clear();
		//DBG("🎧 InputTap started: channels=" << inCh << " bufferSize=" << bufSize);
	}

	// ✅ JUCE 7.0.5以降（macOS含む）で使用
	void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
										  int numInputChannels,
										  float* const* outputChannelData,
										  int numOutputChannels,
										  int numSamples,
										  const juce::AudioIODeviceCallbackContext&) override
	{
		juce::ignoreUnused(outputChannelData, numOutputChannels);
		if (numInputChannels == 0) return;

		buffer.setSize(numInputChannels, numSamples, false, false, true);
		smartGate.setThresholds (0.015f,0.1f);
		smartGate.setSpeeds(0.05f, 0.03f);


		for (int ch = 0; ch < numInputChannels; ++ch)
		{
			if (inputChannelData[ch] != nullptr)
				buffer.copyFrom(ch, 0, inputChannelData[ch], numSamples);
		}

		//smartGate.processBlock(buffer,buffer);

		updateInputLevel(buffer);

		inputManager.analyze(buffer);


		// 🎙️ 音声レベルチェック
		if (numInputChannels > 0 && inputChannelData[0] != nullptr)
		{
			auto range = juce::FloatVectorOperations::findMinAndMax(inputChannelData[0], numSamples);
			if (std::abs(range.getStart()) > 0.001f || std::abs(range.getEnd()) > 0.001f)
			{
//				DBG("🎙️ Input active | range: "
//					<< range.getStart() << " ~ " << range.getEnd());
			}
		}
//		if(inputManager.getTriggerEvent().triggerd)
//			triggerFlag.store(true);

	}


	// ✅ JUCE 7.0.4以前（もしくは別環境）用の後方互換
	void audioDeviceIOCallback(const float* const* inputChannelData,
							   int numInputChannels,
							   float* const* outputChannelData,
							   int numOutputChannels,
							   int numSamples) 
	{
		juce::ignoreUnused(outputChannelData, numOutputChannels);
		captureInput(inputChannelData, numInputChannels, numSamples);
		
	}

	void audioDeviceStopped() override
	{
		buffer.setSize(0, 0);
		//DBG("🛑 InputTap stopped");
	}

	void getLatestInput(juce::AudioBuffer<float>& dest)
	{
		const int channels = juce::jmin(dest.getNumChannels(), buffer.getNumChannels());
		const int samples  = juce::jmin(dest.getNumSamples(),  buffer.getNumSamples());
		for (int ch = 0; ch < channels; ++ch)
			dest.copyFrom(ch, 0, buffer, ch, 0, samples);
	}
	void resetTriggerEvent()
	{
		auto& trig = inputManager.getTriggerEvent();
		trig.triggerd = false;
		trig.sampleInBlock = -1;
		trig.absIndex = -1;
	}



	InputManager& getManager() noexcept{return inputManager; }
	const InputManager& getManager() const noexcept {return inputManager;}
	juce::TriggerEvent& getTriggerEvent() noexcept {return inputManager.getTriggerEvent();}

	// 入力RMSレベルを取得
	float getInputRMS() const
	{
		return currentInputLevel.load();
	}

private:
	juce::AudioBuffer<float> buffer;
	InputManager inputManager;
	SmartGate smartGate;

	double sampleRate = 44100.0;
	std::atomic<float> currentInputLevel { 0.0f };

	void updateInputLevel(const juce::AudioBuffer<float>& buf)
	{
		float rms = 0.0f;
		if (buf.getNumChannels() > 0 && buf.getNumSamples() > 0)
			rms = buf.getRMSLevel(0, 0, buf.getNumSamples());

		// スムージング処理 (LooperAudioと同様)
		float current = currentInputLevel.load();
		constexpr float decayRate = 0.90f; // InputTap側は少し反応良く

		if (rms > current)
			currentInputLevel.store(rms);
		else
			currentInputLevel.store(current * decayRate + rms * (1.0f - decayRate));
	}

	void captureInput(const float* const* inputChannelData, int numInputChannels, int numSamples)
	{
		if (numInputChannels == 0)
			return;

		buffer.setSize(numInputChannels, numSamples, false, false, true);
		for (int ch = 0; ch < numInputChannels; ++ch)
		{
			if (inputChannelData[ch] != nullptr)
				buffer.copyFrom(ch, 0, inputChannelData[ch], numSamples);
		}
		
		updateInputLevel(buffer);

		// 🎙️ Debugログ（入力検知）
		//auto range = juce::FloatVectorOperations::findMinAndMax(buffer.getReadPointer(0), numSamples);
		//if (std::abs(range.getStart()) > 0.001f || std::abs(range.getEnd()) > 0.001f)
		//	DBG("🎙️ InputTap active | range: " << range.getStart() << " ~ " << range.getEnd());
	}
};

