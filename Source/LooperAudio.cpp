                                                                        #include "LooperAudio.h"
#include <juce_events/juce_events.h>

LooperAudio::LooperAudio(double sr, int max)
    : sampleRate(sr), maxSamples(max)
{
    monitorFifoBuffer.resize(monitorFifoSize, 0.0f);
}

LooperAudio::~LooperAudio()
{
    listeners.clear();
}

void LooperAudio::prepareToPlay(int samplesPerBlockExpected, double sr)
{
    sampleRate = sr;
    
    // Store spec for per-track FX initialization
    fxSpec.sampleRate = sampleRate;
    fxSpec.maximumBlockSize = samplesPerBlockExpected;
    fxSpec.numChannels = 2;
}

void LooperAudio::processBlock(juce::AudioBuffer<float>& output,
                               const juce::AudioBuffer<float>& input)
{
    const juce::ScopedLock sl(audioLock); // 再生開始処理(startAllPlayback)との競合を防ぐ

    // 録音・再生処理
    output.clear();
    recordIntoTracks(input);
    mixTracksToOutput(output);

    // 入力音をモニター出力
    const int numInChannels = input.getNumChannels();
    const int numOutChannels = output.getNumChannels();
    const int numSamples = input.getNumSamples();

    if (numInChannels > 0)
    {
        for (int ch = 0; ch < numOutChannels; ++ch)
        {
            output.addFrom(ch, 0, input, ch % numInChannels, 0, numSamples);
        }
    }
    
    currentSamplePosition += numSamples;
}

void LooperAudio::addTrack(int trackId)
{
    auto& track = tracks[trackId];
    track.buffer.setSize(2, maxSamples);
    track.buffer.clear();
    
    // Initialize per-track FX
    if (fxSpec.sampleRate > 0)
    {
        track.fx.compressor.prepare(fxSpec);
        track.fx.filter.prepare(fxSpec);
        track.fx.delay.prepare(fxSpec);
        track.fx.reverb.prepare(fxSpec);
        
        // Defaults
        track.fx.compressor.setThreshold(0.0f);
        track.fx.compressor.setRatio(1.0f);
        track.fx.filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        track.fx.filter.setCutoffFrequency(20000.0f);
        track.fx.delay.setMaximumDelayInSamples(static_cast<int>(sampleRate * 2.0));
        
        juce::dsp::Reverb::Parameters params;
        params.dryLevel = 1.0f; params.wetLevel = 0.0f; params.roomSize = 0.5f;
        track.fx.reverb.setParameters(params);

        // Flanger Init
        track.fx.flanger.prepare(fxSpec);
        track.fx.flanger.setCentreDelay(1.5f); // 1.5ms for Flanger
        track.fx.flanger.setFeedback(0.0f);
        track.fx.flanger.setMix(0.5f);
        track.fx.flanger.setDepth(0.5f);
        track.fx.flanger.setRate(0.5f);

        // Chorus Init (longer delay for thickening effect)
        track.fx.chorus.prepare(fxSpec);
        track.fx.chorus.setCentreDelay(10.0f); // 10ms for Chorus
        track.fx.chorus.setFeedback(0.0f);
        track.fx.chorus.setMix(0.5f);
        track.fx.chorus.setDepth(0.5f);
        track.fx.chorus.setRate(0.3f);
    }
}



void LooperAudio::startRecording(int trackId)
{
    // 履歴に追加
    backupTrackBeforeRecord(trackId);

    auto& track = tracks[trackId];
    
    // Safety: Ensure buffer is full size if we are defining a new master loop
    if (masterLoopLength <= 0)
    {
        if (track.buffer.getNumSamples() < maxSamples)
        {
            track.buffer.setSize(2, maxSamples);
            DBG("🔧 Resized Track " << trackId << " buffer to maxSamples (" << maxSamples << ")");
        }
        
        // 新しいマスターループの基準時間を設定
        masterStartSample = currentSamplePosition;
        DBG("🏁 Master Start Sample reset to " << masterStartSample);
    }
    // Optimization/Safety: If Slave, ensure at least Master Length * multiplier
    else if (masterLoopLength > 0)
    {
        int requiredSize = (int)(masterLoopLength * track.loopMultiplier);
        // サイズが異なる場合は必ずリサイズ（大きすぎる場合も縮小してVisualizerの表示ズレを防ぐ）
        if (track.buffer.getNumSamples() != requiredSize)
        {
            track.buffer.setSize(2, requiredSize);
            DBG("🔧 Resized Track " << trackId << " buffer to " << requiredSize 
                << " (masterLoopLength * multiplier=" << track.loopMultiplier << ")");
        }
    }

    track.isRecording = true;
    track.isPlaying = false;
    track.recordLength = 0;

    // マスターが再生中なら、その位置から録音開始
    if (masterLoopLength > 0 && tracks.find(masterTrackId) != tracks.end() && tracks[masterTrackId].isPlaying)
    {
        // === x2位相のスマート調整 (Smart Phase Alignment) ===
        // もしこれが「最初の長尺トラック（倍率>1）」の録音で、かつ奇数週目（裏拍）なら、
        // グローバル時間をシフトして「偶数週目（表拍）」に合わせる。
        if (track.loopMultiplier > 1.0f)
        {
            bool hasOtherLongTracks = false;
            for (const auto& [id, t] : tracks)
            {
                if (id != trackId && t.loopMultiplier > 1.0f && t.buffer.getNumSamples() > 0 && (t.isPlaying || t.recordLength > 0))
                {
                    hasOtherLongTracks = true;
                    break;
                }
            }
            
            // DISABLING Smart Phase Alignment based on user feedback.
            // visualizer should reflect absolute recording time, even if it's on the 2nd loop (odd index).
            /*
            if (!hasOtherLongTracks)
            {
                int64_t rel = currentSamplePosition - masterStartSample;
                int64_t loopIdx = rel / masterLoopLength;
                if (loopIdx % 2 != 0) // 奇数（1, 3, 5...） = 裏拍
                {
                    masterStartSample += masterLoopLength;
                    DBG("🔄 Smart Phase Alignment: Shifted Master Start by 1 loop to align x2 start to 0");
                }
            }
            */
        }

        // A. Trigger録音の場合：ブロック内の正確なトリガー位置を使用
        int sampleIdxInBlock = (triggerRef && triggerRef->triggerd) ? triggerRef->sampleInBlock : 0;
        if (sampleIdxInBlock < 0) sampleIdxInBlock = 0;

        // マスターの位置に同期させる: 絶対位置から計算することで、x2等の長いトラックでの「2周目」を正しく判定
        // currentSamplePosition（ブロック先頭）にブロック内オフセットを加算
        int64_t exactTriggerPosition = currentSamplePosition + sampleIdxInBlock;
        int64_t relativeGlobal = exactTriggerPosition - masterStartSample;
        int trackLoopLength = track.buffer.getNumSamples();
        if (relativeGlobal < 0) relativeGlobal = 0;

        track.writePosition = (int)(relativeGlobal % trackLoopLength);
        
        // Visualizerの描画開始位置: 絶対時刻を使用する
        track.recordStartSample = (int)exactTriggerPosition;
        track.recordingStartPhase = track.writePosition;
        
        DBG("🎬 Start recording track " << trackId
            << " (Precision Aligned). AbsDiff: " << relativeGlobal 
            << " (sampleInBlock: " << sampleIdxInBlock << ")"
            << " -> WritePos: " << track.writePosition
            << " | RecordStartSample: " << track.recordStartSample);
    }
    // TriggerEventが有効なら記録開始位置として反映
    else if (triggerRef && triggerRef->triggerd)
    {
        int sampleIdx = triggerRef->sampleInBlock >= 0 ? triggerRef->sampleInBlock : 0;
        
        // absIndexが有効な場合はそれを使用、無効（-1）の場合は現在位置＋オフセットで計算
        int64_t triggerAbsTime = (triggerRef->absIndex >= 0) 
            ? triggerRef->absIndex 
            : (currentSamplePosition + sampleIdx);
        
        track.recordStartSample = static_cast<int>(triggerAbsTime);
        track.writePosition = juce::jlimit(0, maxSamples - 1, (int)(triggerAbsTime % maxSamples));
        DBG("🎬 Start recording track " << trackId
            << " triggered at " << triggerAbsTime << " (sampleIdx: " << sampleIdx << ")");
    }
    else
    {
        track.readPosition = 0;
        track.writePosition = 0;
        track.recordStartSample = 0;

        DBG("🎬 Start recording track " << trackId << " from beginning");
    }
    track.buffer.clear();

    listeners.call([&](Listener& l) { l.onRecordingStarted(trackId); });
}

void LooperAudio::startRecordingWithLookback(int trackId, const juce::AudioBuffer<float>& lookbackData)
{
    // First, standard start
    startRecording(trackId);

    if (auto it = tracks.find(trackId); it != tracks.end())
    {
        auto& track = it->second;
        int numLookback = lookbackData.getNumSamples();
        if (numLookback <= 0) return;

        // Loop limit definition: Use track's buffer size (handles x2, etc.)
        const int loopLimit = track.buffer.getNumSamples();
        if (loopLimit <= 0) return;

        // Calculate write start position (go back in time)
        int startWritePos = track.writePosition - numLookback;
        while (startWritePos < 0) startWritePos += loopLimit;

        // Limit lookback to loop size (sanity check)
        int samplesToCopy = numLookback;
        if (samplesToCopy > loopLimit)
            samplesToCopy = loopLimit;

        // --- Wrap-around Copy Logic ---
        int currentWritePos = startWritePos;
        int lookbackOffset = 0;
        int remaining = samplesToCopy;

        while (remaining > 0)
        {
            int samplesToEnd = loopLimit - currentWritePos;
            int chunk = juce::jmin(remaining, samplesToEnd);

            // Channel mapping (handle Mono to Stereo if needed)
            for (int ch = 0; ch < track.buffer.getNumChannels(); ++ch)
            {
                int srcCh = (ch < lookbackData.getNumChannels()) ? ch : 0;
                track.buffer.copyFrom(ch, currentWritePos, lookbackData, srcCh, lookbackOffset, chunk);
            }

            currentWritePos = (currentWritePos + chunk) % loopLimit;
            lookbackOffset += chunk;
            remaining -= chunk;
        }

        // --- Update Track State ---
        if (masterLoopLength <= 0)
        {
            // Master creation mode: adjust pointers forward
            track.recordLength += samplesToCopy;
            track.writePosition = currentWritePos; // Should match internal calc
            
            // Adjust global start time backward to reflect earlier start
            track.recordStartSample = currentSamplePosition - samplesToCopy;
            track.recordingStartPhase = (track.recordingStartPhase - samplesToCopy + loopLimit) % loopLimit;
        }
        else
        {
            // Slave mode: We pre-filled buffer sections.
            // Increase recorded length so loop completes sooner (as we already have data)
            track.recordLength += samplesToCopy;
            track.recordingStartPhase = (track.recordingStartPhase - samplesToCopy + loopLimit) % loopLimit;
            
            // Visualizerのために開始位置も調整（PreRoll分戻す）
            track.recordStartSample -= samplesToCopy; 
        }

        DBG("🔙 Lookback injected: " << samplesToCopy << " samples. Adjusted start: " << track.recordStartSample);
    }
}

void shiftBufferLeft(juce::AudioBuffer<float>& buffer, int numSamplesToShift)
{
    if (numSamplesToShift <= 0 || numSamplesToShift >= buffer.getNumSamples()) return;

    int numChannels = buffer.getNumChannels();
    int bufferSize = buffer.getNumSamples();
    int remaining = bufferSize - numSamplesToShift;

    // 一時バッファを使用
    juce::AudioBuffer<float> tempBuffer(numChannels, numSamplesToShift);
    
    for (int ch = 0; ch < numChannels; ++ch)
    {
        // 1. 先頭（0〜shift-1）を退避
        tempBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamplesToShift);
        
        // 2. 後半を先頭へ (memmoveを使用: AudioBuffer::copyFromはオーバーラップ未対応のため)
        auto* writePtr = buffer.getWritePointer(ch);
        std::memmove(writePtr, writePtr + numSamplesToShift, remaining * sizeof(float));
        
        // 3. 退避したデータを末尾へ
        buffer.copyFrom(ch, remaining, tempBuffer, ch, 0, numSamplesToShift);
    }
}

void LooperAudio::stopRecording(int trackId)
{
    auto& track = tracks[trackId];
    track.isRecording = false;

    track.isRecording = false;

    track.isRecording = false;

    // バッファアラインメント（強制シフト）は削除
    // (リズム優先のため、Sync録音された通りの配置を維持する)

    const int recordedLength = track.recordLength;
    if (recordedLength <= 0) return;

    if (masterLoopLength <= 0)
    {
        masterTrackId = trackId;
        masterLoopLength = recordedLength;
        track.lengthInSample = masterLoopLength;
        
        masterStartSample = (track.recordStartSample >= 0) ? track.recordStartSample : 0;
        
        if (track.recordStartSample < 0)
            track.recordStartSample = 0;

        masterReadPosition = 0;
        track.readPosition = 0;  // 🆕 ギャップ修正: マスター作成時は直接0から開始

        DBG("🎛 Master loop length set to " << masterLoopLength
            << " samples | recorded=" << recordedLength
            << " | masterStart=" << masterStartSample
            << " | readPos reset to 0");
    }
    else
    {
        // スレーブトラック: loopMultiplierを考慮したサイズでアラインメント
        int effectiveLength = (int)(masterLoopLength * track.loopMultiplier);
        juce::AudioBuffer<float> aligned;
        aligned.setSize(2, effectiveLength, false, false, true);
        aligned.clear();

        const int copyLen = juce::jmin(effectiveLength, track.buffer.getNumSamples());
        
        aligned.copyFrom(0, 0, track.buffer, 0, 0, copyLen);
        aligned.copyFrom(1, 0, track.buffer, 1, 0, copyLen);

        track.buffer.makeCopyOf(aligned);
        track.lengthInSample = effectiveLength;
        track.recordLength = recordedLength; 

        // ★ 重要: recordStartSampleは録音開始時に設定済み。ここで上書きしない。
        // (以前は masterStartSample で上書きしていたが、それが startAngleRatio=0 の原因だった)

        DBG("🟢 Track " << trackId << ": aligned to " << effectiveLength 
            << " samples (master=" << masterLoopLength << " * multiplier=" << track.loopMultiplier << ")");
    }

    listeners.call([&](Listener& l) { l.onRecordingStopped(trackId); });
}

void LooperAudio::startPlaying(int trackId, bool syncToMaster)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
    {
        auto& track = it->second;
        track.isPlaying = true;

        if (trackId == masterTrackId)
        {
            // マスタートラックは常に位置0から
            track.readPosition = 0;
            DBG("▶️ Start playing master track " << trackId << " from position 0");
        }
        else if (syncToMaster && masterLoopLength > 0)
        {
            // スレーブトラック: マスターの現在位置に同期（録音後の自動再生用）
            int effectiveLoopLength = (int)(masterLoopLength * track.loopMultiplier);
            if (effectiveLoopLength < 1) effectiveLoopLength = 1;

            int64_t relativePos = currentSamplePosition - masterStartSample;
            track.readPosition = (int)(relativePos % effectiveLoopLength);
            
            DBG("▶️ Start playing track " << trackId
                << " synced to master at " << track.readPosition);
        }
        else
        {
            // 手動停止→再生時: 位置0から
            track.readPosition = 0;
            DBG("▶️ Start playing track " << trackId << " from position 0");
        }
    }
}

void LooperAudio::startAllPlayback()
{
    // 全トラックを一斉に0位置からスタートさせる（ループで個別に呼ぶとコールバック割り込みでズレるため）
    const juce::ScopedLock sl(audioLock); // 必要ならロック
    
    // まずマスタートラックがあるか確認（あればそれもリセット）
    for (auto& [id, track] : tracks)
    {
        if (track.recordLength > 0)
        {
            track.isPlaying = true;
            track.readPosition = 0;
        }
    }
    DBG("▶️ Start ALL tracks from position 0 (Perfect Sync)");
}

void LooperAudio::stopPlaying(int trackId)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
        it->second.isPlaying = false;
}

void LooperAudio::clearTrack(int trackId)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
        it->second.buffer.clear();
}

void LooperAudio::recordIntoTracks(const juce::AudioBuffer<float>& input)
{
    const int numSamples = input.getNumSamples();

    for (auto& [id, track] : tracks)
    {
        if (!track.isRecording)
            continue;

        const int numChannels = juce::jmin(input.getNumChannels(), track.buffer.getNumChannels());
        
        const int loopLimit = (masterLoopLength > 0)
            ? (int)(masterLoopLength * track.loopMultiplier)
            : track.buffer.getNumSamples();

        if (loopLimit == 0) continue; 

        int currentWritePos;
        if (masterLoopLength > 0)
        {
            // x2の場合: 2周分のデータを正しく配置するため、相対位置を使用
            // /2の場合: 半周で同じ位置に戻るため、これも正しく動作
            int effectiveLoopLength = (int)(masterLoopLength * track.loopMultiplier);
            
            // マスターの累積ループカウントを考慮した書き込み位置
            // masterReadPositionだけでは2周目以降の位置がわからないため、
            // 絶対サンプル位置から計算
            int64_t relativePos = currentSamplePosition - masterStartSample;
            currentWritePos = (int)(relativePos % effectiveLoopLength);
        }
        else
        {
            currentWritePos = track.recordLength % loopLimit;
        }

        int samplesRemaining = numSamples;

        if (masterLoopLength > 0)
        {
            // loopMultiplierを考慮した最大録音可能量（x2なら2倍まで録音可能）
            int targetRecordLength = (int)(masterLoopLength * track.loopMultiplier);
            int maxRecordable = targetRecordLength - track.recordLength;
            if (maxRecordable < 0) maxRecordable = 0;
            samplesRemaining = juce::jmin(samplesRemaining, maxRecordable);
        }

        int inputReadOffset = 0;

        while (samplesRemaining > 0)
        {
            const int samplesToEnd = loopLimit - currentWritePos;
            const int samplesToCopy = juce::jmin(samplesRemaining, samplesToEnd);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                track.buffer.copyFrom(ch, currentWritePos, input, ch, inputReadOffset, samplesToCopy);
            }

            currentWritePos = (currentWritePos + samplesToCopy) % loopLimit;
            inputReadOffset += samplesToCopy;
            samplesRemaining -= samplesToCopy;
            
            track.recordLength = juce::jmin(track.recordLength + samplesToCopy, loopLimit);
        }

        track.writePosition = currentWritePos;

        // マスター位置ベースの自動録音終了
        // 録音開始位置（recordingStartPhase）をマスターが再通過したら終了
        if (masterLoopLength > 0)
        {
            int targetRecordLength = (int)(masterLoopLength * track.loopMultiplier);
            int startPhase = track.recordingStartPhase;
            
            // x2: 2周分録音したい → recordLengthがtargetRecordLengthに達したら終了
            // x1: 1周分録音したい → recordLengthがmasterLoopLengthに達したら終了
            // /2: 半周分録音したい → recordLengthがmasterLoopLength/2に達したら終了
            
            // さらに、録音開始位置に戻ったタイミングで終了（境界同期）
            bool reachedTarget = track.recordLength >= targetRecordLength;
            
            // マスターが録音開始位置を通過したかチェック（より正確な終了タイミング）
            // prevMasterPos から現在の masterReadPosition の間に startPhase があるか
            int prevMasterPos = (masterReadPosition - numSamples + masterLoopLength) % masterLoopLength;
            bool crossedStart = false;
            
            if (track.loopMultiplier >= 1.0f)
            {
                // x1以上: 録音開始位置を通過したかチェック
                if (prevMasterPos > masterReadPosition) {
                    // ループ境界を跨いだ
                    crossedStart = (prevMasterPos >= startPhase || masterReadPosition < startPhase);
                } else {
                    crossedStart = (prevMasterPos < startPhase && masterReadPosition >= startPhase);
                }
            }
            
            // 録音終了条件：サンプル数が目標に達した
            if (reachedTarget)
            {
                stopRecording(id);
                startPlaying(id);
                DBG("✅ Master-synced loop complete for Track " << id
                    << " | length=" << track.recordLength << " (multiplier=" << track.loopMultiplier << ")");
            }
        }
    }
}

void LooperAudio::mixTracksToOutput(juce::AudioBuffer<float>& output)
{
    const int numSamples = output.getNumSamples();
    
    // Temporary buffer for per-track FX processing
    juce::AudioBuffer<float> trackBuffer(2, numSamples);
    
    // Sum all tracks to output
    for (auto& [id, track] : tracks)
    {
        if (!track.isPlaying)
        {
            track.currentLevel *= 0.8f;
            if (track.currentLevel < 0.001f) track.currentLevel = 0.0f;
            continue;
        }

        const int outChannels = output.getNumChannels();
        
        const int loopLength = (masterLoopLength > 0)
            ? (int)(masterLoopLength * track.loopMultiplier)
            : juce::jmax(1, track.recordLength > 0 ? track.recordLength : track.buffer.getNumSamples());

        // Clear temp buffer
        trackBuffer.clear();
        
        int readPos = track.readPosition;
        int remaining = numSamples;
        int outputOffset = 0;

        // 🔄 再生ラップアラウンドループ - write to temp buffer first
        while (remaining > 0)
        {
            const int samplesToEnd = loopLength - readPos;
            const int samplesToCopy = juce::jmin(remaining, samplesToEnd);

            for (int ch = 0; ch < trackBuffer.getNumChannels(); ++ch)
            {
                trackBuffer.addFrom(ch, outputOffset, track.buffer, ch, readPos, samplesToCopy, track.gain);
            }

            readPos = (readPos + samplesToCopy) % loopLength;
            remaining -= samplesToCopy;
            outputOffset += samplesToCopy;
        }

        track.readPosition = readPos;

        // ============ Beat Repeat (Stutter) Logic ============
        auto& br = track.fx.beatRepeat;
        if (br.isActive)
        {
            // --- 1. Transient Detection (if armed but not repeating) ---
            if (!br.isRepeating)
            {
                // Check current block for peaks
                float blockPeak = trackBuffer.getMagnitude(0, 0, numSamples);
                
                // Simple transient detection: current peak > lastPeak + threshold
                if (blockPeak > br.lastPeak + br.threshold && blockPeak > 0.05f)
                {
                    br.isRepeating = true;
                    //基準点のキャプチャ（現在のブロックの開始位置を基準にする）
                    br.repeatSourcePos = (track.readPosition - numSamples + loopLength) % loopLength;
                    br.repeatLength = loopLength / br.division;
                    br.currentRepeatPos = 0;
                    
                    DBG("🔥 Beat Repeat Triggered! Track " << id << " | Div: " << br.division << " | Pos: " << br.repeatSourcePos);
                }
                br.lastPeak = blockPeak * 0.9f; // Decay for next detection
            }

            // --- 2. Playback Substitution (if repeating) ---
            if (br.isRepeating)
            {
                // Recalculate repeatLength in case division changed while repeating
                int newRepeatLength = loopLength / juce::jmax(1, br.division);
                if (newRepeatLength != br.repeatLength)
                {
                    br.repeatLength = newRepeatLength;
                    br.currentRepeatPos = br.currentRepeatPos % br.repeatLength;
                }
                
                // Clear the buffer that was just filled with normal playback
                trackBuffer.clear();
                
                int samplesToFill = numSamples;
                int fillOffset = 0;
                
                while (samplesToFill > 0)
                {
                    int samplesInSegmentToEnd = br.repeatLength - br.currentRepeatPos;
                    int chunk = juce::jmin(samplesToFill, samplesInSegmentToEnd);
                    
                    int sourceReadPos = (br.repeatSourcePos + br.currentRepeatPos) % loopLength;
                    
                    // Copy from captured segment
                    for (int ch = 0; ch < trackBuffer.getNumChannels(); ++ch)
                    {
                        trackBuffer.addFrom(ch, fillOffset, track.buffer, ch, sourceReadPos, chunk, track.gain);
                    }
                    
                    br.currentRepeatPos = (br.currentRepeatPos + chunk) % br.repeatLength;
                    samplesToFill -= chunk;
                    fillOffset += chunk;
                }
            }
        }
        else
        {
            br.lastPeak = 0.0f;
            br.isRepeating = false;
        }

        // ============ Per-Track FX Processing ============
        juce::dsp::AudioBlock<float> block(trackBuffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        
        // Filter (only if enabled)
        if (track.fx.filterEnabled)
            track.fx.filter.process(context);
        
        // Flanger
        if (track.fx.flangerEnabled)
            track.fx.flanger.process(context);

        // Chorus
        if (track.fx.chorusEnabled)
            track.fx.chorus.process(context);

        // Delay (only if enabled and mix > 0)
        if (track.fx.delayEnabled && track.fx.delayMix > 0.0f)
        {
            auto* left = trackBuffer.getWritePointer(0);
            auto* right = trackBuffer.getWritePointer(1);
            
            for(int i = 0; i < numSamples; ++i)
            {
                float inL = left[i];
                float inR = right[i];
                
                float wetL = track.fx.delay.popSample(0);
                float wetR = track.fx.delay.popSample(1);
                
                left[i] = inL * (1.0f - track.fx.delayMix) + wetL * track.fx.delayMix;
                right[i] = inR * (1.0f - track.fx.delayMix) + wetR * track.fx.delayMix;
                
                float feedL = inL + wetL * track.fx.delayFeedback;
                float feedR = inR + wetR * track.fx.delayFeedback;
                
                feedL = std::tanh(feedL);
                feedR = std::tanh(feedR);
                
                track.fx.delay.pushSample(0, feedL);
                track.fx.delay.pushSample(1, feedR);
            }
        }
        
        // Reverb (only if enabled)
        if (track.fx.reverbEnabled)
            track.fx.reverb.process(context);
        
        // Add FX-processed track to final output
        for (int ch = 0; ch < outChannels; ++ch)
        {
            output.addFrom(ch, 0, trackBuffer, ch % 2, 0, numSamples);
        }

        // --- Visualization Monitoring ---
        if (id == monitorTrackId.load())
        {
            // モノラルミックスしてFIFOへ
            int start1, size1, start2, size2;
            monitorFifo.prepareToWrite(numSamples, start1, size1, start2, size2);
            
            if (size1 > 0)
            {
                // Channel 0 only for simplified viz
                for (int i = 0; i < size1; ++i)
                    monitorFifoBuffer[start1 + i] = trackBuffer.getSample(0, i);
            }
            if (size2 > 0)
            {
                for (int i = 0; i < size2; ++i)
                    monitorFifoBuffer[start2 + i] = trackBuffer.getSample(0, size1 + i);
            }
            monitorFifo.finishedWrite(size1 + size2);
        }

        // 🧮 RMS計算 (Visualizer用)
        // FX適用後の trackBuffer から計算する（ブロック全体のRMS）
        float rmsValue = 0.0f;
        if (numSamples > 0)
        {
            rmsValue = trackBuffer.getRMSLevel(0, 0, numSamples);
            // 2chの場合は平均
            if (trackBuffer.getNumChannels() > 1)
            {
                rmsValue = (rmsValue + trackBuffer.getRMSLevel(1, 0, numSamples)) * 0.5f;
            }
        }
        
        rmsValue *= track.gain;
        constexpr float decayRate = 0.95f;
        if (rmsValue > track.currentLevel)
            track.currentLevel = rmsValue;
        else
            track.currentLevel = track.currentLevel * decayRate + rmsValue * (1.0f - decayRate);
    } // End track loop

    // 再生中または録音中のトラックが1つでもあるかチェック
    bool isActive = isAnyPlaying() || isAnyRecording();

    // マスターが決まっていて、かつ「誰かが動いている時だけ」時間を進める
    if (masterLoopLength > 0 && isActive)
    {
        masterReadPosition = (masterReadPosition + numSamples) % masterLoopLength;
    }
}

void LooperAudio::backupTrackBeforeRecord(int trackId)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
    {
        lastHistory = TrackHistory();
        lastHistory->trackId = trackId;
        lastHistory->previousBuffer.makeCopyOf(it->second.buffer);

        DBG("💾 Backup created for track " << trackId);
    }
}

int LooperAudio::undoLastRecording()
{
    if (!lastHistory.has_value())
    {
        DBG("⚠️ Nothing to undo");
        return -1;
    }

    auto& history = lastHistory.value();
    int undoneTrackId = history.trackId;
    
    if (auto it = tracks.find(history.trackId); it != tracks.end())
    {
        it->second.buffer.makeCopyOf(history.previousBuffer);
        it->second.isRecording = false;
        it->second.isPlaying = false;
        it->second.writePosition = 0;
        it->second.recordLength = history.previousBuffer.getNumSamples();

        DBG("↩️ Undo applied to track " << history.trackId);
    }
    lastHistory.reset();
    return undoneTrackId;
}

void LooperAudio::allClear()
{
    for (auto& [id, track] : tracks)
    {
        track.buffer.clear();
        track.isPlaying = false;
        track.isRecording = false;
        track.writePosition = 0;
        track.readPosition = 0;
        track.recordLength = 0;
        track.loopMultiplier = 1.0f; // Multiplierもリセット
    }
    masterTrackId = -1;
    masterLoopLength = 0;
    masterReadPosition = 0;

    DBG("🧹 LooperAudio::clearAll() → All buffers cleared");
}

void LooperAudio::stopAllTracks()
{
    for (auto& [id, track] : tracks)
    {
        track.isRecording = false;
        track.isPlaying = false;
        track.readPosition = 0; // 停止時に読み込み位置を先頭に戻す
    }
    masterReadPosition = 0;
}

int LooperAudio::getCurrentTrackId() const
{
    if (currentRecordingIndex >= 0 && currentRecordingIndex < (int)recordingQueue.size())
        return recordingQueue[currentRecordingIndex];

    if (!tracks.empty())
        return tracks.begin()->first;

    return -1;
}

bool LooperAudio::isAnyRecording() const
{
    return anyTrackSatisfies(tracks, [](const auto& t) { return t.isRecording; });
}

bool LooperAudio::isAnyPlaying() const
{
    return anyTrackSatisfies(tracks, [](const auto& t) { return t.isPlaying; });
}

bool LooperAudio::hasRecordedTracks() const
{
    for (const auto& [id, track] : tracks)
        if (track.recordLength > 0) return true;
    return false;
}



void LooperAudio::setTrackGain(int trackId, float gain)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
        it->second.gain = gain;
}

void LooperAudio::generateTestClick(int trackId)
{
    auto it = tracks.find(trackId);
    if (it == tracks.end()) return;
    
    auto& track = it->second;
    
    const int samplesPerBeat = static_cast<int>(sampleRate * 0.5);
    const int numBeats = 4;
    const int totalSamples = samplesPerBeat * numBeats;
    
    const float clickFrequency = 1000.0f;  
    const int clickDuration = static_cast<int>(sampleRate * 0.02); 
    
    track.buffer.clear();
    
    for (int beat = 0; beat < numBeats; ++beat)
    {
        int beatStart = beat * samplesPerBeat;
        
        for (int i = 0; i < clickDuration && (beatStart + i) < track.buffer.getNumSamples(); ++i)
        {
            float envelope = std::exp(-5.0f * (float)i / (float)clickDuration);
            float phase = juce::MathConstants<float>::twoPi * clickFrequency * (float)i / (float)sampleRate;
            float sample = std::sin(phase) * envelope * 0.8f;
            
            for (int ch = 0; ch < track.buffer.getNumChannels(); ++ch)
            {
                track.buffer.setSample(ch, beatStart + i, sample);
            }
        }
    }
    
    track.recordLength = totalSamples;
    track.lengthInSample = totalSamples;
    track.readPosition = 0;
    track.isPlaying = true;
    track.isRecording = false;
    
    if (masterLoopLength == 0)
    {
        masterLoopLength = totalSamples;
        masterStartSample = 0;
        masterReadPosition = 0;
        DBG("🎛 Master loop set from test click: " << totalSamples << " samples");
    }
    
    DBG("🔊 Test click generated for track " << trackId << " | " << numBeats << " beats @ 120BPM");
    listeners.call([trackId](Listener& l) { l.onRecordingStopped(trackId); });
}

void LooperAudio::generateTestWaveformsForVisualTest()
{
    // 120BPM = 0.5秒/ビート、4ビート = 2秒がマスターループ
    const int samplesPerBeat = static_cast<int>(sampleRate * 0.5);
    const int masterSamples = samplesPerBeat * 4;  // マスター: 4拍
    
    // クリック音のパラメータ
    const float clickFrequency = 1000.0f;
    const int clickDuration = static_cast<int>(sampleRate * 0.02);
    
    auto generateClick = [this, clickFrequency, clickDuration](juce::AudioBuffer<float>& buffer, int position) {
        for (int i = 0; i < clickDuration && (position + i) < buffer.getNumSamples(); ++i)
        {
            float envelope = std::exp(-5.0f * (float)i / (float)clickDuration);
            float phase = juce::MathConstants<float>::twoPi * clickFrequency * (float)i / (float)sampleRate;
            float sample = std::sin(phase) * envelope * 0.8f;
            
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.setSample(ch, position + i, sample);
        }
    };
    
    // ===== トラック1: マスター（等倍）=====
    {
        auto& track = tracks[1];
        track.buffer.setSize(2, masterSamples);
        track.buffer.clear();
        
        // 4拍のクリック音（各拍の先頭）
        for (int beat = 0; beat < 4; ++beat)
            generateClick(track.buffer, beat * samplesPerBeat);
        
        track.recordLength = masterSamples;
        track.lengthInSample = masterSamples;
        track.recordStartSample = 0;
        track.loopMultiplier = 1.0f;
        track.readPosition = 0;
        track.isPlaying = true;
        track.isRecording = false;
        
        masterLoopLength = masterSamples;
        masterStartSample = 0;
        masterReadPosition = 0;
        masterTrackId = 1;
        
        DBG("🎵 Track 1 (Master x1): " << masterSamples << " samples, 4 clicks");
        listeners.call([](Listener& l) { l.onRecordingStopped(1); });
    }
    
    // ===== トラック2: x2（先頭にクリック）=====
    {
        auto& track = tracks[2];
        int x2Samples = masterSamples * 2;  // x2 = 8拍分
        track.buffer.setSize(2, x2Samples);
        track.buffer.clear();
        
        // 先頭にクリック（x2ループの開始点を示す）
        generateClick(track.buffer, 0);
        
        // マスターループ2周目の先頭にもクリック（2周目開始を示す）
        generateClick(track.buffer, masterSamples);
        
        track.recordLength = x2Samples;
        track.lengthInSample = x2Samples;
        track.recordStartSample = 0;  // バッファ先頭から録音開始
        track.loopMultiplier = 2.0f;
        track.readPosition = 0;
        track.isPlaying = true;
        track.isRecording = false;
        
        DBG("🎵 Track 2 (x2): " << x2Samples << " samples, clicks at 0 and " << masterSamples);
        listeners.call([](Listener& l) { l.onRecordingStopped(2); });
    }
    
    // ===== トラック3: /2（先頭にクリック）=====
    {
        auto& track = tracks[3];
        int halfSamples = masterSamples / 2;  // /2 = 2拍分
        track.buffer.setSize(2, halfSamples);
        track.buffer.clear();
        
        // 先頭にクリック（/2ループの開始点を示す）
        generateClick(track.buffer, 0);
        
        track.recordLength = halfSamples;
        track.lengthInSample = halfSamples;
        track.recordStartSample = 0;  // バッファ先頭から録音開始
        track.loopMultiplier = 0.5f;
        track.readPosition = 0;
        track.isPlaying = true;
        track.isRecording = false;
        
        DBG("🎵 Track 3 (/2): " << halfSamples << " samples, click at 0");
        listeners.call([](Listener& l) { l.onRecordingStopped(3); });
    }
    
    // ===== トラック4: x1 (2拍目から録音開始、長さは1周分) =====
    {
        auto& track = tracks[4];
        track.buffer.setSize(2, masterSamples); // フル尺確保
        track.buffer.clear();
        
        // 録音開始直後（バッファ先頭）にクリック
        generateClick(track.buffer, 0);
        
        track.recordLength = masterSamples;
        track.lengthInSample = masterSamples;
        // 2拍目（1拍終わったところ）から録音開始
        track.recordStartSample = samplesPerBeat; 
        track.loopMultiplier = 1.0f;
        track.readPosition = 0;
        track.isPlaying = true;
        track.isRecording = false;
        
        DBG("🎵 Track 4 (x1, Start@Beat2): click at buffer start, len: " << masterSamples);
        listeners.call([](Listener& l) { l.onRecordingStopped(4); });
    }

    // ===== トラック5: x2 (2拍目から録音開始、長さはx2周分) =====
    {
        auto& track = tracks[5];
        int x2Samples = masterSamples * 2;
        track.buffer.setSize(2, x2Samples); // フル尺確保
        track.buffer.clear();
        
        // 最初の小節の2拍目（バッファ先頭）にのみクリック。2小節目（後半）は無音。
        generateClick(track.buffer, 0);
        
        track.recordLength = x2Samples;
        track.lengthInSample = x2Samples;
        track.recordStartSample = samplesPerBeat; 
        track.loopMultiplier = 2.0f;
        track.readPosition = 0;
        track.isPlaying = true;
        track.isRecording = false;
        
        DBG("🎵 Track 5 (x2, Start@Beat2): click at buffer start, len: " << x2Samples);
        listeners.call([](Listener& l) { l.onRecordingStopped(5); });
    }

    // ===== トラック6: /2 (2拍目から録音開始、長さは/2周分) =====
    {
        auto& track = tracks[6];
        int halfSamples = masterSamples / 2;
        track.buffer.setSize(2, halfSamples); // フル尺確保
        track.buffer.clear();
        
        generateClick(track.buffer, 0);
        
        track.recordLength = halfSamples;
        track.lengthInSample = halfSamples;
        track.recordStartSample = samplesPerBeat; 
        track.loopMultiplier = 0.5f;
        track.readPosition = 0;
        track.isPlaying = true;
        track.isRecording = false;
        
        DBG("🎵 Track 6 (/2, Start@Beat2): click at buffer start, len: " << halfSamples);
        listeners.call([](Listener& l) { l.onRecordingStopped(6); });
    }

    // ===== トラック7: x2 (2小節目の4拍目から録音開始) =====
    {
        auto& track = tracks[7];
        int x2Samples = masterSamples * 2;
        track.buffer.setSize(2, x2Samples);
        track.buffer.clear();
        
        // 録音開始直後（バッファ先頭）にクリック
        generateClick(track.buffer, 0);
        
        track.recordLength = x2Samples;
        track.lengthInSample = x2Samples;
        
        // 2小節目の4拍目 = 通算8拍目(index 7)
        // Master(4拍)内での位置は 7 % 4 = 3 (4拍目)
        track.recordStartSample = samplesPerBeat * 7; // Start@Bar2-Beat4 
        
        track.loopMultiplier = 2.0f;
        track.readPosition = 0;
        track.isPlaying = true;
        track.isRecording = false;
        
        DBG("🎵 Track 7 (x2, Start@Bar2-Beat4): click at buffer start");
        listeners.call([](Listener& l) { l.onRecordingStopped(7); });
    }

    // ===== トラック8: /2 (2小節目の4拍目から録音開始) =====
    {
        auto& track = tracks[8];
        int halfSamples = masterSamples / 2;
        track.buffer.setSize(2, halfSamples);
        track.buffer.clear();
        
        generateClick(track.buffer, 0);
        
        track.recordLength = halfSamples;
        track.lengthInSample = halfSamples;
        
        // 2小節目の4拍目 = 通算8拍目(index 7)
        // Master(4拍)内での位置は 7 % 4 = 3 (4拍目)
        track.recordStartSample = samplesPerBeat * 7; 

        track.loopMultiplier = 0.5f;
        track.readPosition = 0;
        track.isPlaying = true;
        track.isRecording = false;
        
        DBG("🎵 Track 8 (/2, Start@Bar2-Beat4): click at buffer start");
        listeners.call([](Listener& l) { l.onRecordingStopped(8); });
    }
    
    DBG("✅ Visual test waveforms generated: T1-3(Full), T4-6(Punch-in @ Beat2), T7-8(Punch-in @ Bar2-Beat4)");
}

void LooperAudio::setTrackFilterCutoff(int trackId, float freq)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
        it->second.fx.filter.setCutoffFrequency(freq);
}

void LooperAudio::setTrackFilterResonance(int trackId, float q)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
        it->second.fx.filter.setResonance(q);
}

void LooperAudio::setTrackFilterType(int trackId, int type)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
    {
        if(type == 0) it->second.fx.filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        else if(type == 1) it->second.fx.filter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    }
}

void LooperAudio::setTrackCompressor(int trackId, float threshold, float ratio)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
    {
        it->second.fx.compressor.setThreshold(threshold);
        it->second.fx.compressor.setRatio(ratio);
    }
}

void LooperAudio::setTrackDelayMix(int trackId, float mix, float time)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
    {
        it->second.fx.delayMix = mix;
        
        float maxDelay = sampleRate * 1.0f;
        float delaySamples = time * maxDelay;
        if(delaySamples < 1.0f) delaySamples = 1.0f;
        
        it->second.fx.delay.setDelay(delaySamples);
    }
}

void LooperAudio::setTrackDelayFeedback(int trackId, float feedback)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
        it->second.fx.delayFeedback = feedback;
}

void LooperAudio::setTrackReverbMix(int trackId, float mix)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
    {
        it->second.fx.reverbMix = mix;
        juce::dsp::Reverb::Parameters params = it->second.fx.reverb.getParameters();
        params.dryLevel = 1.0f - (mix * 0.5f);
        params.wetLevel = mix;
        it->second.fx.reverb.setParameters(params);
    }
}

void LooperAudio::setTrackReverbDamping(int trackId, float damping)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
    {
        juce::dsp::Reverb::Parameters params = it->second.fx.reverb.getParameters();
        params.damping = damping;
        it->second.fx.reverb.setParameters(params);
    }
}

void LooperAudio::setTrackReverbRoomSize(int trackId, float size)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
    {
        juce::dsp::Reverb::Parameters params = it->second.fx.reverb.getParameters();
        params.roomSize = size;
        it->second.fx.reverb.setParameters(params);
    }
}

// ================= Beat Repeat Setters =================

void LooperAudio::setTrackBeatRepeatActive(int trackId, bool active)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
    {
        it->second.fx.beatRepeat.isActive = active;
        if (!active)
            it->second.fx.beatRepeat.isRepeating = false;
    }
}

void LooperAudio::setTrackBeatRepeatDiv(int trackId, int div)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
        it->second.fx.beatRepeat.division = juce::jmax(1, div);
}

void LooperAudio::setTrackBeatRepeatThresh(int trackId, float thresh)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
        it->second.fx.beatRepeat.threshold = thresh;
}

// ================= Monitor / Visualization =================

void LooperAudio::setMonitorTrackId(int trackId)
{
    monitorTrackId.store(trackId);
}

void LooperAudio::popMonitorSamples(juce::AudioBuffer<float>& destBuffer)
{
    const int numSamples = destBuffer.getNumSamples();
    int start1, size1, start2, size2;
    monitorFifo.prepareToRead(numSamples, start1, size1, start2, size2);
    
    if (size1 > 0)
        destBuffer.copyFrom(0, 0, monitorFifoBuffer.data() + start1, size1);
    
    if (size2 > 0)
        destBuffer.copyFrom(0, size1, monitorFifoBuffer.data() + start2, size2);
        
    monitorFifo.finishedRead(size1 + size2);
    
    // データが足りない場合はゼロ埋め（または前回の値を維持するか、ここでゼロクリアするか）
    // AbstractFifoは読み込めた分だけ返すので、足りない分はクリアしておく方が安全
    if (size1 + size2 < numSamples)
    {
        destBuffer.clear(size1 + size2, numSamples - (size1 + size2));
    }
}

// ================= FX Enable/Disable =================

void LooperAudio::setTrackFilterEnabled(int trackId, bool enabled)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
        it->second.fx.filterEnabled = enabled;
}



void LooperAudio::setTrackFlangerEnabled(int trackId, bool enabled)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
        it->second.fx.flangerEnabled = enabled;
}

void LooperAudio::setTrackFlangerRate(int trackId, float rate)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
        it->second.fx.flanger.setRate(rate);
}

void LooperAudio::setTrackFlangerDepth(int trackId, float depth)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
        it->second.fx.flanger.setDepth(depth);
}

void LooperAudio::setTrackFlangerFeedback(int trackId, float feedback)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
        it->second.fx.flanger.setFeedback(feedback);
}

void LooperAudio::setTrackChorusEnabled(int trackId, bool enabled)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
        it->second.fx.chorusEnabled = enabled;
}

void LooperAudio::setTrackChorusRate(int trackId, float rate)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
        it->second.fx.chorus.setRate(rate);
}

void LooperAudio::setTrackChorusDepth(int trackId, float depth)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
        it->second.fx.chorus.setDepth(depth);
}

void LooperAudio::setTrackChorusMix(int trackId, float mix)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
        it->second.fx.chorus.setMix(mix);
}

void LooperAudio::setTrackDelayEnabled(int trackId, bool enabled)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
        it->second.fx.delayEnabled = enabled;
}

void LooperAudio::setTrackReverbEnabled(int trackId, bool enabled)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
        it->second.fx.reverbEnabled = enabled;
}

void LooperAudio::setTrackLoopMultiplier(int trackId, float multiplier)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
    {
        it->second.loopMultiplier = multiplier;
        
        // 再生位置を現在の絶対時刻に合わせて再計算（x2切り替え時のズレ防止）
        if (masterLoopLength > 0)
        {
            int64_t relativePos = currentSamplePosition - masterStartSample;
            int effectiveLoopLength = (int)(masterLoopLength * multiplier);
            if (effectiveLoopLength > 0)
            {
                it->second.readPosition = (int)(relativePos % effectiveLoopLength);
            }
        }
        
        DBG("Track " << trackId << " loop multiplier set to " << multiplier << " | ReadPos adjusted to " << it->second.readPosition);
    }
}
