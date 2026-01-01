#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "InputManager.h"
#include "ThemeColours.h"

// =====================================================
// 個別チャンネル設定行コンポーネント
// =====================================================
class ChannelSettingRow : public juce::Component
{
public:
    ChannelSettingRow(int chIndex, InputManager& im) 
        : index(chIndex), inputManager(im)
    {
        auto& settings = im.getChannelManager().getSettings(chIndex);
        
        // Ch Label
        label.setText("CH " + juce::String(chIndex + 1), juce::dontSendNotification);
        label.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        label.setColour(juce::Label::textColourId, ThemeColours::Silver);
        addAndMakeVisible(label);
        
        // Active Toggle
        activeToggle.setButtonText("ACTIVE");
        activeToggle.setClickingTogglesState(true);
        activeToggle.setToggleState(settings.isActive, juce::dontSendNotification);
        activeToggle.setColour(juce::TextButton::buttonOnColourId, ThemeColours::NeonCyan);
        activeToggle.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
        activeToggle.onClick = [this, &settings]() {
            settings.isActive = activeToggle.getToggleState();
        };
        addAndMakeVisible(activeToggle);
        
        // Stereo Link Toggle (Even channels only)
        if (chIndex % 2 == 0)
        {
            linkToggle.setButtonText("LINK");
            linkToggle.setClickingTogglesState(true);
            linkToggle.setToggleState(settings.isStereoLinked, juce::dontSendNotification);
            linkToggle.setColour(juce::TextButton::buttonOnColourId, ThemeColours::NeonMagenta);
            linkToggle.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
            linkToggle.onClick = [this, &settings, chIndex]() {
                bool linked = linkToggle.getToggleState();
                settings.isStereoLinked = linked;
                // ペアのもう片方にも反映（ロジック上の都合）
                if (chIndex + 1 < inputManager.getNumChannels())
                    inputManager.getChannelManager().getSettings(chIndex + 1).isStereoLinked = linked;
            };
            addAndMakeVisible(linkToggle);
        }
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().reduced(2);
        
        // チャンネルごとの背景
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRoundedRectangle(bounds.toFloat(), 4.0f);
        
        // メーター
        auto meterArea = getLocalBounds().removeFromRight(150).reduced(10, 12);
        g.setColour(juce::Colours::black);
        g.fillRect(meterArea);
        
        float level = inputManager.getChannelLevel(index);
        float width = meterArea.getWidth() * juce::jlimit(0.0f, 1.0f, level);
        
        if (width > 0)
        {
            auto& settings = inputManager.getChannelManager().getSettings(index);
            bool isTriggering = level > settings.getEffectiveThreshold();
            
            g.setColour(isTriggering ? ThemeColours::RecordingRed : ThemeColours::NeonCyan);
            g.fillRect(meterArea.withWidth(width).toFloat());
        }
        
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.drawRect(meterArea, 1.0f);
    }
    
    void resized() override
    {
        auto area = getLocalBounds().reduced(5);
        label.setBounds(area.removeFromLeft(50));
        activeToggle.setBounds(area.removeFromLeft(70).reduced(2));
        
        if (index % 2 == 0)
            linkToggle.setBounds(area.removeFromLeft(60).reduced(2));
    }

private:
    int index;
    InputManager& inputManager;
    juce::Label label;
    juce::TextButton activeToggle;
    juce::TextButton linkToggle;
};

// =====================================================
// チャンネルリストコンテナ
// =====================================================
class ChannelListContainer : public juce::Component
{
public:
    ChannelListContainer(InputManager& im) : inputManager(im) {}
    
    void refresh()
    {
        rows.clear();
        int numCh = inputManager.getNumChannels();
        for (int i = 0; i < numCh; ++i)
        {
            auto row = std::make_unique<ChannelSettingRow>(i, inputManager);
            addAndMakeVisible(*row);
            rows.add(std::move(row));
        }
        resized();
    }
    
    void resized() override
    {
        int rowHeight = 40;
        for (int i = 0; i < rows.size(); ++i)
            rows[i]->setBounds(0, i * rowHeight, getWidth(), rowHeight);
        
        setSize(getWidth(), rows.size() * rowHeight);
    }

private:
    InputManager& inputManager;
    juce::OwnedArray<ChannelSettingRow> rows;
};

// =====================================================
// SettingsComponent
// =====================================
class SettingsComponent : public juce::Component, public juce::Timer
{
public:
    SettingsComponent(juce::AudioDeviceManager& dm, InputManager& im)
        : inputManager(im), deviceManager(dm), channelList(im)
    {
        // Apply Dark Theme
        darkLAF.setColourScheme(juce::LookAndFeel_V4::getMidnightColourScheme());
        
        // 1. Audio Device Selector
        audioSelector.reset(new juce::AudioDeviceSelectorComponent(dm,
                                                                   0, MAX_CHANNELS,
                                                                   0, 2,
                                                                   false, false,
                                                                   true, true));
        audioSelector->setLookAndFeel(&darkLAF);
        addAndMakeVisible(audioSelector.get());

        // 2. Global Controls Header
        globalControlsHeader.setText("Global Trigger Settings", juce::dontSendNotification);
        globalControlsHeader.setFont(juce::FontOptions(16.0f, juce::Font::bold));
        addAndMakeVisible(globalControlsHeader);
        
        // 3. Calibration Controls
        useCalibrationButton.setButtonText("Use Auto Calibration");
        useCalibrationButton.setClickingTogglesState(true);
        useCalibrationButton.setToggleState(im.isCalibrationEnabled(), juce::dontSendNotification);
        useCalibrationButton.setColour(juce::TextButton::buttonOnColourId, ThemeColours::PlayingGreen);
        useCalibrationButton.onClick = [this]() {
            inputManager.setCalibrationEnabled(useCalibrationButton.getToggleState());
        };
        addAndMakeVisible(useCalibrationButton);
        
        calibrateButton.setButtonText("Run Calibration (2s)");
        calibrateButton.onClick = [this]() {
            if (!inputManager.isCalibrating()) inputManager.startCalibration();
        };
        addAndMakeVisible(calibrateButton);
        
        // 4. Threshold Slider
        thresholdSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        thresholdSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
        thresholdSlider.setRange(0.001, 1.0, 0.001); 
        thresholdSlider.setSkewFactorFromMidPoint(0.1);
        thresholdSlider.setColour(juce::Slider::thumbColourId, ThemeColours::NeonCyan);
        thresholdSlider.setValue(inputManager.getConfig().userThreshold, juce::dontSendNotification);
        thresholdSlider.onValueChange = [this]() {
            auto conf = inputManager.getConfig();
            conf.userThreshold = (float)thresholdSlider.getValue();
            inputManager.setConfig(conf);
            inputManager.getChannelManager().setGlobalThreshold((float)thresholdSlider.getValue());
        };
        addAndMakeVisible(thresholdSlider);
        
        threshLabel.setText("Sensitivity", juce::dontSendNotification);
        threshLabel.attachToComponent(&thresholdSlider, true);
        addAndMakeVisible(threshLabel);

        // 5. Channel Detail List
        channelListHeader.setText("Input Channel Details", juce::dontSendNotification);
        channelListHeader.setFont(juce::FontOptions(16.0f, juce::Font::bold));
        addAndMakeVisible(channelListHeader);
        
        viewport.setViewedComponent(&channelList);
        viewport.setScrollBarsShown(true, false);
        addAndMakeVisible(viewport);

        updateChannelUI();
        startTimerHz(30);
        setSize(700, 750);
    }
    
    ~SettingsComponent() override
    {
         audioSelector->setLookAndFeel(nullptr);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(20);
        
        // Device Selector (Top)
        if (audioSelector)
            audioSelector->setBounds(area.removeFromTop(250));
            
        area.removeFromTop(10);
        
        // Global Controls (Middle)
        auto globalArea = area.removeFromTop(120);
        globalControlsHeader.setBounds(globalArea.removeFromTop(30));
        
        auto row1 = globalArea.removeFromTop(40);
        useCalibrationButton.setBounds(row1.removeFromLeft(180).reduced(5));
        calibrateButton.setBounds(row1.removeFromLeft(180).reduced(5));
        
        auto row2 = globalArea.removeFromTop(40);
        row2.removeFromLeft(100); // offset for label
        thresholdSlider.setBounds(row2.reduced(5));
        
        area.removeFromTop(10);
        
        // Channel List (Bottom)
        channelListHeader.setBounds(area.removeFromTop(30));
        viewport.setBounds(area);
        
        channelList.resized();
    }
    
    void timerCallback() override
    {
        if (inputManager.isCalibrating())
            calibrateButton.setButtonText("Calibrating...");
        else
            calibrateButton.setButtonText("Run Calibration (2s)");
            
        updateChannelUI();
        repaint();
    }
    
    void updateChannelUI()
    {
        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            int numInputChannels = device->getActiveInputChannels().countNumberOfSetBits();
            if (inputManager.getNumChannels() != numInputChannels)
            {
                inputManager.setNumChannels(numInputChannels);
                channelList.refresh();
            }
        }
    }

private:
    std::unique_ptr<juce::AudioDeviceSelectorComponent> audioSelector;
    juce::Label globalControlsHeader;
    juce::TextButton useCalibrationButton;
    juce::TextButton calibrateButton;
    juce::Slider thresholdSlider;
    juce::Label threshLabel;
    
    juce::Label channelListHeader;
    juce::Viewport viewport;
    ChannelListContainer channelList;
    
    juce::LookAndFeel_V4 darkLAF;
    InputManager& inputManager;
    juce::AudioDeviceManager& deviceManager;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsComponent)
};
