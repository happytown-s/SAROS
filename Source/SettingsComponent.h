#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "InputManager.h"
#include "ThemeColours.h"

// =====================================================
// 個別チャンネル設定カードコンポーネント（縦型コンパクト）
// =====================================================
class ChannelSettingCard : public juce::Component
{
public:
    ChannelSettingCard(int chIndex, InputManager& im) 
        : index(chIndex), inputManager(im)
    {
        auto& settings = im.getChannelManager().getSettings(chIndex);
        
        // Active Toggle（小さいトグル）
        activeToggle.setClickingTogglesState(true);
        activeToggle.setToggleState(settings.isActive, juce::dontSendNotification);
        activeToggle.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey.darker());
        activeToggle.setColour(juce::TextButton::buttonOnColourId, ThemeColours::NeonCyan);
        activeToggle.onClick = [this, &settings]() {
            settings.isActive = activeToggle.getToggleState();
        };
        addAndMakeVisible(activeToggle);
        
        // Stereo Link Toggle (Even channels only)
        if (chIndex % 2 == 0)
        {
            linkToggle.setButtonText("L");
            linkToggle.setClickingTogglesState(true);
            linkToggle.setToggleState(settings.isStereoLinked, juce::dontSendNotification);
            linkToggle.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey.darker());
            linkToggle.setColour(juce::TextButton::buttonOnColourId, ThemeColours::NeonMagenta);
            linkToggle.onClick = [this, &settings, chIndex]() {
                bool linked = linkToggle.getToggleState();
                settings.isStereoLinked = linked;
                if (chIndex + 1 < inputManager.getNumChannels())
                    inputManager.getChannelManager().getSettings(chIndex + 1).isStereoLinked = linked;
            };
            addAndMakeVisible(linkToggle);
        }
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().reduced(2);
        
        // カード背景
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRoundedRectangle(bounds.toFloat(), 4.0f);
        
        // チャンネル番号
        g.setColour(ThemeColours::Silver);
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText(juce::String(index + 1), bounds.removeFromTop(16), juce::Justification::centred);
        
        // 縦型メーター（カード中央）
        auto meterArea = bounds.reduced(8, 4);
        meterArea = meterArea.removeFromTop(meterArea.getHeight() - 24);
        
        g.setColour(juce::Colours::black);
        g.fillRect(meterArea);
        
        float level = inputManager.getChannelLevel(index);
        int meterHeight = (int)(meterArea.getHeight() * juce::jlimit(0.0f, 1.0f, level));
        
        if (meterHeight > 0)
        {
            auto& settings = inputManager.getChannelManager().getSettings(index);
            bool isTriggering = level > settings.getEffectiveThreshold();
            
            g.setColour(isTriggering ? ThemeColours::RecordingRed : ThemeColours::NeonCyan);
            g.fillRect(meterArea.getX(), meterArea.getBottom() - meterHeight, 
                       meterArea.getWidth(), meterHeight);
        }
        
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        g.drawRect(meterArea, 1);
    }
    
    void resized() override
    {
        auto area = getLocalBounds().reduced(2);
        area.removeFromTop(16); // ch番号のスペース
        area = area.removeFromBottom(24).reduced(2);
        
        int btnWidth = (getWidth() - 8) / 2;
        activeToggle.setBounds(area.removeFromLeft(btnWidth).reduced(1));
        
        if (index % 2 == 0)
            linkToggle.setBounds(area.removeFromLeft(btnWidth).reduced(1));
    }

private:
    int index;
    InputManager& inputManager;
    juce::TextButton activeToggle;
    juce::TextButton linkToggle;
};

// =====================================================
// チャンネルグリッドコンテナ（横並び）
// =====================================================
class ChannelGridContainer : public juce::Component
{
public:
    ChannelGridContainer(InputManager& im) : inputManager(im) {}
    
    void refresh()
    {
        cards.clear();
        int numCh = inputManager.getNumChannels();
        for (int i = 0; i < numCh; ++i)
        {
            auto card = std::make_unique<ChannelSettingCard>(i, inputManager);
            addAndMakeVisible(*card);
            cards.add(std::move(card));
        }
        resized();
    }
    
    void resized() override
    {
        if (cards.isEmpty()) return;
        
        int cardWidth = 55;
        int cardHeight = 90;
        int cols = juce::jmax(1, getWidth() / cardWidth);
        int numRows = (cards.size() + cols - 1) / cols;
        
        for (int i = 0; i < cards.size(); ++i)
        {
            int col = i % cols;
            int row = i / cols;
            cards[i]->setBounds(col * cardWidth, row * cardHeight, cardWidth, cardHeight);
        }
        
        setSize(getWidth(), numRows * cardHeight);
    }

private:
    InputManager& inputManager;
    juce::OwnedArray<ChannelSettingCard> cards;
};

// =====================================================
// SettingsComponent
// =====================================
class SettingsComponent : public juce::Component, public juce::Timer
{
public:
    SettingsComponent(juce::AudioDeviceManager& dm, InputManager& im)
        : inputManager(im), deviceManager(dm), channelGrid(im)
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
        
        viewport.setViewedComponent(&channelGrid);
        viewport.setScrollBarsShown(true, false);
        addAndMakeVisible(viewport);

        updateChannelUI(true); // Force refresh initially
        startTimerHz(30);
        setSize(700, 750);
    }
    
    ~SettingsComponent() override
    {
         audioSelector->setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        // Viewportの外側の背景を埋める
        g.fillAll(juce::Colour(0xff151515));

        // マスターメーターの描画
        if (!masterMeterRect.isEmpty())
        {
            g.setColour(juce::Colours::black);
            g.fillRect(masterMeterRect);
            
            float level = inputManager.getCurrentLevel();
            float threshold = (float)thresholdSlider.getValue();
            float width = masterMeterRect.getWidth() * juce::jlimit(0.0f, 1.0f, level);
            
            if (width > 0)
            {
                bool isTriggering = level > threshold;
                g.setColour(isTriggering ? ThemeColours::RecordingRed : ThemeColours::NeonCyan);
                g.fillRect(masterMeterRect.withWidth(width));
            }
            
            // 閾値ライン
            float threshX = masterMeterRect.getX() + masterMeterRect.getWidth() * threshold;
            g.setColour(juce::Colours::white);
            g.drawLine(threshX, masterMeterRect.getY() - 2, threshX, masterMeterRect.getBottom() + 2, 2.0f);
            
            g.setColour(juce::Colours::white.withAlpha(0.2f));
            g.drawRect(masterMeterRect, 1.0f);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(20);
        
        // Device Selector (Top)
        if (audioSelector)
            audioSelector->setBounds(area.removeFromTop(250));
            
        area.removeFromTop(10);
        
        // Global Controls (Middle)
        auto globalArea = area.removeFromTop(150);
        globalControlsHeader.setBounds(globalArea.removeFromTop(30));
        
        auto row1 = globalArea.removeFromTop(40);
        useCalibrationButton.setBounds(row1.removeFromLeft(180).reduced(5));
        calibrateButton.setBounds(row1.removeFromLeft(180).reduced(5));
        
        auto row2 = globalArea.removeFromTop(40);
        row2.removeFromLeft(100); // offset for label
        thresholdSlider.setBounds(row2.reduced(5));
        
        // Row 3: Master Meter
        masterMeterRect = globalArea.removeFromTop(30).reduced(100, 5).toFloat();
        
        area.removeFromTop(10);
        
        // Channel List (Bottom)
        channelListHeader.setBounds(area.removeFromTop(30));
        viewport.setBounds(area);
        
        // ChannelGridの幅をViewportの可視領域に合わせる
        channelGrid.setSize(viewport.getMaximumVisibleWidth(), channelGrid.getHeight());
        channelGrid.resized();
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
    
    void updateChannelUI(bool forceRefresh = false)
    {
        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            int numInputChannels = device->getActiveInputChannels().countNumberOfSetBits();
            if (forceRefresh || inputManager.getNumChannels() != numInputChannels)
            {
                inputManager.setNumChannels(numInputChannels);
                channelGrid.refresh();
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
    juce::Rectangle<float> masterMeterRect;
    
    juce::Label channelListHeader;
    juce::Viewport viewport;
    ChannelGridContainer channelGrid;
    
    juce::LookAndFeel_V4 darkLAF;
    InputManager& inputManager;
    juce::AudioDeviceManager& deviceManager;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsComponent)
};
