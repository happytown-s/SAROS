#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "TriggerEvent.h"
#include <map>
#include <optional>
#include "TrackUtils.h"
#include "PitchDetector.h"
#include "PitchShifter.h"


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
    void startAllPlayback(); // 全トラックを一斉に再生開始（同期ズレ防止）
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

        // Flanger (using Chorus with short delay)
        juce::dsp::Chorus<float> flanger;
        bool flangerEnabled = false;
        bool flangerSync = false;
        float flangerRate = 0.5f;
        float flangerDepth = 0.5f;
        float flangerFeedback = 0.0f;
        double flangerPhase = 0.0;   // Manual phase if synced

        // Chorus (using Chorus with longer delay for thickening)
        juce::dsp::Chorus<float> chorus;
        bool chorusEnabled = false;
        bool chorusSync = false;
        float chorusRate = 0.3f;
        float chorusDepth = 0.5f;
        float chorusMix = 0.5f;
        double chorusPhase = 0.0;    // Manual phase if synced

        // Tremolo (LFO-based volume modulation)
        bool tremoloEnabled = false;
        bool tremoloSync = false;
        float tremoloRate = 4.0f;    // Hz
        float tremoloDepth = 0.5f;   // 0.0-1.0
        int tremoloShape = 0;        // 0=Sine, 1=Square, 2=Triangle
        double tremoloPhase = 0.0;   // LFO phase state

        // Slicer / Trance Gate (rhythmic volume gate)
        bool slicerEnabled = false;
        bool slicerSync = false;
        float slicerRate = 4.0f;     // Hz (gate frequency)
        float slicerDepth = 1.0f;    // 0.0-1.0 (gate depth, 1.0 = full cut)
        float slicerDuty = 0.5f;     // 0.0-1.0 (gate open ratio)
        int slicerShape = 0;         // 0=Square, 1=Smooth
        double slicerPhase = 0.0;    // LFO phase state

        // Bitcrusher
        bool bitcrusherEnabled = false;
        float bitcrusherDepth = 0.0f; // Bits Reduction: 0.0(24bit) -> 1.0(4bit)
        float bitcrusherRate = 0.0f;  // Downsampling: 0.0(1x) -> 1.0(40x)
        float bitcrusherLastSampleL = 0.0f; // For sample & hold (L)
        float bitcrusherLastSampleR = 0.0f; // For sample & hold (R)
        int bitcrusherSampleCount = 0;      // Counter for downsampling

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

        // Granular Cloud
        struct Grain
        {
            bool isActive = false;
            int position = 0;       // Current read position in buffer
            int life = 0;           // Remaining samples
            int totalLife = 0;      // Total duration
            float speed = 1.0f;     // Playback speed
            float pan = 0.5f;       // 0.0 (L) - 1.0 (R)
            float gain = 1.0f;
            float window = 0.0f;    // Current window gain
        };

        struct GranularParams
        {
            bool enabled = false;
            
            // Parameters
            float sizeMs = 100.0f;      // Grain duration (50-500ms)
            float density = 0.5f;       // Spawn probability/rate (0.0-1.0)
            float jitter = 0.5f;        // Position randomness (0.0-1.0)
            float pitch = 1.0f;         // Base pitch shift (0.5-2.0)
            float pitchRandom = 0.2f;   // Pitch randomness
            float mix = 0.5f;           // Dry/Wet
            float feedback = 0.0f;      // Feedback (optional)
            
            // Runtime
            static const int MAX_GRAINS = 32;
            std::vector<Grain> activeGrains;
            int spawnTimer = 0; // Timer for next grain spawn
            
            GranularParams() {
                activeGrains.resize(MAX_GRAINS);
            }
        } granular;

        // Autotune (Pitch Correction)
        struct AutotuneParams
        {
            bool enabled = false;
            int key = 0;            // 0=C, 1=C#, 2=D, ... 11=B
            int scale = 0;          // 0=Chromatic, 1=Major, 2=Minor
            float amount = 1.0f;    // 0.0=No correction, 1.0=Hard tune
            float speed = 0.1f;     // Correction speed (0=instant, 1=slow glide)

            PitchDetector detector;
            PitchShifter shifterL;
            PitchShifter shifterR;

            float currentPitch = 0.0f;
            float targetPitch = 0.0f;
            float smoothedRatio = 1.0f;
        } autotune;
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
        std::atomic<float> currentEffectRMS {0.0f}; // FX適用後のRMS（Visualizer用）
		
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
    
	void setTrackGain(int trackId, float gain);
	void setTrackLoopMultiplier(int trackId, float multiplier);

    // Per-Track FX Setters
    void setTrackFilterCutoff(int trackId, float freq);
    void setTrackFilterResonance(int trackId, float q);
    void setTrackFilterType(int trackId, int type); // 0=LPF, 1=HPF

    // Flanger
    void setTrackFlangerEnabled(int trackId, bool enabled);
    void setTrackFlangerRate(int trackId, float rate);
    void setTrackFlangerDepth(int trackId, float depth);
    void setTrackFlangerFeedback(int trackId, float feedback);
    void setTrackFlangerSync(int trackId, bool sync);

    // Chorus
    void setTrackChorusEnabled(int trackId, bool enabled);
    void setTrackChorusRate(int trackId, float rate);
    void setTrackChorusDepth(int trackId, float depth);
    void setTrackChorusMix(int trackId, float mix);
    void setTrackChorusSync(int trackId, bool sync);

    // Tremolo
    void setTrackTremoloEnabled(int trackId, bool enabled);
    void setTrackTremoloRate(int trackId, float rate);
    void setTrackTremoloDepth(int trackId, float depth);
    void setTrackTremoloShape(int trackId, int shape);
    void setTrackTremoloSync(int trackId, bool sync);

    // Slicer / Trance Gate
    void setTrackSlicerEnabled(int trackId, bool enabled);
    void setTrackSlicerRate(int trackId, float rate);
    void setTrackSlicerDepth(int trackId, float depth);
    void setTrackSlicerDuty(int trackId, float duty);
    void setTrackSlicerShape(int trackId, int shape);
    void setTrackSlicerSync(int trackId, bool sync);

    // Bitcrusher
    void setTrackBitcrusherEnabled(int trackId, bool enabled);
    void setTrackBitcrusherDepth(int trackId, float depth);
    void setTrackBitcrusherRate(int trackId, float rate);

    // Granular Cloud
    void setTrackGranularEnabled(int trackId, bool enabled);
    void setTrackGranularSize(int trackId, float sizeMs);
    void setTrackGranularDensity(int trackId, float density);
    void setTrackGranularPitch(int trackId, float pitch);
    void setTrackGranularJitter(int trackId, float jitter);
    void setTrackGranularMix(int trackId, float mix);

    // Autotune
    void setTrackAutotuneEnabled(int trackId, bool enabled);
    void setTrackAutotuneKey(int trackId, int key);
    void setTrackAutotuneScale(int trackId, int scale);
    void setTrackAutotuneAmount(int trackId, float amount);
    void setTrackAutotuneSpeed(int trackId, float speed);

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

    // x2等の倍率を考慮した累積位置を返す (0-1でmaxMultiplier周分)
    float getEffectiveNormalizedPosition(float maxMultiplier) const
    {
        if (masterLoopLength <= 0 || maxMultiplier <= 0.0f)
            return 0.0f;

        // グローバル位置からの累積サンプル数
        int64_t relativePos = currentSamplePosition - masterStartSample;
        if (relativePos < 0) relativePos = 0;

        // maxMultiplier周分のループ長
        int64_t effectiveLoopLength = (int64_t)(masterLoopLength * maxMultiplier);

        // 正規化位置 (0-1)
        return (float)(relativePos % effectiveLoopLength) / (float)effectiveLoopLength;
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
        // マスター未確定時は1.0
        if (masterLoopLength == 0) return 1.0f;

        float maxMult = 1.0f;
        for(const auto& t : tracks)
        {
            float mult = 1.0f;
            // 録音中のトラックは設定されている倍率を使用（recordLengthが増加中なので）
            if (t.second.isRecording)
            {
                mult = t.second.loopMultiplier;
            }
            // 録音済みの場合は実際の長さから倍率を計算
            else if (t.second.recordLength > 0)
            {
                mult = (float)t.second.recordLength / (float)masterLoopLength;
            }
            // 未録音の場合は設定されている倍率を使用
            else
            {
                mult = t.second.loopMultiplier;
            }

            if (mult > maxMult) maxMult = mult;
        }
        return maxMult; 
    }

    // トラックの現在のRMSを取得 (Visualizer用)
    float getTrackRMS(int trackId) const
    {
        if (auto it = tracks.find(trackId); it != tracks.end())
            return it->second.currentLevel;
        return 0.0f;
    }

    // 現在の絶対サンプル位置を取得（Video Mode用）
    long getCurrentSamplePosition() const { return currentSamplePosition; }

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
    
    juce::CriticalSection audioLock;

	juce::TriggerEvent* triggerRef = nullptr;

	void recordIntoTracks(const juce::AudioBuffer<float>& input);
	void mixTracksToOutput(juce::AudioBuffer<float>& output);

    // Monitoring
    std::atomic<int> monitorTrackId { -1 };
    
    static constexpr int monitorFifoSize = 4096;
    juce::AbstractFifo monitorFifo { monitorFifoSize };
    std::vector<float> monitorFifoBuffer;

};

