#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "ThemeColours.h"

class CircularVisualizer : public juce::Component, public juce::Timer
{
public:
    CircularVisualizer()
        : forwardFFT(fftOrder),
          window(fftSize, juce::dsp::WindowingFunction<float>::hann)
    {
        startTimerHz(60);
    }

    void pushBuffer(const juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() > 0)
        {
            auto* channelData = buffer.getReadPointer(0);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                pushSampleIntoFifo(channelData[i]);
        }
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto centre = bounds.getCentre();
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.35f;

        // Draw background circle (subtle)
        g.setColour(ThemeColours::MetalGray.withAlpha(0.1f));
        g.fillEllipse(bounds.withSizeKeepingCentre(radius * 2.0f, radius * 2.0f));
        g.setColour(ThemeColours::MetalGray.withAlpha(0.3f));
        g.drawEllipse(bounds.withSizeKeepingCentre(radius * 2.1f, radius * 2.1f), 1.0f);

        // Draw spinning accent rings
        float rotation = (float)juce::Time::getMillisecondCounterHiRes() * 0.001f;
        g.setColour(ThemeColours::NeonCyan.withAlpha(0.15f));
        drawRotatingRing(g, centre, radius * 1.05f, rotation, 0.4f);
        g.setColour(ThemeColours::NeonMagenta.withAlpha(0.1f));
        drawRotatingRing(g, centre, radius * 1.1f, -rotation * 0.7f, 0.3f);

        // Draw FFT bars
        const int numBars = fftSize / 4; // Use first half of spectrum and decimate
        const float angleStep = juce::MathConstants<float>::twoPi / numBars;

        for (int i = 0; i < numBars; ++i)
        {
            float level = juce::jlimit(0.0f, 1.0f, scopeData[i]);
            float barLength = level * radius * 0.8f;
            float angle = i * angleStep - juce::MathConstants<float>::halfPi;

            auto start = centre.getPointOnCircumference(radius, angle);
            auto end = centre.getPointOnCircumference(radius + barLength, angle);

            juce::Path p;
            p.startNewSubPath(start);
            p.lineTo(end);

            float clampedLevel = juce::jlimit(0.0f, 1.0f, level);
            g.setColour(ThemeColours::NeonCyan.overlaidWith(ThemeColours::NeonMagenta.withAlpha(clampedLevel)));
            g.strokePath(p, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            
            // Add a glow effect for high levels
            if (level > 0.5f)
            {
                g.setColour(ThemeColours::NeonCyan.withAlpha(juce::jlimit(0.0f, 1.0f, 0.3f * level)));
                g.strokePath(p, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }
        
        // Outer ring
        g.setColour(ThemeColours::NeonCyan.withAlpha(0.4f));
        g.drawEllipse(bounds.withSizeKeepingCentre(radius * 2.0f, radius * 2.0f), 1.5f);
    }

    void timerCallback() override
    {
        repaint(); // Always repaint for animations
        
        if (nextFFTBlockReady)
        {
            drawNextFrameOfSpectrum();
            nextFFTBlockReady = false;
        }
    }

private:
    void drawRotatingRing(juce::Graphics& g, juce::Point<float> centre, float radius, float rotation, float arcLength)
    {
        juce::Path ring;
        ring.addCentredArc(centre.x, centre.y, radius, radius, rotation, 0.0f, juce::MathConstants<float>::twoPi * arcLength, true);
        g.strokePath(ring, juce::PathStrokeType(1.5f));
    }
    void pushSampleIntoFifo(float sample) noexcept
    {
        if (fifoIndex == fftSize)
        {
            if (!nextFFTBlockReady)
            {
                juce::zeromem(fftData, sizeof(fftData));
                std::memcpy(fftData, fifo, sizeof(fifo));
                nextFFTBlockReady = true;
            }
            fifoIndex = 0;
        }
        fifo[fifoIndex++] = sample;
    }

    void drawNextFrameOfSpectrum()
    {
        window.multiplyWithWindowingTable(fftData, fftSize);
        forwardFFT.performFrequencyOnlyForwardTransform(fftData);

        auto mindB = -100.0f;
        auto maxdB = 0.0f;
        const float decayRate = 0.85f; // Slow decay (higher = slower)

        for (int i = 0; i < scopeSize; ++i)
        {
            auto skewedProportionX = 1.0f - std::exp(std::log(1.0f - (float)i / (float)scopeSize) * 0.2f);
            auto fftDataIndex = juce::jlimit(0, fftSize / 2, (int)(skewedProportionX * (float)fftSize / 2));
            auto newLevel = juce::jmap(juce::Decibels::gainToDecibels(fftData[fftDataIndex]) - juce::Decibels::gainToDecibels((float)fftSize), mindB, maxdB, 0.0f, 1.0f);
            newLevel = juce::jlimit(0.0f, 1.0f, newLevel);

            // Apply decay: only decrease slowly, increase immediately
            if (newLevel > scopeData[i])
                scopeData[i] = newLevel;
            else
                scopeData[i] = scopeData[i] * decayRate + newLevel * (1.0f - decayRate);
        }
    }

    static constexpr int fftOrder = 10;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int scopeSize = 256;

    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;

    float fifo[fftSize];
    float fftData[fftSize * 2];
    int fifoIndex = 0;
    bool nextFFTBlockReady = false;
    float scopeData[scopeSize];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CircularVisualizer)
};
