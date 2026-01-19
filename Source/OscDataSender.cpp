/*
  ==============================================================================

    OscDataSender.cpp
    Created: 18 Jan 2026
    Author:  Antigravity

  ==============================================================================
*/

#include "OscDataSender.h"

OscDataSender::OscDataSender(LooperAudio& looper)
    : looperAudio(looper)
{
    // Default connection to localhost UE5 (standard OSC port often used is 8000, but we'll use 8000)
    connect("127.0.0.1", 8000);
    
    // Start timer for 60fps updates
    startTimerHz(updateRateHz);
}

OscDataSender::~OscDataSender()
{
    stopTimer();
    disconnect();
}

void OscDataSender::connect(const juce::String& hostName, int portNumber)
{
    if (!oscSender.connect(hostName, portNumber))
    {
        DBG("OSC: Failed to connect to " << hostName << ":" << portNumber);
    }
    else
    {
        DBG("OSC: Connected to " << hostName << ":" << portNumber);
    }
}

void OscDataSender::disconnect()
{
    oscSender.disconnect();
}

void OscDataSender::sendTriggerEvent(const juce::String& triggerName)
{
    // OSCSenderにはisConnected()がないので、connect済みなら直接送信
    try
    {
        oscSender.send("/saros/event", (juce::String)triggerName);
    }
    catch (const juce::OSCException& e)
    {
        DBG("OSC Send Error: " << e.what());
    }
}

void OscDataSender::timerCallback()
{
    try
    {
        juce::OSCBundle bundle;

        // 1. Master Transport
        // Loop position (0.0 - 1.0) - 既に正規化されたメソッドを使用
        float loopPos = looperAudio.getMasterNormalizedPosition();
        bundle.addElement(juce::OSCMessage("/saros/master/pos", loopPos));

        // 2. Track Levels (RMS)
        // Tracks 1-8
        for (int i = 1; i <= 8; ++i)
        {
            // Lock-free access using atomic or safe getters
            float level = looperAudio.getTrackRMS(i);
            
            // Add to bundle (address: /saros/track/{id}/level)
            bundle.addElement(juce::OSCMessage("/saros/track/" + juce::String(i) + "/level", level));
        }
        
        // 3. Trigger Events (if any specific state is polled)
        // For now, events are strictly push-based via sendTriggerEvent
        
        oscSender.send(bundle);
    }
    catch (const juce::OSCException& e)
    {
        // DBG("OSC Bundle Error: " << e.what());
    }
}

void OscDataSender::sendTrackLevels()
{
    // Implementation moved to timerCallback for bundling
}

void OscDataSender::sendTransportInfo()
{
   // Implementation moved to timerCallback for bundling
}
