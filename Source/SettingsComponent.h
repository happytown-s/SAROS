#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "InputManager.h"
#include "ThemeColours.h"

class SettingsComponent : public juce::Component, public juce::Timer
{
public:
    SettingsComponent(juce::AudioDeviceManager& dm, InputManager& im)
        : inputManager(im)
    {
        // Apply Dark Theme
        darkLAF.setColourScheme(juce::LookAndFeel_V4::getMidnightColourScheme());
        
        // 1. Audio Device Selector
        audioSelector.reset(new juce::AudioDeviceSelectorComponent(dm,
                                                                   0, 2,   // min/max input
                                                                   0, 2,   // min/max output
                                                                   false, false,
                                                                   true, true));
        audioSelector->setLookAndFeel(&darkLAF);
        addAndMakeVisible(audioSelector.get());

        // 2. Threshold Slider
        thresholdSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        thresholdSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
        thresholdSlider.setRange(0.001, 1.0, 0.001); 
        thresholdSlider.setSkewFactorFromMidPoint(0.1);
        thresholdSlider.setColour(juce::Slider::trackColourId, juce::Colours::grey);
        thresholdSlider.setColour(juce::Slider::thumbColourId, ThemeColours::NeonCyan);
        
        // Initial config
        float currentThresh = im.getConfig().userThreshold;
        thresholdSlider.setValue(currentThresh, juce::dontSendNotification);
        
        thresholdSlider.onValueChange = [this]()
        {
            auto conf = inputManager.getConfig();
            conf.userThreshold = (float)thresholdSlider.getValue();
            inputManager.setConfig(conf);
        };
        addAndMakeVisible(thresholdSlider);

        // Label
        threshLabel.setText("High Trigger Threshold", juce::dontSendNotification);
        threshLabel.attachToComponent(&thresholdSlider, true);
        threshLabel.setJustificationType(juce::Justification::right);
        threshLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(threshLabel);

        startTimerHz(60); // Animation rate
        setSize(600, 550);
    }
    
    ~SettingsComponent() override
    {
         audioSelector->setLookAndFeel(nullptr); // clear before destruction
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(20);

        if (audioSelector)
            audioSelector->setBounds(area.removeFromTop(320));

        area.removeFromTop(20);

        // Slider
        auto sliderArea = area.removeFromTop(40);
        thresholdSlider.setBounds(sliderArea.reduced(20, 0).removeFromRight(350));
        
        // Meter Area is drawn in paint below the slider
    }
    
    void timerCallback() override
    {
        repaint();
    }
    
    void paint(juce::Graphics& g) override
    {
        // Dark Background for the whole panel
        g.fillAll(juce::Colour(0xff151515)); 

        // Draw Meter below slider
        auto bounds = getLocalBounds();
        juce::Rectangle<float> meterRect(150, 420, 350, 24); // Slightly taller
        
        // 1. Meter Channel Background (Deep indented look)
        g.setColour(juce::Colour(0xff0a0a0a));
        g.fillRect(meterRect);
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.drawRect(meterRect, 1.0f);
        
        // Get Levels
        float currentLevel = inputManager.getCurrentLevel();
        float threshold = (float)thresholdSlider.getValue();
        
        // 2. Calculate Meter Width
        // Use non-linear scaling for better visibility of low signals?
        // Let's stick to linear for threshold accuracy mapping.
        float normalizedLevel = juce::jlimit(0.0f, 1.0f, currentLevel);
        float meterWidth = meterRect.getWidth() * normalizedLevel;
        
        juce::Rectangle<float> barRect = meterRect.withWidth(meterWidth);
        
        // 3. Futuristic Gradient Fill
        if (meterWidth > 0)
        {
            bool isTriggering = currentLevel > threshold;
            
            juce::ColourGradient gradient(ThemeColours::NeonCyan, meterRect.getX(), meterRect.getY(),
                                          isTriggering ? ThemeColours::RecordingRed : ThemeColours::NeonMagenta, 
                                          meterRect.getRight(), meterRect.getY(), false);
            
            // Add intermediate color for richness
            gradient.addColour(0.5, ThemeColours::ElectricBlue);
            
            g.setGradientFill(gradient);
            g.fillRect(barRect);
            
            // Add Glow
            /* // Too expensive for simple paint? Simplified glow via separate rect
             g.setColour(gradient.getColourAtPosition(1.0).withAlpha(0.3f));
             g.fillRect(barRect.expanded(2));
             */
        }

        // 4. Grid Lines (Digital Look)
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        for (int i = 1; i < 20; ++i)
        {
            float x = meterRect.getX() + meterRect.getWidth() * (i / 20.0f);
            g.drawLine(x, meterRect.getY(), x, meterRect.getBottom(), 1.0f);
        }

        // 5. Draw Threshold Line (Bright & Clear)
        float threshX = meterRect.getX() + meterRect.getWidth() * threshold;
        g.setColour(juce::Colours::white);
        g.drawLine(threshX, meterRect.getY() - 4, threshX, meterRect.getBottom() + 4, 3.0f);
        
        // Threshold Label (Tiny stats)
        // g.setFont(10.0f);
        // g.drawText(juce::String(threshold, 3), threshX + 4, meterRect.getY() - 14, 40, 10, juce::Justification::left);
        
        // 6. Peak Hold / Trigger Indicator
        bool isTriggering = currentLevel > threshold;
        if (isTriggering)
        {
            // Flash Effect Frame
            g.setColour(ThemeColours::RecordingRed.withAlpha(0.8f));
            g.drawRect(meterRect.expanded(3), 2.0f);
            
            // Text Warning
            g.setColour(ThemeColours::RecordingRed);
            g.setFont(juce::Font(juce::FontOptions("Arial", 14.0f, juce::Font::bold)));
            g.drawText("TRIGGER DETECTED", meterRect.getRight() + 10, meterRect.getY(), 140, 24, juce::Justification::left);
        }
    }

private:
    std::unique_ptr<juce::AudioDeviceSelectorComponent> audioSelector;
    juce::Slider thresholdSlider;
    juce::Label threshLabel;
    
    juce::LookAndFeel_V4 darkLAF;
    
    InputManager& inputManager;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsComponent)
};
