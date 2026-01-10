#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "TriggerEvent.h"
#include <map>
#include <optional>
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
    void startRecordingWithLookback(int trackId, const juce::AudioBuffer<float>& lookbackData);
	void stopRecording(int trackId);
	void startPlaying(int trackId, bool syncToMaster = true);
	void stopPlaying(int trackId);
	void clearTrack(int trackId);

	void startSequentialRecording(const std::vector<int>& selectedTracks);
	void stopRecordingAndContinue();

	void masterPositionReset(){ masterReadPosition = 0;}

	bool isRecordingActive() const;
	bool isLastTrackRecording() const;
	void allClear();
	
	// テスト用: 120BPM 4拍のクリック音を生成してトラックに入れる
	void generateTestClick(int trackId);
	
	// 波形アライメントテスト用: トラック1=等倍、トラック2=x2、トラック3=/2
	void generateTestWaveformsForVisualTest();

	void stopAllTracks();

	//UNDO関連
	void backupTrackBeforeRecord (int trackId);
	int undoLastRecording();  // undoしたトラックIDを返す（-1は失敗）

	//リスナー関係
	void addListener(Listener* l) {listeners.add(l);}
	void removeListener(Listener* l){listeners.remove(l);}


private:

	//トラック音声関連のデータ
	//トラック音声関連のデータ
    // FX Chain Definition
    struct FXChain
    {
        // Modules
        juce::dsp::Compressor<float> compressor;
        juce::dsp::StateVariableTPTFilter<float> filter;
        juce::dsp::DelayLine<float> delay { 96000 }; 
        juce::dsp::Reverb reverb;
        
        // Parameters
        float filterCutoff = 20000.0f;
        float filterRes = 0.707f;
        int   filterType = 0; // 0=LPF, 1=HPF
        bool  filterEnabled = false;

        float reverbMix = 0.0f;
        bool  reverbEnabled = false;
        
        float delayMix = 0.0f;
        float delayFeedback = 0.0f;
        float delayTime = 0.5f; // sec
        bool  delayEnabled = false;

        // Beat Repeat (Stutter)
        struct BeatRepeatState
        {
            bool isActive = false;      // ON/OFF toggle
            bool isRepeating = false;   // Currently repeating
            int division = 4;           // DIV: 4, 8, 16...
            float threshold = 0.1f;    // Transient detection
            
            int repeatSourcePos = 0;    // Start of the loop segment
            int repeatLength = 0;       // Length of segment
            int currentRepeatPos = 0;   // Progress in segment
            
            float lastPeak = 0.0f;      // For simple attack detection
        } beatRepeat;
    };

	struct TrackData
	{
		juce::AudioBuffer<float> buffer;
		bool isRecording = false;
		bool isPlaying = false;
		int writePosition = 0;
		int readPosition = 0;
		int recordLength = 0;
		int recordStartSample = 0; //グローバル位置での録音開始サンプル
		int recordingStartPhase = 0; // マスター基準の録音開始位相 (0~masterLength)
		int lengthInSample = 0; //トラックの長さ
		float currentLevel = 0.0f;
		float gain = 1.0f;
		float loopMultiplier = 1.0f; // 1.0, 2.0 (x2), 0.5 (/2)
		
		// Per-Track FX Chain
		FXChain fx;
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

	float getTrackRMS(int trackId) const;
	void setTrackGain(int trackId, float gain);
	void setTrackLoopMultiplier(int trackId, float multiplier);

    // Per-Track FX Setters
    void setTrackFilterCutoff(int trackId, float freq);
    void setTrackFilterResonance(int trackId, float q);
    void setTrackFilterType(int trackId, int type); // 0=LPF, 1=HPF

    void setTrackReverbMix(int trackId, float mix); // 0.0 - 1.0
    void setTrackReverbDamping(int trackId, float damping);
    void setTrackReverbRoomSize(int trackId, float size);

    void setTrackDelayMix(int trackId, float mix, float time); // mix 0-1, time 0-1 sec
    void setTrackDelayFeedback(int trackId, float feedback); // 0-1

    void setTrackCompressor(int trackId, float threshold, float ratio); 

    // Beat Repeat Setters
    void setTrackBeatRepeatActive(int trackId, bool active);
    void setTrackBeatRepeatDiv(int trackId, int div);
    void setTrackBeatRepeatThresh(int trackId, float thresh);
    
    // FX Enable/Disable
    void setTrackFilterEnabled(int trackId, bool enabled);
    void setTrackDelayEnabled(int trackId, bool enabled);
    void setTrackReverbEnabled(int trackId, bool enabled);

    // ================= Monitor / Visualization =================
    void setMonitorTrackId(int trackId);
    int getMonitorTrackId() const { return monitorTrackId.load(); }
    void popMonitorSamples(juce::AudioBuffer<float>& destBuffer);
	// ビジュアライザ用
	const juce::AudioBuffer<float>* getTrackBuffer(int trackId) const
	{
		if (auto it = tracks.find(trackId); it != tracks.end())
			return &it->second.buffer;
		return nullptr;
	}

	float getMasterNormalizedPosition() const
	{
		if (masterLoopLength > 0)
			return (float)masterReadPosition / (float)masterLoopLength;
		return 0.0f;
	}

    int getMasterLoopLength() const { return masterLoopLength; }
    
    // トラックのサンプル長取得 (アライメント後の長さ)
    int getTrackLength(int trackId) const
    {
        if (auto it = tracks.find(trackId); it != tracks.end())
        {
            // スレーブトラックはアライメント後、masterLoopLength と同じ長さのバッファになる
            // recordLength は実際に録音した長さ（メタデータ）
            // lengthInSample がループとして再生される長さ
            if (it->second.lengthInSample > 0)
                return it->second.lengthInSample;
            // マスタートラック（まだ lengthInSample が設定されていない場合）
            return it->second.recordLength;
        }
        return 0;
    }

    // トラックの録音開始位置（グローバル位置）を取得
    int getTrackRecordStart(int trackId) const
    {
        if (auto it = tracks.find(trackId); it != tracks.end())
             return it->second.recordStartSample; // グローバルサンプル数
        return 0;
    }
    
    // マスター作成時の開始絶対位置を取得 (トラックの相対位置計算用)
    int getMasterStartSample() const { return masterStartSample; }
    
    // 現在の最大ループ倍率を取得
    float getMaxLoopMultiplier() const 
    { 
        if (masterLoopLength == 0) return 1.0f;
        float maxMult = 1.0f;
        for(const auto& t : tracks)
        {
            if (t.second.recordLength > 0)
            {
                float mult = (float)t.second.recordLength / (float)masterLoopLength;
                if (mult > maxMult) maxMult = mult;
            }
        }
        return maxMult; 
    }


private:

	std::map<int, TrackData> tracks;
	std::optional<TrackHistory> lastHistory;

	double sampleRate;
	int maxSamples;
	juce::dsp::ProcessSpec fxSpec; // For per-track FX initialization

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

    // Monitoring
    std::atomic<int> monitorTrackId { -1 };
    
    static constexpr int monitorFifoSize = 4096;
    juce::AbstractFifo monitorFifo { monitorFifoSize };
    std::vector<float> monitorFifoBuffer;

};

