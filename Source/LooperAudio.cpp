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
    
    // Store spec for per-track FX initialization
    fxSpec.sampleRate = sampleRate;
    fxSpec.maximumBlockSize = samplesPerBlockExpected;
    fxSpec.numChannels = 2;
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
    }
}



void LooperAudio::startRecording(int trackId)
{
    // 履歴に追加
    backupTrackBeforeRecord(trackId);

    auto& track = tracks[trackId];
    
    // Safety: Ensure buffer is full size if we are defining a new master loop
    if (masterLoopLength <= 0 && track.buffer.getNumSamples() < maxSamples)
    {
        track.buffer.setSize(2, maxSamples);
        DBG("🔧 Resized Track " << trackId << " buffer to maxSamples (" << maxSamples << ")");
    }
    // Optimization/Safety: If Slave, ensure at least Master Length
    else if (masterLoopLength > 0 && track.buffer.getNumSamples() < masterLoopLength)
    {
        track.buffer.setSize(2, masterLoopLength);
        DBG("🔧 Resized Track " << trackId << " buffer to masterLoopLength (" << masterLoopLength << ")");
    }

    track.isRecording = true;
    track.isPlaying = false;
    track.recordLength = 0;

    // マスターが再生中なら、その位置から録音開始
    if (masterLoopLength > 0 && tracks.find(masterTrackId) != tracks.end() && tracks[masterTrackId].isPlaying)
    {
        // マスターの位置に同期させる
        track.writePosition = masterReadPosition;
        track.recordStartSample = masterReadPosition;
        track.recordingStartPhase = masterReadPosition;
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

void LooperAudio::startRecordingWithLookback(int trackId, const juce::AudioBuffer<float>& lookbackData)
{
    // First, standard start
    startRecording(trackId);

    if (auto it = tracks.find(trackId); it != tracks.end())
    {
        auto& track = it->second;
        int numLookback = lookbackData.getNumSamples();
        if (numLookback <= 0) return;

        // Loop limit definition
        const int loopLimit = (masterLoopLength > 0) ? masterLoopLength : maxSamples;

        // Calculate write start position (go back in time)
        int startWritePos = track.writePosition - numLookback;
        while (startWritePos < 0) startWritePos += loopLimit;

        // Limit lookback to loop size (sanity check)
        int samplesToCopy = numLookback;
        if (masterLoopLength > 0 && samplesToCopy > masterLoopLength)
            samplesToCopy = masterLoopLength;

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
        }

        DBG("🔙 Lookback injected: " << samplesToCopy << " samples. Adjusted start: " << track.recordStartSample);
    }
}

void LooperAudio::stopRecording(int trackId)
{
    auto& track = tracks[trackId];
    track.isRecording = false;

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

        DBG("🎛 Master loop length set to " << masterLoopLength
            << " samples | recorded=" << recordedLength
            << " | masterStart=" << masterStartSample
            << " | readPos reset to 0");
    }
    else
    {
        juce::AudioBuffer<float> aligned;
        aligned.setSize(2, masterLoopLength, false, false, true);
        aligned.clear();

        const int copyLen = masterLoopLength;
        
        aligned.copyFrom(0, 0, track.buffer, 0, 0, copyLen);
        aligned.copyFrom(1, 0, track.buffer, 1, 0, copyLen);

        track.buffer.makeCopyOf(aligned);
        track.lengthInSample = masterLoopLength;
        track.recordLength = recordedLength; 

        track.recordStartSample = masterStartSample;

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

void LooperAudio::recordIntoTracks(const juce::AudioBuffer<float>& input)
{
    const int numSamples = input.getNumSamples();

    for (auto& [id, track] : tracks)
    {
        if (!track.isRecording)
            continue;

        const int numChannels = juce::jmin(input.getNumChannels(), track.buffer.getNumChannels());
        
        const int loopLimit = (masterLoopLength > 0) ? masterLoopLength : track.buffer.getNumSamples();

        if (loopLimit == 0) continue; 

        int currentWritePos;
        if (masterLoopLength > 0)
        {
            currentWritePos = masterReadPosition % loopLimit;
        }
        else
        {
            currentWritePos = track.recordLength % loopLimit;
        }

        int samplesRemaining = numSamples;

        if (masterLoopLength > 0)
        {
            int maxRecordable = masterLoopLength - track.recordLength;
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

        if (masterLoopLength > 0 && track.recordLength >= masterLoopLength)
        {
            stopRecording(id);
            startPlaying(id);
            DBG("✅ Master-synced loop complete for Track " << id
                << " | length=" << masterLoopLength);
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

        const int numChannels = juce::jmin(output.getNumChannels(), track.buffer.getNumChannels());
        
        const int loopLength = (masterLoopLength > 0)
            ? masterLoopLength
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

            for (int ch = 0; ch < numChannels; ++ch)
            {
                trackBuffer.addFrom(ch, outputOffset, track.buffer, ch, readPos, samplesToCopy, track.gain);
            }

            readPos = (readPos + samplesToCopy) % loopLength;
            remaining -= samplesToCopy;
            outputOffset += samplesToCopy;
        }

        track.readPosition = readPos;
        
        // ============ Per-Track FX Processing ============
        juce::dsp::AudioBlock<float> block(trackBuffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        
        // Filter
        track.fx.filter.process(context);
        
        // Delay (Manual Feedback)
        if (track.fx.delayMix > 0.0f)
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
        
        // Reverb
        track.fx.reverb.process(context);
        
        // Add FX-processed track to final output
        for (int ch = 0; ch < numChannels; ++ch)
        {
            output.addFrom(ch, 0, trackBuffer, ch, 0, numSamples);
        }

        // 🧮 RMS計算
        const int rmsWindow = 256;
        int rmsStart = (readPos - rmsWindow + loopLength) % loopLength; 
        
        float rmsValue = 0.0f;
        if (rmsStart + rmsWindow <= loopLength)
        {
             rmsValue = track.buffer.getRMSLevel(0, rmsStart, rmsWindow);
        }
        else
        {
            int part1 = loopLength - rmsStart;
            int part2 = rmsWindow - part1;
            float r1 = track.buffer.getRMSLevel(0, rmsStart, part1);
            float r2 = track.buffer.getRMSLevel(0, 0, part2);
            rmsValue = (r1 + r2) * 0.5f; 
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

// ================= FX Setters (Per-Track) =================

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