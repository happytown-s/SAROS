#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "InputManager.h"
#include "ThemeColours.h"

// =====================================================
// チャンネルペアカード（2ch を1ペアとして管理）
// =====================================================
class ChannelPairCard : public juce::Component
{
public:
    ChannelPairCard(int pairIndex, InputManager& im) 
        : leftIndex(pairIndex * 2), rightIndex(pairIndex * 2 + 1), inputManager(im)
    {
        int numCh = im.getNumChannels();
        auto& leftSettings = im.getChannelManager().getSettings(leftIndex);
        
        // Left Active Toggle
        leftActiveBtn.setButtonText("ON");
        leftActiveBtn.setClickingTogglesState(true);
        leftActiveBtn.setToggleState(leftSettings.isActive, juce::dontSendNotification);
        leftActiveBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey.darker());
        leftActiveBtn.setColour(juce::TextButton::buttonOnColourId, ThemeColours::NeonCyan);
        leftActiveBtn.onClick = [this, &leftSettings]() {
            leftSettings.isActive = leftActiveBtn.getToggleState();
        };
        addAndMakeVisible(leftActiveBtn);
        
        // Right Active Toggle (if exists)
        if (rightIndex < numCh)
        {
            auto& rightSettings = im.getChannelManager().getSettings(rightIndex);
            rightActiveBtn.setButtonText("ON");
            rightActiveBtn.setClickingTogglesState(true);
            rightActiveBtn.setToggleState(rightSettings.isActive, juce::dontSendNotification);
            rightActiveBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey.darker());
            rightActiveBtn.setColour(juce::TextButton::buttonOnColourId, ThemeColours::NeonCyan);
            rightActiveBtn.onClick = [this, &rightSettings]() {
                rightSettings.isActive = rightActiveBtn.getToggleState();
            };
            addAndMakeVisible(rightActiveBtn);
            hasRightChannel = true;
        }
        
        // Link Toggle (spans both channels)
        linkBtn.setButtonText("LINK");
        linkBtn.setClickingTogglesState(true);
        linkBtn.setToggleState(leftSettings.isStereoLinked, juce::dontSendNotification);
        linkBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey.darker());
        linkBtn.setColour(juce::TextButton::buttonOnColourId, ThemeColours::NeonMagenta);
        linkBtn.onClick = [this, &leftSettings]() {
            bool linked = linkBtn.getToggleState();
            leftSettings.isStereoLinked = linked;
            if (rightIndex < inputManager.getNumChannels())
                inputManager.getChannelManager().getSettings(rightIndex).isStereoLinked = linked;
        };
        addAndMakeVisible(linkBtn);
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().reduced(2);
        
        // カード背景
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRoundedRectangle(bounds.toFloat(), 6.0f);
        
        int halfWidth = getWidth() / 2;
        int meterTop = 18;
        int meterBottom = getHeight() - 52;
        int meterHeight = meterBottom - meterTop;
        
        // 左チャンネル
        drawChannel(g, leftIndex, 4, meterTop, halfWidth - 6, meterHeight);
        
        // 右チャンネル（存在する場合）
        if (hasRightChannel)
            drawChannel(g, rightIndex, halfWidth + 2, meterTop, halfWidth - 6, meterHeight);
    }
    
    void drawChannel(juce::Graphics& g, int chIndex, int x, int y, int w, int h)
    {
        // チャンネル番号
        g.setColour(ThemeColours::Silver);
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText(juce::String(chIndex + 1), x, 2, w, 14, juce::Justification::centred);
        
        // メーター背景
        juce::Rectangle<int> meterArea(x + 4, y, w - 8, h);
        g.setColour(juce::Colours::black);
        g.fillRect(meterArea);
        
        // メーターレベル
        float level = inputManager.getChannelLevel(chIndex);
        int levelHeight = (int)(meterArea.getHeight() * juce::jlimit(0.0f, 1.0f, level));
        
        if (levelHeight > 0)
        {
            auto& settings = inputManager.getChannelManager().getSettings(chIndex);
            bool isTriggering = level > settings.getEffectiveThreshold();
            
            g.setColour(isTriggering ? ThemeColours::RecordingRed : ThemeColours::NeonCyan);
            g.fillRect(meterArea.getX(), meterArea.getBottom() - levelHeight, 
                       meterArea.getWidth(), levelHeight);
        }
        
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        g.drawRect(meterArea, 1);
    }
    
    void resized() override
    {
        int halfWidth = getWidth() / 2;
        int btnHeight = 22;
        int btnY = getHeight() - 50;
        int linkY = getHeight() - 26;
        
        // Active buttons (1ch width each)
        leftActiveBtn.setBounds(4, btnY, halfWidth - 6, btnHeight);
        if (hasRightChannel)
            rightActiveBtn.setBounds(halfWidth + 2, btnY, halfWidth - 6, btnHeight);
        
        // Link button (2ch width)
        linkBtn.setBounds(4, linkY, getWidth() - 8, btnHeight);
    }

private:
    int leftIndex;
    int rightIndex;
    bool hasRightChannel = false;
    InputManager& inputManager;
    juce::TextButton leftActiveBtn;
    juce::TextButton rightActiveBtn;
    juce::TextButton linkBtn;
};

// =====================================================
// チャンネルペアグリッドコンテナ
// =====================================================
class ChannelPairGridContainer : public juce::Component
{
public:
    ChannelPairGridContainer(InputManager& im) : inputManager(im) {}
    
    void refresh()
    {
        cards.clear();
        int numCh = inputManager.getNumChannels();
        int numPairs = (numCh + 1) / 2;
        
        for (int i = 0; i < numPairs; ++i)
        {
            auto card = std::make_unique<ChannelPairCard>(i, inputManager);
            addAndMakeVisible(*card);
            cards.add(std::move(card));
        }
        resized();
    }
    
    void resized() override
    {
        if (cards.isEmpty()) return;
        
        int cardWidth = 80;   // 2ch分の幅
        int cardHeight = 130; // メーターを縦長に
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
    juce::OwnedArray<ChannelPairCard> cards;
};

// =====================================================
// SettingsComponent
// =====================================
class SettingsComponent : public juce::Component, public juce::Timer
{
public:
    SettingsComponent(juce::AudioDeviceManager& dm, InputManager& im)
        : inputManager(im), deviceManager(dm), channelPairGrid(im)
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
        
        viewport.setViewedComponent(&channelPairGrid);
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
        
        // ChannelPairGridの幅をViewportの可視領域に合わせる
        channelPairGrid.setSize(viewport.getMaximumVisibleWidth(), channelPairGrid.getHeight());
        channelPairGrid.resized();
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
                channelPairGrid.refresh();
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
    ChannelPairGridContainer channelPairGrid;
    
    juce::LookAndFeel_V4 darkLAF;
    InputManager& inputManager;
    juce::AudioDeviceManager& deviceManager;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsComponent)
};
