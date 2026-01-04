#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "InputManager.h"
#include "ThemeColours.h"
#include "MidiTabContent.h"
#include "KeyboardTabContent.h"

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
    
    void setNames(const juce::String& lName, const juce::String& rName)
    {
        leftName = lName;
        rightName = rName;
        repaint();
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().reduced(2);
        
        // カード背景
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRoundedRectangle(bounds.toFloat(), 6.0f);
        
        int halfWidth = getWidth() / 2;
        int meterTop = 32; // メーター位置を下げる
        int meterBottom = getHeight() - 52;
        int meterHeight = meterBottom - meterTop;
        
        // 左チャンネル
        drawChannel(g, leftIndex, leftName, 4, meterTop, halfWidth - 6, meterHeight);
        
        // 右チャンネル（存在する場合）
        if (hasRightChannel)
            drawChannel(g, rightIndex, rightName, halfWidth + 2, meterTop, halfWidth - 6, meterHeight);
    }
    
    void drawChannel(juce::Graphics& g, int chIndex, const juce::String& name, int x, int y, int w, int h)
    {
        // デバイス名
        g.setColour(juce::Colours::grey);
        g.setFont(10.0f);
        g.drawText(name, x, 4, w, 12, juce::Justification::centred);

        // チャンネル番号
        g.setColour(ThemeColours::Silver);
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText(juce::String(chIndex + 1), x, 16, w, 14, juce::Justification::centred);
        
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
    juce::String leftName, rightName;
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
    
    void refresh(const juce::StringArray& channelNames)
    {
        cards.clear();
        int numCh = inputManager.getNumChannels();
        int numPairs = (numCh + 1) / 2;
        
        for (int i = 0; i < numPairs; ++i)
        {
            auto card = std::make_unique<ChannelPairCard>(i, inputManager);
            
            // 名前を設定
            juce::String lName = (i * 2 < channelNames.size()) ? channelNames[i * 2] : "";
            juce::String rName = (i * 2 + 1 < channelNames.size()) ? channelNames[i * 2 + 1] : "";
            card->setNames(lName, rName);
            
            addAndMakeVisible(*card);
            cards.add(std::move(card));
        }
        resized();
    }
    
    void resized() override
    {
        if (cards.isEmpty()) return;
        
        // 1行に4ペア（8チャンネル）を表示し、全幅を使う
        int cols = 4; 
        int cardWidth = getWidth() / cols;
        int cardHeight = 130;
        
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
// デバイス設定タブのコンテンツ
// =====================================================
class DeviceTabContent : public juce::Component
{
public:
    DeviceTabContent(juce::AudioDeviceManager& dm)
    {
        darkLAF.setColourScheme(juce::LookAndFeel_V4::getMidnightColourScheme());
        
        audioSelector.reset(new juce::AudioDeviceSelectorComponent(dm,
                                                                   0, MAX_CHANNELS,
                                                                   0, MAX_CHANNELS,
                                                                   false, false,
                                                                   true, true));
        audioSelector->setLookAndFeel(&darkLAF);
        addAndMakeVisible(audioSelector.get());
    }
    
    ~DeviceTabContent() override
    {
        audioSelector->setLookAndFeel(nullptr);
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff151515));
    }
    
    void resized() override
    {
        audioSelector->setBounds(getLocalBounds().reduced(10));
    }

private:
    std::unique_ptr<juce::AudioDeviceSelectorComponent> audioSelector;
    juce::LookAndFeel_V4 darkLAF;
};

// =====================================================
// トリガー設定タブのコンテンツ
// =====================================================
class TriggerTabContent : public juce::Component, public juce::Timer
{
public:
    TriggerTabContent(juce::AudioDeviceManager& dm, InputManager& im)
        : inputManager(im), deviceManager(dm), channelPairGrid(im)
    {
        // Global Controls Header
        globalControlsHeader.setText("Trigger Sensitivity", juce::dontSendNotification);
        globalControlsHeader.setFont(juce::FontOptions(16.0f, juce::Font::bold));
        globalControlsHeader.setColour(juce::Label::textColourId, ThemeColours::Silver);
        addAndMakeVisible(globalControlsHeader);
        
        // Calibration Controls
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
        
        // Threshold Slider
        thresholdSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        thresholdSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
        thresholdSlider.setRange(0.001, 1.0, 0.001); 
        thresholdSlider.setSkewFactorFromMidPoint(0.1);
        thresholdSlider.setColour(juce::Slider::thumbColourId, ThemeColours::NeonCyan);
        thresholdSlider.setValue(im.getConfig().userThreshold, juce::dontSendNotification);
        thresholdSlider.onValueChange = [this]() {
            auto conf = inputManager.getConfig();
            conf.userThreshold = (float)thresholdSlider.getValue();
            inputManager.setConfig(conf);
            inputManager.getChannelManager().setGlobalThreshold((float)thresholdSlider.getValue());
        };
        addAndMakeVisible(thresholdSlider);
        
        threshLabel.setText("Sensitivity", juce::dontSendNotification);
        threshLabel.setColour(juce::Label::textColourId, ThemeColours::Silver);
        threshLabel.attachToComponent(&thresholdSlider, true);
        addAndMakeVisible(threshLabel);

        // Channel Detail Header
        channelListHeader.setText("Input Channel Details", juce::dontSendNotification);
        channelListHeader.setFont(juce::FontOptions(16.0f, juce::Font::bold));
        channelListHeader.setColour(juce::Label::textColourId, ThemeColours::Silver);
        addAndMakeVisible(channelListHeader);
        
        viewport.setViewedComponent(&channelPairGrid);
        viewport.setScrollBarsShown(true, false);
        addAndMakeVisible(viewport);

        updateChannelUI(true);
        startTimerHz(30);
    }
    
    void paint(juce::Graphics& g) override
    {
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
            
            float threshX = masterMeterRect.getX() + masterMeterRect.getWidth() * threshold;
            g.setColour(juce::Colours::white);
            g.drawLine(threshX, masterMeterRect.getY() - 2, threshX, masterMeterRect.getBottom() + 2, 2.0f);
            
            g.setColour(juce::Colours::white.withAlpha(0.2f));
            g.drawRect(masterMeterRect, 1.0f);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(15);
        
        // Global Controls
        globalControlsHeader.setBounds(area.removeFromTop(30));
        
        auto row1 = area.removeFromTop(35);
        useCalibrationButton.setBounds(row1.removeFromLeft(180).reduced(3));
        calibrateButton.setBounds(row1.removeFromLeft(180).reduced(3));
        
        auto row2 = area.removeFromTop(35);
        row2.removeFromLeft(90);
        thresholdSlider.setBounds(row2.reduced(3));
        
        masterMeterRect = area.removeFromTop(25).reduced(90, 3).toFloat();
        
        area.removeFromTop(10);
        
        // Channel List
        channelListHeader.setBounds(area.removeFromTop(30));
        viewport.setBounds(area);
        
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
                
                // アクティブなチャンネルの名前リストを生成
                juce::StringArray activeNames;
                auto allNames = device->getInputChannelNames();
                auto activeBits = device->getActiveInputChannels();
                
                for (int i = 0; i < allNames.size(); ++i)
                {
                    if (activeBits[i])
                        activeNames.add(allNames[i]);
                }
                
                channelPairGrid.refresh(activeNames);
            }
        }
    }

private:
    juce::Label globalControlsHeader;
    juce::TextButton useCalibrationButton;
    juce::TextButton calibrateButton;
    juce::Slider thresholdSlider;
    juce::Label threshLabel;
    juce::Rectangle<float> masterMeterRect;
    
    juce::Label channelListHeader;
    juce::Viewport viewport;
    ChannelPairGridContainer channelPairGrid;
    
    InputManager& inputManager;
    juce::AudioDeviceManager& deviceManager;
};

// =====================================================
// SettingsComponent（タブ形式）
// =====================================================
class SettingsComponent : public juce::Component
{
public:
    SettingsComponent(juce::AudioDeviceManager& dm, InputManager& im, 
                      MidiLearnManager& midiMgr, KeyboardMappingManager& keyMgr)
        : tabs(juce::TabbedButtonBar::TabsAtTop), midiManager(midiMgr), keyboardManager(keyMgr)
    {
        // ダークテーマ適用
        darkLAF.setColourScheme(juce::LookAndFeel_V4::getMidnightColourScheme());
        tabs.setLookAndFeel(&darkLAF);
        
        // タブカラー設定
        tabs.setColour(juce::TabbedComponent::backgroundColourId, juce::Colour(0xff151515));
        tabs.setColour(juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
        
        // タブ追加
        tabs.addTab("Device", juce::Colour(0xff1a1a1a), new DeviceTabContent(dm), true);
        tabs.addTab("Trigger", juce::Colour(0xff1a1a1a), new TriggerTabContent(dm, im), true);
        tabs.addTab("MIDI", juce::Colour(0xff1a1a1a), new MidiTabContent(midiMgr), true);
        tabs.addTab("Keyboard", juce::Colour(0xff1a1a1a), new KeyboardTabContent(keyMgr), true);
        
        addAndMakeVisible(tabs);
        setSize(750, 850);
    }
    
    ~SettingsComponent() override
    {
        tabs.setLookAndFeel(nullptr);
    }

    void resized() override
    {
        tabs.setBounds(getLocalBounds());
    }

private:
    juce::TabbedComponent tabs;
    juce::LookAndFeel_V4 darkLAF;
    MidiLearnManager& midiManager;
    KeyboardMappingManager& keyboardManager;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsComponent)
};

