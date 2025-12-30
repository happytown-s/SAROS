#include "LooperAudio.h"
#include <juce_events/juce_events.h>

LooperAudio::LooperAudio(double sr, int max)
    : sampleRate(sr), maxSamples(max)
{
}

LooperAudio::~LooperAudio()
{
    listeners.clear();
}

void LooperAudio::prepareToPlay(int samplesPerBlockExpected, double sr)
{
    sampleRate = sr;
}

void LooperAudio::processBlock(juce::AudioBuffer<float>& output,
                               const juce::AudioBuffer<float>& input)
{
    // 録音・再生処理
    output.clear();
    recordIntoTracks(input);
    mixTracksToOutput(output);

    // 入力音をモニター出力
    const int numChannels = juce::jmin(input.getNumChannels(), output.getNumChannels());
    const int numSamples = input.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        output.addFrom(ch, 0, input, ch, 0, numSamples);
    }
}

//------------------------------------------------------------
// トラック管理
void LooperAudio::addTrack(int trackId)
{
    TrackData track;
    track.buffer.setSize(2, maxSamples);
    track.buffer.clear();
    tracks[trackId] = std::move(track);
}

void LooperAudio::startRecording(int trackId)
{
    // 履歴に追加
    backupTrackBeforeRecord(trackId);

    auto& track = tracks[trackId];
    track.isRecording = true;
    track.isPlaying = false;
    track.recordLength = 0;

    // マスターが再生中なら、その位置から録音開始
    if (masterLoopLength > 0 && tracks.find(masterTrackId) != tracks.end() && tracks[masterTrackId].isPlaying)
    {
        // マスターの位置に同期させる
        track.writePosition = masterReadPosition;
        track.recordStartSample = masterReadPosition;
        DBG("🎬 Start recording track " << trackId
            << " aligned with master at position " << masterReadPosition);
    }
    // TriggerEventが有効なら記録開始位置として反映
    else if (triggerRef && triggerRef->triggerd)
    {
        track.recordStartSample = static_cast<int>(triggerRef->absIndex);
        track.writePosition = juce::jlimit(0, maxSamples - 1, (int)triggerRef->absIndex);
        DBG("🎬 Start recording track " << trackId
            << " triggered at " << triggerRef->absIndex);
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

//------------------------------------------------------------

void LooperAudio::stopRecording(int trackId)
{
    auto& track = tracks[trackId];
    track.isRecording = false;

    // 現在の録音長を保持
    const int recordedLength = track.recordLength; // 修正: writePositionではなくrecordLengthを使用
    if (recordedLength <= 0) return;

    if (masterLoopLength <= 0)
    {
        // 録音長をそのままマスター長に採用
        masterTrackId = trackId;
        masterLoopLength = recordedLength;
        track.lengthInSample = masterLoopLength;
        masterStartSample = track.recordStartSample;

        DBG("🎛 Master loop length set to " << masterLoopLength
            << " samples | recorded=" << recordedLength
            << " | masterStart=" << masterStartSample);
    }
    else
    {
        // マスター長に合わせてバッファを整列させる処理
        juce::AudioBuffer<float> aligned;
        aligned.setSize(2, masterLoopLength, false, false, true);
        aligned.clear();

        // 単純コピーではなく、循環バッファの展開が必要な場合があるが、
        // ここでは簡易的に先頭からのコピーとしている（用途による）
        const int copyLen = juce::jmin(recordedLength, masterLoopLength);
        
        // 注: バッファがラップしている可能性を考慮して copyFrom を使うべきですが、
        // ここでは一旦単純化しています。本来は recordIntoTracks と同様の周回コピーが必要です。
        aligned.copyFrom(0, 0, track.buffer, 0, 0, copyLen);
        aligned.copyFrom(1, 0, track.buffer, 1, 0, copyLen);

        // 🎯 整列済みループを保存
        track.buffer.makeCopyOf(aligned);
        track.lengthInSample = masterLoopLength;
        track.recordLength = copyLen;

        DBG("🟢 Track " << trackId << ": aligned to master (length " << masterLoopLength << ")");
    }

    listeners.call([&](Listener& l) { l.onRecordingStopped(trackId); });
}

void LooperAudio::startPlaying(int trackId)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
    {
        auto& track = it->second;
        track.isPlaying = true;

        // 🔥 再生開始位置をマスター位置に合わせる
        if (masterLoopLength > 0)
        {
            track.readPosition = masterReadPosition % masterLoopLength;
        }
        else
        {
            track.readPosition = 0;
        }

        DBG("▶️ Start playing track " << trackId
            << " aligned to master at " << track.readPosition);
    }
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

//==============================================================================
// 🛠️ 【修正箇所】録音処理 (Wraparound対応 & 初期録音対応)
//==============================================================================
void LooperAudio::recordIntoTracks(const juce::AudioBuffer<float>& input)
{
    const int numSamples = input.getNumSamples();

    for (auto& [id, track] : tracks)
    {
        if (!track.isRecording)
            continue;

        const int numChannels = juce::jmin(input.getNumChannels(), track.buffer.getNumChannels());
        
        // バッファの物理的な限界、またはマスターの長さ
        // まだマスターが決まっていない場合は、トラックのバッファ最大長をリミットとする
        const int loopLimit = (masterLoopLength > 0) ? masterLoopLength : track.buffer.getNumSamples();

        if (loopLimit == 0) continue; // 安全策

        // 書き込み開始位置の決定
        int currentWritePos;
        if (masterLoopLength > 0)
        {
            // マスター同期中
            currentWritePos = masterReadPosition % loopLimit;
        }
        else
        {
            // 最初のトラック録音中 (リニア進行)
            // track.recordLength を現在のヘッド位置として扱う
            currentWritePos = track.recordLength % loopLimit;
        }

        // --- 🔄 ラップアラウンド対応書き込みループ ---
        int samplesRemaining = numSamples;

        // 【修正】マスター同期録音の場合は、ループ長を超えて録音しないように制限する
        if (masterLoopLength > 0)
        {
            int maxRecordable = masterLoopLength - track.recordLength;
            if (maxRecordable < 0) maxRecordable = 0;
            samplesRemaining = juce::jmin(samplesRemaining, maxRecordable);
        }

        int inputReadOffset = 0;

        while (samplesRemaining > 0)
        {
            // バッファ終端までの距離
            const int samplesToEnd = loopLimit - currentWritePos;
            const int samplesToCopy = juce::jmin(samplesRemaining, samplesToEnd);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                // inputOffset を使って入力バッファの正しい位置からコピー
                track.buffer.copyFrom(ch, currentWritePos, input, ch, inputReadOffset, samplesToCopy);
            }

            // ポインタ更新
            currentWritePos = (currentWritePos + samplesToCopy) % loopLimit;
            inputReadOffset += samplesToCopy;
            samplesRemaining -= samplesToCopy;
            
            // 録音長を更新
            track.recordLength = juce::jmin(track.recordLength + samplesToCopy, loopLimit);
        }

        // トラックの状態に最終的な位置を保存（必要であれば）
        track.writePosition = currentWritePos;

        // ✅ マスター同期モードの場合、1周録音完了で停止して再生へ
        if (masterLoopLength > 0 && track.recordLength >= masterLoopLength)
        {
            stopRecording(id);
            startPlaying(id);
            DBG("✅ Master-synced loop complete for Track " << id
                << " | length=" << masterLoopLength);
        }
    }
}

//==============================================================================
// 再生処理 (ミックス)
//==============================================================================
void LooperAudio::mixTracksToOutput(juce::AudioBuffer<float>& output)
{
    const int numSamples = output.getNumSamples();

    for (auto& [id, track] : tracks)
    {
        if (!track.isPlaying)
        {
            // 停止中はレベルを減衰させてゼロにする
            track.currentLevel *= 0.8f;
            if (track.currentLevel < 0.001f) track.currentLevel = 0.0f;
            continue;
        }

        const int numChannels = juce::jmin(output.getNumChannels(), track.buffer.getNumChannels());
        
        const int loopLength = (masterLoopLength > 0)
            ? masterLoopLength
            : juce::jmax(1, track.recordLength > 0 ? track.recordLength : track.buffer.getNumSamples());

        int readPos = track.readPosition;
        int remaining = numSamples;
        int outputOffset = 0;

        // 🔄 再生ラップアラウンドループ
        while (remaining > 0)
        {
            const int samplesToEnd = loopLength - readPos;
            const int samplesToCopy = juce::jmin(remaining, samplesToEnd);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                // 出力の現在の位置 (numSamples - remaining は間違いやすいので outputOffset を使用)
                // ゲインを適用して加算
                output.addFrom(ch, outputOffset, track.buffer, ch, readPos, samplesToCopy, track.gain);
            }

            readPos = (readPos + samplesToCopy) % loopLength;
            remaining -= samplesToCopy;
            outputOffset += samplesToCopy;
        }

        track.readPosition = readPos;

        // 🧮 RMS計算（wrapを考慮）
        // 計算負荷軽減のため、簡易的にラップアラウンド後の位置周辺で計算
        // 正確にまたぐ計算が必要な場合は修正が必要だが、UI表示用ならこれで十分
        const int rmsWindow = 256;
        int rmsStart = (readPos - rmsWindow + loopLength) % loopLength; 
        
        // バッファ終端をまたぐ可能性があるため、安全策として getRMSLevel を2回呼ぶか、
        // 簡易的に読み出し位置の直前を使う
        // ここでは単純化のため、現在の読み出し位置の手前 window 分を取得（ラップ考慮なし）で妥協するか、
        // ちゃんと分割するか。既存コードのロジックを整理して記述：
        
        float rmsValue = 0.0f;
        if (rmsStart + rmsWindow <= loopLength)
        {
             rmsValue = track.buffer.getRMSLevel(0, rmsStart, rmsWindow);
        }
        else
        {
            // またぐ場合
            int part1 = loopLength - rmsStart;
            int part2 = rmsWindow - part1;
            float r1 = track.buffer.getRMSLevel(0, rmsStart, part1);
            float r2 = track.buffer.getRMSLevel(0, 0, part2);
            rmsValue = (r1 + r2) * 0.5f; // 簡易平均
        }
        // Apply decay smoothing: rise immediately, fall slowly
        constexpr float decayRate = 0.95f;  // Higher = slower decay
        if (rmsValue > track.currentLevel)
            track.currentLevel = rmsValue;
        else
            track.currentLevel = track.currentLevel * decayRate + rmsValue * (1.0f - decayRate);
    }


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

void LooperAudio::undoLastRecording()
{
    if (!lastHistory.has_value())
    {
        DBG("⚠️ Nothing to undo");
        return;
    }

    auto& history = lastHistory.value();
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
		
    }
	masterReadPosition = 0;
	//DBG("⏹ LooperAudio::stopAllTracks() → All tracks stopped");
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

float LooperAudio::getTrackRMS(int trackId) const
{
    if (auto it = tracks.find(trackId); it != tracks.end())
        return it->second.currentLevel;
    return 0.0f;
}

void LooperAudio::setTrackGain(int trackId, float gain)
{
    if (auto it = tracks.find(trackId); it != tracks.end())
        it->second.gain = gain;
}