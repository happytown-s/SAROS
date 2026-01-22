/*
  ==============================================================================

    OscDataSender.h
    Created: 18 Jan 2026
    Author:  Antigravity

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_osc/juce_osc.h>
#include "LooperAudio.h"

//==============================================================================
/**
    Sends LooperAudio state and analysis data via OSC to external visualizers (e.g. Unreal Engine 5).
*/
#if ! defined (__EMSCRIPTEN__)
// 🏗 Normal Implementation
class OscDataSender : public juce::Timer
{
public:
    OscDataSender(LooperAudio& looperAudioToMonitor);
    ~OscDataSender() override;

    // Connect to a specific host and port (default UE5 listen port is often 8000-9000)
    void connect(const juce::String& hostName, int portNumber);
    void disconnect();

    // Sends specific event messages immediately
    void sendTriggerEvent(const juce::String& triggerName);

private:
    // Timer callback for periodic updates (level, FFT, playhead)
    void timerCallback() override;

    // Helper to bundle track levels
    void sendTrackLevels();
    
    // Helper to send master transport info (playhead, bpm)
    void sendTransportInfo();

    LooperAudio& looperAudio;
    juce::OSCSender oscSender;
    
    // Smoothers or cached values could go here if needed to reduce jitter
    
    // Update rate in Hz
    const int updateRateHz = 60; 

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscDataSender)
};
#else
// 🌐 Dummy Implementation for Web
class OscDataSender
{
public:
    OscDataSender(LooperAudio&) {}
    ~OscDataSender() {}
    void connect(const juce::String&, int) {}
    void disconnect() {}
    void sendTriggerEvent(const juce::String&) {}
};
#endif
