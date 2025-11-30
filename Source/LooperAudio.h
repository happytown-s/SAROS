#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "TriggerEvent.h"
#include <map>
#include "TrackUtils.h"


//UNDO用の履歴
struct TrackHistory
{
	int trackId = -1;
	juce::AudioBuffer<float> previousBuffer;
};


class LooperAudio
{
	public:
	//録音開始と終了をMainComponentに知らせる
	struct Listener
	{
		virtual ~Listener() = default;

		virtual void onRecordingStarted(int trackID) = 0;
		virtual void onRecordingStopped(int trackID) = 0;
	};

	LooperAudio(double sr,int max);

	~LooperAudio();

	void prepareToPlay(int samplesPerBlockExpected, double sr);
	void processBlock(juce::AudioBuffer<float>& output, const juce::AudioBuffer<float>& input);
	void releaseResources() {}

	//TriggerEventの参照をセット
	void setTriggerReference(juce::TriggerEvent& ref)
	{triggerRef = &ref;}

//トラック操作
	void addTrack(int trackId);
	void startRecording(int trackId);
	void stopRecording(int trackId);
	void startPlaying(int trackId);
	void stopPlaying(int trackId);
	void clearTrack(int trackId);

	void startSequentialRecording(const std::vector<int>& selectedTracks);
	void stopRecordingAndContinue();

	void masterPositionReset(){ masterReadPosition = 0;}

	bool isRecordingActive() const;
	bool isLastTrackRecording() const;
	void allClear();

	void stopAllTracks();

	//UNDO関連
	void backupTrackBeforeRecord (int trackId);
	void undoLastRecording();

	//リスナー関係
	void addListener(Listener* l) {listeners.add(l);}
	void removeListener(Listener* l){listeners.remove(l);}


private:

	//トラック音声関連のデータ
	struct TrackData
	{
		juce::AudioBuffer<float> buffer;
		bool isRecording = false;
		bool isPlaying = false;
		int writePosition = 0;
		int readPosition = 0;
		int recordLength = 0;
		int recordStartSample = 0; //グローバル位置での録音開始サンプル
		int lengthInSample = 0; //トラックの長さ
		float currentLevel = 0.0f;
	};

public:

	//===================================
	//トラックIDと状態のゲッター
	//===================================
	
	const std::map<int, TrackData>& getTracks() const {return tracks;}
	bool isAnyRecording() const;
	bool isAnyPlaying() const;
	bool hasRecordedTracks() const;
	int getCurrentTrackId() const;


private:




	std::map<int, TrackData> tracks;
	std::optional<TrackHistory> lastHistory;


	double sampleRate;
	int maxSamples;

	//最初に録音完了したトラックをマスターとする
	int masterStartSample    = 0;
	int masterTrackId = -1;
	int masterLoopLength = 0;
	int masterReadPosition = 0;
	long currentSamplePosition = 0;

	std::vector<int> recordingQueue;
	int currentRecordingIndex = -1;

	juce::ListenerList<Listener> listeners;

	juce::TriggerEvent* triggerRef = nullptr;

	void recordIntoTracks(const juce::AudioBuffer<float>& input);
	void mixTracksToOutput(juce::AudioBuffer<float>& output);

};

