/*
  ==============================================================================
    PitchShifter.h - Delay-line based pitch shifting for Autotune
  ==============================================================================
*/

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>

class PitchShifter
{
public:
    void prepare(double sampleRate, int /*blockSize*/)
    {
        sr = sampleRate;
        // Delay buffer ~50ms for pitch shifting
        bufferSize = static_cast<int>(sr * 0.05);
        delayBuffer.resize(bufferSize, 0.0f);
        writePos = 0;
        readPos1 = 0.0;
        readPos2 = bufferSize / 2.0;
        crossfadePos = 0.0;
        pitchRatio = 1.0f;
    }

    void setPitchRatio(float ratio)
    {
        pitchRatio = juce::jlimit(0.5f, 2.0f, ratio);
    }

    float process(float input)
    {
        // Write to delay buffer
        delayBuffer[writePos] = input;
        writePos = (writePos + 1) % bufferSize;

        // Two read heads with crossfade for seamless looping
        float out1 = readInterpolated(readPos1);
        float out2 = readInterpolated(readPos2);

        // Hann window crossfade
        double fadeLen = bufferSize / 2.0;
        double fade1 = 0.5 * (1.0 - std::cos(juce::MathConstants<double>::pi * crossfadePos / fadeLen));
        double fade2 = 1.0 - fade1;

        float output = static_cast<float>(out1 * fade2 + out2 * fade1);

        // Advance read positions based on pitch ratio
        double speed = static_cast<double>(pitchRatio);
        readPos1 += speed;
        readPos2 += speed;
        crossfadePos += 1.0;

        // Wrap read positions
        if (readPos1 >= bufferSize) readPos1 -= bufferSize;
        if (readPos2 >= bufferSize) readPos2 -= bufferSize;
        if (crossfadePos >= fadeLen)
        {
            crossfadePos = 0.0;
            // Reset lagging head to maintain sync
            if (speed > 1.0)
                readPos1 = readPos2 + fadeLen;
            else
                readPos2 = readPos1 + fadeLen;

            if (readPos1 >= bufferSize) readPos1 -= bufferSize;
            if (readPos2 >= bufferSize) readPos2 -= bufferSize;
        }

        return output;
    }

    void reset()
    {
        std::fill(delayBuffer.begin(), delayBuffer.end(), 0.0f);
        writePos = 0;
        readPos1 = 0.0;
        readPos2 = bufferSize / 2.0;
        crossfadePos = 0.0;
    }

private:
    float readInterpolated(double pos)
    {
        int idx0 = static_cast<int>(pos) % bufferSize;
        int idx1 = (idx0 + 1) % bufferSize;
        double frac = pos - std::floor(pos);
        return static_cast<float>(delayBuffer[idx0] * (1.0 - frac) + delayBuffer[idx1] * frac);
    }

    double sr = 44100.0;
    int bufferSize = 2048;
    std::vector<float> delayBuffer;
    int writePos = 0;
    double readPos1 = 0.0, readPos2 = 0.0;
    double crossfadePos = 0.0;
    float pitchRatio = 1.0f;
};
