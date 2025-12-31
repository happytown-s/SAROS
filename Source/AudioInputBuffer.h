/*
  ==============================================================================

    AudioInputBuffer.h
    Created: 30 Dec 2025
    Author:  mt sh

  ==============================================================================
*/

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <atomic>

class AudioInputBuffer
{
public:
    AudioInputBuffer() = default;
    
    void prepare(double sampleRate, int bufferSizeSeconds)
    {
        bufferSize = (int)(sampleRate * bufferSizeSeconds);
        ringBuffer.resize(bufferSize, 0.0f);
        writePos = 0;
        // Reset state
        potentialStartIndex = -1;
        inPreRoll = false;
        silenceCounter = 0;
        this->sampleRate = sampleRate;
    }
    
    // Write new input samples to the ring buffer
    void write(const float* input, int numSamples)
    {
        if (bufferSize == 0) return;
        
        for (int i = 0; i < numSamples; ++i)
        {
            ringBuffer[writePos] = input[i];
            
            // Advance write position
            writePos = (writePos + 1) % bufferSize;
        }
        
        // 現在のブロックサイズを記録（getLookbackData で除外するため）
        lastWrittenBlockSize = numSamples;
    }
    
    // Check triggers and return true if high threshold detected
    // Updates internal state for low threshold tracking
    bool processTriggers(const float* input, int numSamples, float lowThresh, float highThresh)
    {
        if (bufferSize == 0) return false;
        
        // We iterate through the input block again to check levels
        // Note: 'write' has already advanced writePos. We need to look at what we just wrote,
        // or just process the input buffer directly.
        // It's safer to process input directly and map to ring buffer index if needed.
        
        // But we need to know the *exact index in the ring buffer* where trigger happened.
        // So we can re-calculate the ring buffer index for the current sample.
        
        int currentWritePos = writePos.load();
        // The start of this block in the ring buffer was (currentWritePos - numSamples + bufferSize) % bufferSize
        
        for (int i = 0; i < numSamples; ++i)
        {
            float absSample = std::abs(input[i]);
            
            // 1. Low Threshold Check (Potential Start)
            if (absSample > lowThresh)
            {
                if (!inPreRoll)
                {
                    // Found a new potential start after silence
                    inPreRoll = true;
                    // Calculate index in ring buffer for this sample
                    // We need to trace back from current writePos
                    // sample i corresponds to writePos - (numSamples - i)
                    int distFromEnd = numSamples - i;
                    int index = (currentWritePos - distFromEnd + bufferSize) % bufferSize;
                    potentialStartIndex = index;
                    silenceCounter = 0;
                }
            }
            else
            {
                if (inPreRoll)
                {
                    silenceCounter++;
                    // If silence persists for e.g. 500ms, reset
                    if (silenceCounter > (int)(sampleRate * 0.5)) 
                    {
                        inPreRoll = false;
                        potentialStartIndex = -1;
                        silenceCounter = 0;
                    }
                }
            }
            
            // 2. High Threshold Check (Confirm Trigger)
            if (absSample > highThresh && inPreRoll)
            {
                // Trigger confirmed!
                // We return true immediately. The potentialStartIndex is correctly set.
                return true;
            }
        }
        
        return false;
    }
    
    // Get audio data from potentialStartIndex (Low Trigger) up to current WritePos
    // This forms the "attack" part that was buffered.
    void getLookbackData(juce::AudioBuffer<float>& dest)
    {
        if (potentialStartIndex < 0 || bufferSize == 0) return;
        
        int currentWritePos = writePos.load();
        
        // まず総サンプル数を計算（元のロジック）
        int totalAvailable = 0;
        if (currentWritePos >= potentialStartIndex)
            totalAvailable = currentWritePos - potentialStartIndex;
        else
            totalAvailable = (bufferSize - potentialStartIndex) + currentWritePos;
        
        // 現在のブロックを除外（二重記録防止）
        // recordIntoTracks() で同じブロックが再度記録されるため
        int availableSamples = totalAvailable - lastWrittenBlockSize;
            
        // Safety cap
        if (availableSamples > bufferSize) availableSamples = bufferSize;
        if (availableSamples <= 0) return; // 現在ブロックを除外すると何も残らない場合
        
        dest.setSize(1, availableSamples);
        
        // Copy 1: potentialStart -> end of buffer (or writePos if no wrap)
        int samplesFirstPart = juce::jmin(availableSamples, bufferSize - potentialStartIndex);
        
        // Using float* for raw access
        const float* src = ringBuffer.data();
        float* dst = dest.getWritePointer(0);
        
        for (int i = 0; i < samplesFirstPart; ++i)
            dst[i] = src[potentialStartIndex + i];
            
        // Copy 2: If wrapped, start of buffer -> writePos
        if (samplesFirstPart < availableSamples)
        {
            int samplesSecondPart = availableSamples - samplesFirstPart;
            for (int i = 0; i < samplesSecondPart; ++i)
                dst[samplesFirstPart + i] = src[i];
        }
        
        // Reset state after retrieval
        // We do NOT reset inPreRoll here, to prevent re-triggering during the same phrase.
        // We only invalidate the start index since we consumed the buffer.
        potentialStartIndex = -1; 
        
        // inPreRoll remains true until silence timeout resets it in processTriggers.
        // silenceCounter remains valid.
    }
    
    bool isInPreRoll() const { return inPreRoll; }
    
    // Helper to clear buffer
    void clear()
    {
        std::fill(ringBuffer.begin(), ringBuffer.end(), 0.0f);
        potentialStartIndex = -1;
        inPreRoll = false;
    }

private:
    std::vector<float> ringBuffer;
    std::atomic<int> writePos {0};
    int bufferSize = 0;
    double sampleRate = 48000.0;
    
    int potentialStartIndex = -1; // The ring buffer index where Low Threshold was crossed
    bool inPreRoll = false;       // Are we tracking a potential sound?
    int silenceCounter = 0;       // Buffer counters for silence logic
    int lastWrittenBlockSize = 0; // 最後に書き込まれたブロックサイズ（二重記録防止用）
};
