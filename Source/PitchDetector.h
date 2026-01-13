/*
  ==============================================================================
    PitchDetector.h - YIN-based pitch detection for Autotune
  ==============================================================================
*/

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>

class PitchDetector
{
public:
    void prepare(double sampleRate, int /*blockSize*/)
    {
        sr = sampleRate;
        yinWindowSize = static_cast<int>(sr * 0.025); // 25ms
        yinWindowSize = std::max(256, std::min(yinWindowSize, 2048));

        inputBuffer.resize(yinWindowSize * 2, 0.0f);
        yinBuffer.resize(yinWindowSize, 0.0f);
        writePos = 0;
        smoothedPitch = 0.0f;
    }

    void pushSamples(const float* samples, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            inputBuffer[writePos] = samples[i];
            writePos = (writePos + 1) % static_cast<int>(inputBuffer.size());
        }
    }

    float detectPitch()
    {
        if (sr <= 0) return 0.0f;

        // Extract window from ring buffer
        std::vector<float> window(yinWindowSize);
        int readPos = (writePos - yinWindowSize + (int)inputBuffer.size()) % (int)inputBuffer.size();
        for (int i = 0; i < yinWindowSize; ++i)
            window[i] = inputBuffer[(readPos + i) % (int)inputBuffer.size()];

        // RMS check
        float rms = 0.0f;
        for (int i = 0; i < yinWindowSize; ++i)
            rms += window[i] * window[i];
        rms = std::sqrt(rms / yinWindowSize);
        if (rms < 0.01f) { smoothedPitch *= 0.9f; return smoothedPitch; }

        // YIN difference function
        int halfWin = yinWindowSize / 2;
        std::fill(yinBuffer.begin(), yinBuffer.end(), 0.0f);
        for (int tau = 1; tau < halfWin; ++tau)
        {
            float sum = 0.0f;
            for (int j = 0; j < halfWin; ++j)
            {
                float d = window[j] - window[j + tau];
                sum += d * d;
            }
            yinBuffer[tau] = sum;
        }

        // Cumulative mean normalized
        yinBuffer[0] = 1.0f;
        float runSum = 0.0f;
        for (int tau = 1; tau < halfWin; ++tau)
        {
            runSum += yinBuffer[tau];
            yinBuffer[tau] = (runSum > 0) ? (yinBuffer[tau] * tau / runSum) : 1.0f;
        }

        // Find threshold crossing
        int tauEst = -1;
        for (int tau = 2; tau < halfWin; ++tau)
        {
            if (yinBuffer[tau] < 0.15f)
            {
                while (tau + 1 < halfWin && yinBuffer[tau + 1] < yinBuffer[tau]) ++tau;
                tauEst = tau;
                break;
            }
        }
        if (tauEst < 1) { smoothedPitch *= 0.95f; return smoothedPitch; }

        // Parabolic interpolation
        float betterTau = static_cast<float>(tauEst);
        if (tauEst > 0 && tauEst < halfWin - 1)
        {
            float s0 = yinBuffer[tauEst - 1], s1 = yinBuffer[tauEst], s2 = yinBuffer[tauEst + 1];
            float denom = 2.0f * (2.0f * s1 - s2 - s0);
            if (std::abs(denom) > 1e-9f) betterTau = tauEst + (s2 - s0) / denom;
        }

        float pitch = (betterTau > 0) ? static_cast<float>(sr) / betterTau : 0.0f;
        if (pitch < 50.0f || pitch > 2000.0f) { smoothedPitch *= 0.95f; return smoothedPitch; }

        smoothedPitch = smoothedPitch * 0.7f + pitch * 0.3f;
        return smoothedPitch;
    }

private:
    double sr = 44100.0;
    int yinWindowSize = 1024;
    std::vector<float> inputBuffer, yinBuffer;
    int writePos = 0;
    float smoothedPitch = 0.0f;
};
