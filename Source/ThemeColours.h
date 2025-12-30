/*
  ==============================================================================

    ThemeColours.h
    Created: 30 Dec 2025
    Author:  Antigravity

  ==============================================================================
*/

#pragma once
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace ThemeColours
{
    // High-tech / Cyberpunk Palette
    const juce::Colour Background      = juce::Colour::fromRGB(11, 11, 12);   // Deep Black
    const juce::Colour NeonCyan        = juce::Colour::fromRGB(0, 242, 255);  // Accent 1
    const juce::Colour NeonMagenta     = juce::Colour::fromRGB(255, 0, 204);  // Accent 2
    const juce::Colour ElectricBlue    = juce::Colour::fromRGB(77, 77, 255);  // Highlight
    const juce::Colour Silver          = juce::Colour::fromRGB(224, 224, 224); // Text
    
    const juce::Colour RecordingRed    = juce::Colour::fromRGB(255, 46, 46);  // Recording state
    const juce::Colour PlayingGreen    = juce::Colour::fromRGB(57, 255, 20);  // Playing state
    const juce::Colour StandbyBlue     = juce::Colour::fromRGB(0, 102, 204);  // Standby state
    const juce::Colour MetalGray       = juce::Colour::fromRGB(45, 45, 50);   // UI Elements
}

inline void setupFuturisticButton(juce::TextButton& btn, juce::Colour accentColour)
{
    btn.setColour(juce::TextButton::buttonColourId, ThemeColours::MetalGray);
    btn.setColour(juce::TextButton::buttonOnColourId, accentColour);
    btn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    btn.setColour(juce::TextButton::textColourOffId, ThemeColours::Silver);

    btn.setButtonText(btn.getButtonText().toUpperCase());
    btn.setClickingTogglesState(false);
    btn.setConnectedEdges(0);
    btn.setWantsKeyboardFocus(false);
    btn.setMouseCursor(juce::MouseCursor::PointingHandCursor);
}
