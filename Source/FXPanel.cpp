/*
  ==============================================================================

    FXPanel.cpp
    Created: 31 Dec 2025
    Author:  mt sh

  ==============================================================================
*/

#include "FXPanel.h"
#include "MidiLearnManager.h"

FXPanel::FXPanel(LooperAudio& looperRef) : looper(looperRef)
{
    addAndMakeVisible(titleLabel);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, ThemeColours::NeonCyan);
    titleLabel.setText("Track FX Rack", juce::dontSendNotification);

    // スロットボタンの初期化
    for (int i = 0; i < 4; ++i)
    {
        slotButtons[i].setButtonText("Empty");
        slotButtons[i].setClickingTogglesState(true);
        slotButtons[i].setLookAndFeel(&slotLnF);
        slotButtons[i].setRadioGroupId(1);  // ラジオボタングループ（1つだけ選択可能）
        
        // 左クリック: 選択 + 空スロットならメニュー表示
        slotButtons[i].onClick = [this, i]() {
            selectedSlotIndex = i;
            // 他のスロットボタンの選択状態をクリア
            for (int j = 0; j < 4; ++j)
                slotButtons[j].setToggleState(j == i, juce::dontSendNotification);
            
            // エフェクトがNoneならメニューを表示
            if (slots[i].type == EffectType::None)
                showEffectMenu(i);
            
            updateSliderVisibility();
            repaint();
        };
        
        // 右クリック: 常にメニュー表示（エフェクト変更・クリア用）
        slotButtons[i].setTriggeredOnMouseDown(false);
        slotButtons[i].addMouseListener(this, false);  // FXPanelでマウスイベントを受け取る
        
        addAndMakeVisible(slotButtons[i]);
        
        // 縦型トグルスイッチの初期化（カスタム描画用に透明設定）
        bypassButtons[i].setClickingTogglesState(true);
        bypassButtons[i].setToggleState(true, juce::dontSendNotification); // 初期状態: ON（エフェクト有効）
        bypassButtons[i].setButtonText(""); // テキストなし
        // 透明に設定（paintOverChildrenでカスタム描画）
        bypassButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        bypassButtons[i].setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        bypassButtons[i].onClick = [this, i]() {
            // MIDI Learnモード時は学習対象として設定
            if (midiManager != nullptr && midiManager->isLearnModeActive())
            {
                juce::String controlId = "fx_slot" + juce::String(i + 1) + "_bypass";
                midiManager->setLearnTarget(controlId);
                DBG("MIDI Learn: Waiting for input - " << controlId);
                repaint();
                return;
            }
            
            bool isActive = bypassButtons[i].getToggleState();
            slots[i].isBypassed = !isActive;
            
            // エフェクトの有効/無効を切り替え
            if (currentTrackId >= 0 && slots[i].type != EffectType::None)
            {
                switch (slots[i].type) {
                    case EffectType::Filter: 
                        looper.setTrackFilterEnabled(currentTrackId, isActive); 
                        break;
                    case EffectType::Delay: 
                        looper.setTrackDelayEnabled(currentTrackId, isActive); 
                        break;
                    case EffectType::Reverb: 
                        looper.setTrackReverbEnabled(currentTrackId, isActive); 
                        break;
                    case EffectType::BeatRepeat: 
                        looper.setTrackBeatRepeatActive(currentTrackId, isActive); 
                        break;
                    default: break;
                }
            }
            
            updateSliderVisibility();
            repaint();
        };
        addAndMakeVisible(bypassButtons[i]);
    }
    addAndMakeVisible(visualizer);
    startTimer(30);

    slotButtons[0].setToggleState(true, juce::dontSendNotification);  // 最初のスロットを選択
    
    // トラック選択ボタンは削除（MainComponentのトラックボタンを使用）

    // --- FILTER ---
    setupSlider(filterSlider, filterLabel, "CUTOFF", "IceBlue");
    filterSlider.setRange(20.0, 20000.0, 1.0);
    filterSlider.setSkewFactorFromMidPoint(1000.0);
    filterSlider.setValue(20000.0);
    filterSlider.textFromValueFunction = [](double value) {
        if (value >= 1000.0)
            return juce::String(value / 1000.0, 1) + "kHz";
        else
            return juce::String(static_cast<int>(value)) + "Hz";
    };
    filterSlider.onValueChange = [this]() {
        float freq = (float)filterSlider.getValue();
        looper.setTrackFilterCutoff(currentTrackId, freq);
        visualizer.setFilterParameters(freq, (float)filterResSlider.getValue(), filterTypeButton.getToggleState() ? 1 : 0);
    };
    
    setupSlider(filterResSlider, filterResLabel, "RES", "IceBlue"); 
    filterResSlider.setRange(0.1, 10.0, 0.1);
    filterResSlider.setValue(0.707);
    filterResSlider.textFromValueFunction = [](double value) {
        return juce::String(value, 1);  // Q値は小数点1桁
    };
    filterResSlider.onValueChange = [this]() {
        float res = (float)filterResSlider.getValue();
        looper.setTrackFilterResonance(currentTrackId, res);
        visualizer.setFilterParameters((float)filterSlider.getValue(), res, filterTypeButton.getToggleState() ? 1 : 0);
    };
    
    addChildComponent(filterTypeButton);
    filterTypeButton.setClickingTogglesState(true);
    filterTypeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    filterTypeButton.setColour(juce::TextButton::buttonOnColourId, ThemeColours::NeonCyan);
    filterTypeButton.onClick = [this]() {
        bool isHPF = filterTypeButton.getToggleState();
        filterTypeButton.setButtonText(isHPF ? "HPF" : "LPF");
        looper.setTrackFilterType(currentTrackId, isHPF ? 1 : 0);
        visualizer.setFilterParameters((float)filterSlider.getValue(), (float)filterResSlider.getValue(), isHPF ? 1 : 0);
    };

    // --- COMP ---
    setupSlider(compThreshSlider, compThreshLabel, "THRESH", "MagmaRed");
    compThreshSlider.setRange(-60.0, 0.0, 1.0);
    compThreshSlider.setValue(-20.0);
    compThreshSlider.textFromValueFunction = [](double value) {
        return juce::String(static_cast<int>(value)) + "dB";
    };
    compThreshSlider.onValueChange = [this]() {
        looper.setTrackCompressor(currentTrackId, 
                                   (float)compThreshSlider.getValue(), 
                                   (float)compRatioSlider.getValue());
    };
    
    setupSlider(compRatioSlider, compRatioLabel, "RATIO", "MagmaRed");
    compRatioSlider.setRange(1.0, 20.0, 0.5);
    compRatioSlider.setValue(4.0);
    compRatioSlider.textFromValueFunction = [](double value) {
        return juce::String(value, 1) + ":1";
    };
    compRatioSlider.onValueChange = [this]() {
        looper.setTrackCompressor(currentTrackId, 
                                   (float)compThreshSlider.getValue(), 
                                   (float)compRatioSlider.getValue());
    };

    // --- DELAY ---
    setupSlider(delaySlider, delayLabel, "TIME", "BlackHole");
    delaySlider.setRange(0.0, 1000.0, 1.0);  // 0〜1000ms
    delaySlider.setValue(500.0);
    delaySlider.textFromValueFunction = [](double value) {
        return juce::String(static_cast<int>(value)) + "ms";
    };
    delaySlider.onValueChange = [this]() {
        // 内部値は0〜1に変換
        looper.setTrackDelayMix(currentTrackId, (float)delayMixSlider.getValue() / 100.0f, (float)delaySlider.getValue() / 1000.0f);
    };
    
    setupSlider(delayFeedbackSlider, delayFeedbackLabel, "F.BACK", "BlackHole");
    delayFeedbackSlider.setRange(0.0, 95.0, 1.0);  // 0〜95%
    delayFeedbackSlider.setValue(0.0);
    delayFeedbackSlider.textFromValueFunction = [](double value) {
        return juce::String(static_cast<int>(value)) + "%";
    };
    delayFeedbackSlider.onValueChange = [this]() {
        looper.setTrackDelayFeedback(currentTrackId, (float)delayFeedbackSlider.getValue() / 100.0f);
    };
    
    setupSlider(delayMixSlider, delayMixLabel, "MIX", "BlackHole");
    delayMixSlider.setRange(0.0, 80.0, 1.0);  // 0〜80%
    delayMixSlider.setValue(0.0);
    delayMixSlider.textFromValueFunction = [](double value) {
        return juce::String(static_cast<int>(value)) + "%";
    };
    delayMixSlider.onValueChange = [this]() {
        looper.setTrackDelayMix(currentTrackId, (float)delayMixSlider.getValue() / 100.0f, (float)delaySlider.getValue() / 1000.0f); 
    };

    // --- REVERB ---
    setupSlider(reverbSlider, reverbLabel, "MIX", "GasGiant");
    reverbSlider.setRange(0.0, 100.0, 1.0);  // 0〜100%
    reverbSlider.setValue(0.0);
    reverbSlider.textFromValueFunction = [](double value) {
        return juce::String(static_cast<int>(value)) + "%";
    };
    reverbSlider.onValueChange = [this]() {
        looper.setTrackReverbMix(currentTrackId, (float)reverbSlider.getValue() / 100.0f);
    };
    
    setupSlider(reverbDecaySlider, reverbDecayLabel, "DECAY", "GasGiant");
    reverbDecaySlider.setRange(0.0, 100.0, 1.0);  // 0〜100%
    reverbDecaySlider.setValue(50.0);
    reverbDecaySlider.textFromValueFunction = [](double value) {
        return juce::String(static_cast<int>(value)) + "%";
    };
    reverbDecaySlider.onValueChange = [this]() {
        looper.setTrackReverbRoomSize(currentTrackId, (float)reverbDecaySlider.getValue() / 100.0f);
    };
    
    // --- BEAT REPEAT ---
    setupSlider(repeatDivSlider, repeatDivLabel, "DIV", "IceBlue");
    repeatDivSlider.setRange(0, 7, 1);  // Internal: 0-7 -> Maps to 2^n: 1,2,4,8,16,32,64,128
    repeatDivSlider.setValue(2);  // Default: 4 (2^2)
    repeatDivSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    repeatDivSlider.textFromValueFunction = [](double value) {
        int div = 1 << static_cast<int>(value);  // 2^value
        return juce::String(div);
    };
    repeatDivSlider.valueFromTextFunction = [](const juce::String& text) {
        int div = text.getIntValue();
        return std::log2(std::max(1, div));
    };
    repeatDivSlider.onValueChange = [this]() {
        int div = 1 << static_cast<int>(repeatDivSlider.getValue());
        looper.setTrackBeatRepeatDiv(currentTrackId, div);
    };
    
    setupSlider(repeatThreshSlider, repeatThreshLabel, "THRESHOLD", "MagmaRed");
    repeatThreshSlider.setRange(0.01, 0.5, 0.01);
    repeatThreshSlider.setValue(0.1);
    repeatThreshSlider.onValueChange = [this]() {
        looper.setTrackBeatRepeatThresh(currentTrackId, (float)repeatThreshSlider.getValue());
    };
    
    addChildComponent(repeatActiveButton);
    repeatActiveButton.setClickingTogglesState(true);
    repeatActiveButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    repeatActiveButton.setColour(juce::TextButton::buttonOnColourId, ThemeColours::RecordingRed);
    repeatActiveButton.onClick = [this]() {
        bool active = repeatActiveButton.getToggleState();
        repeatActiveButton.setButtonText(active ? "REPEAT ON" : "REPEAT OFF");
        looper.setTrackBeatRepeatActive(currentTrackId, active);
    };

    updateSliderVisibility();
}

FXPanel::~FXPanel()
{
    // スロットボタンのLookAndFeelをクリア
    for (int i = 0; i < 4; ++i)
        slotButtons[i].setLookAndFeel(nullptr);
    
    // トラックボタンのLookAndFeelをクリア
    for (int i = 0; i < 8; ++i)
        trackButtons[i].setLookAndFeel(nullptr);
    
    filterSlider.setLookAndFeel(nullptr);
    filterResSlider.setLookAndFeel(nullptr);
    compThreshSlider.setLookAndFeel(nullptr);
    compRatioSlider.setLookAndFeel(nullptr);
    delaySlider.setLookAndFeel(nullptr);
    delayFeedbackSlider.setLookAndFeel(nullptr);
    delayMixSlider.setLookAndFeel(nullptr);
    reverbSlider.setLookAndFeel(nullptr);
    reverbDecaySlider.setLookAndFeel(nullptr);
    repeatDivSlider.setLookAndFeel(nullptr);
    repeatThreshSlider.setLookAndFeel(nullptr);
}

void FXPanel::setupSlider(juce::Slider& slider, juce::Label& label, const juce::String& name, const juce::String& style)
{
    addChildComponent(slider);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 18);
    slider.setLookAndFeel(&planetLnF);
    slider.getProperties().set("PlanetStyle", style);
    
    // 整数表示に設定
    slider.textFromValueFunction = [](double value) {
        return juce::String(static_cast<int>(std::round(value)));
    };
    slider.valueFromTextFunction = [](const juce::String& text) {
        return text.getDoubleValue();
    };
    slider.setNumDecimalPlacesToDisplay(0);
    
    // MIDI Learn用にリスナーを登録
    slider.addListener(this);

    addChildComponent(label);
    label.setText(name, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, ThemeColours::Silver);
    label.setFont(juce::FontOptions(12.0f));
}

void FXPanel::setTargetTrackId(int trackId)
{
    currentTrackId = trackId;
    looper.setMonitorTrackId(trackId); // Monitor audio for this track
    
    // スロットポインタを現在のトラックに切り替え
    if (trackId >= 1 && trackId <= 8)
        slots = trackSlots[trackId - 1];
    else
        slots = trackSlots[0];
    
    // スロットボタンとバイパスボタンの状態を更新
    for (int i = 0; i < 4; ++i)
    {
        // スロットボタンのテキスト更新
        juce::String typeStr;
        switch(slots[i].type) {
            case EffectType::Filter: typeStr = "Filter"; break;
            case EffectType::Compressor: typeStr = "Comp"; break;
            case EffectType::Delay: typeStr = "Delay"; break;
            case EffectType::Reverb: typeStr = "Reverb"; break;
            case EffectType::BeatRepeat: typeStr = "Repeat"; break;
            default: typeStr = "Empty"; break;
        }
        slotButtons[i].setButtonText(typeStr);
        
        // バイパスボタンの状態更新
        bypassButtons[i].setToggleState(!slots[i].isBypassed, juce::dontSendNotification);
    }
    
    // Update button states
    for (int i = 0; i < 8; ++i)
        trackButtons[i].setToggleState((i + 1) == trackId, juce::dontSendNotification);
    
    updateSliderVisibility();
    repaint();
}

void FXPanel::updateSliderVisibility()
{
    EffectType type = slots[selectedSlotIndex].type;
    bool isActive = (type != EffectType::None) && (!slots[selectedSlotIndex].isBypassed);
    if (slots[selectedSlotIndex].type == EffectType::None) isActive = false;

    // Hide all first
    auto hide = [&](juce::Component& c) { c.setVisible(false); };
    hide(filterSlider); hide(filterLabel);
    hide(filterResSlider); hide(filterResLabel);
    hide(filterTypeButton);
    hide(compThreshSlider); hide(compThreshLabel);
    hide(compRatioSlider); hide(compRatioLabel);
    hide(delaySlider); hide(delayLabel);
    hide(delayFeedbackSlider); hide(delayFeedbackLabel);
    hide(delayMixSlider); hide(delayMixLabel);
    hide(reverbSlider); hide(reverbLabel);
    hide(reverbDecaySlider); hide(reverbDecayLabel);
    hide(repeatActiveButton);
    hide(repeatDivSlider); hide(repeatDivLabel);
    hide(repeatThreshSlider); hide(repeatThreshLabel);
    
    visualizer.setVisible(false);

    if (!isActive) { resized(); return; }

    switch (type)
    {
        case EffectType::Filter:
            filterSlider.setVisible(true); filterLabel.setVisible(true);
            filterResSlider.setVisible(true); filterResLabel.setVisible(true);
            filterTypeButton.setVisible(true);
            visualizer.setVisible(true);
            break;
        case EffectType::Compressor:
            compThreshSlider.setVisible(true); compThreshLabel.setVisible(true);
            compRatioSlider.setVisible(true); compRatioLabel.setVisible(true);
            break;
        case EffectType::Delay:
            delaySlider.setVisible(true); delayLabel.setVisible(true);
            delayFeedbackSlider.setVisible(true); delayFeedbackLabel.setVisible(true);
            delayMixSlider.setVisible(true); delayMixLabel.setVisible(true);
            break;
        case EffectType::Reverb:
            reverbSlider.setVisible(true); reverbLabel.setVisible(true);
            reverbDecaySlider.setVisible(true); reverbDecayLabel.setVisible(true);
            break;
        case EffectType::BeatRepeat:
            repeatActiveButton.setVisible(true);
            repeatDivSlider.setVisible(true); repeatDivLabel.setVisible(true);
            repeatThreshSlider.setVisible(true); repeatThreshLabel.setVisible(true);
            break;
        default: break;
    }
    resized(); 
}

void FXPanel::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    if (area.isEmpty()) return;
    
    // Background
    g.setColour(juce::Colours::black.withAlpha(0.9f));
    g.fillRect(area);

    // Border
    g.setColour(ThemeColours::NeonCyan.withAlpha(0.5f));
    g.drawRect(area, 2.0f);
    
    // Layout Calculation
    auto fullArea = getLocalBounds().reduced(10);
    if (fullArea.getWidth() <= 10 || fullArea.getHeight() <= 40) return;

    fullArea.removeFromTop(40); // Title
    
    auto leftCol = fullArea.removeFromLeft(fullArea.getWidth() * 0.25f);
    auto rightArea = fullArea;
    rightArea.removeFromLeft(10); // Spacer
    
    // Draw Divider
    g.setColour(ThemeColours::Silver.withAlpha(0.3f));
    g.drawVerticalLine(leftCol.getRight() + 5, (float)fullArea.getY(), (float)fullArea.getBottom());
    
    // スロットボタンはコンポーネントとして描画されるので、ここでは何もしない
    
    // Hint
    if(slots[selectedSlotIndex].type == EffectType::None)
    {
        g.setColour(juce::Colours::grey);
        g.setFont(juce::FontOptions(18.0f));
        g.drawText("No Effect Selected\nClick slot to assign", rightArea, juce::Justification::centred, true);
    }
}

void FXPanel::resized()
{
    auto area = getLocalBounds();
    titleLabel.setBounds(area.removeFromTop(30));  // タイトル高さを縮小
    
    area.reduce(10, 5);  // 上下余白を縮小
    
    // 左カラム（スロットボタンのみ）
    auto leftCol = area.removeFromLeft(static_cast<int>(area.getWidth() * 0.20f));
    int dividerX = leftCol.getRight();
    
    // エフェクトスロットボタンの配置
    int slotHeight = 45;  // 高さを縮小
    int slotSpacing = 4;
    int toggleWidth = 24;  // 縦型トグルスイッチの幅
    int toggleSpacing = 4;
    
    for (int i = 0; i < 4; ++i)
    {
        auto slotRow = leftCol.removeFromTop(slotHeight);
        
        // トグルスイッチ用のスペースを右側に確保
        auto toggleRect = slotRow.removeFromRight(toggleWidth);
        slotRow.removeFromRight(toggleSpacing);
        
        // スロットボタン
        slotButtons[i].setBounds(slotRow.reduced(2, 0));
        
        // 縦型トグルスイッチ（上下でオン/オフ）
        bypassButtons[i].setBounds(toggleRect.reduced(0, 4));
        
        // エフェクトがNoneの場合はトグルスイッチを非表示
        bypassButtons[i].setVisible(slots[i].type != EffectType::None);
        
        leftCol.removeFromTop(slotSpacing);
    }
    
    // トラック選択ボタンは削除済み
    
    auto rightArea = area;
    rightArea.setLeft(dividerX + 20); // Add margin
    
    // Uniform Knob Layout - ノブを少し下に配置
    int knobSize = 65;  // ユーザー要望により7割程度に縮小 (90 -> 65)
    int textBoxHeight = 20;  // テキストボックスの高さ
    int labelHeight = 16;
    int spacing = 20;
    int startX = rightArea.getCentreX();
    int startY = rightArea.getY() + 15;

    // Helper to place a list of sliders centered
    auto placeControls = [&](std::vector<std::pair<juce::Slider*, juce::Label*>> controls, juce::Component* extra = nullptr) 
    {
        int extraWidth = (extra) ? 80 : 0;
        int totalWidth = (controls.size() * knobSize) + ((controls.size() - 1) * spacing);
        if (extra) totalWidth += spacing + extraWidth;

        int currentX = rightArea.getCentreX() - (totalWidth / 2);
        
        for(auto& p : controls)
        {
            auto* slider = p.first;
            auto* label = p.second;
            
            slider->setBounds(currentX, startY, knobSize, knobSize + textBoxHeight);
            label->setBounds(currentX, startY + knobSize + textBoxHeight + 2, knobSize, labelHeight);
            
            currentX += knobSize + spacing;
        }
        
        if (extra)
        {
            // Place extra component inline (to the right of knobs)
            int extraH = 30;
            // Center vertically with the knob circle (approx)
            int buttonY = startY + (knobSize / 2) - (extraH / 2) + 5; 
            extra->setBounds(currentX, buttonY, extraWidth, extraH);
        }
    };

    // FILTER
    if(filterSlider.isVisible()) {
        // Visualizer at the top (高さを縮小)
        int vizHeight = 70;
        visualizer.setBounds(rightArea.getX(), startY, rightArea.getWidth(), vizHeight);
        
        // Controls below visualizer
        int originalStartY = startY;
        startY += vizHeight + 10; 
        
        placeControls({ {&filterSlider, &filterLabel}, {&filterResSlider, &filterResLabel} }, &filterTypeButton);
        
        // Reset startY for next calls (though unexpected)
        startY = originalStartY;
    }
    
    // COMP
    if(compThreshSlider.isVisible()) {
        placeControls({ {&compThreshSlider, &compThreshLabel}, {&compRatioSlider, &compRatioLabel} });
    }
    
    // DELAY
    if(delaySlider.isVisible()) {
        placeControls({ {&delaySlider, &delayLabel}, {&delayFeedbackSlider, &delayFeedbackLabel}, {&delayMixSlider, &delayMixLabel} });
    }
    
    // REVERB
    if(reverbSlider.isVisible()) {
        placeControls({ {&reverbSlider, &reverbLabel}, {&reverbDecaySlider, &reverbDecayLabel} });
    }

    // BEAT REPEAT
    if(repeatDivSlider.isVisible()) {
        placeControls({ {&repeatDivSlider, &repeatDivLabel}, {&repeatThreshSlider, &repeatThreshLabel} }, &repeatActiveButton);
    }
}

void FXPanel::mouseDown(const juce::MouseEvent& e)
{
    // スロットボタンからの右クリックを検出
    for (int i = 0; i < 4; ++i)
    {
        if (e.eventComponent == &slotButtons[i])
        {
            if (e.mods.isPopupMenu())
            {
                // 右クリック: スロットを選択してメニュー表示
                selectedSlotIndex = i;
                for (int j = 0; j < 4; ++j)
                    slotButtons[j].setToggleState(j == i, juce::dontSendNotification);
                updateSliderVisibility();
                repaint();
                showEffectMenu(i);
                return;
            }
        }
    }
    
    // 従来のパネル領域クリック処理（フォールバック）
    auto fullArea = getLocalBounds().reduced(10);
    fullArea.removeFromTop(40); // Title
    auto leftCol = fullArea.removeFromLeft(fullArea.getWidth() * 0.25f);
    
    // Slots Layout
    int slotHeight = 50;
    int slotSpacing = 10;
    
    for(int i = 0; i < 4; ++i)
    {
        auto slotRect = leftCol.removeFromTop(slotHeight);
        leftCol.removeFromTop(slotSpacing);
        
        if(slotRect.contains(e.getPosition()))
        {
            selectedSlotIndex = i;
            for (int j = 0; j < 4; ++j)
                slotButtons[j].setToggleState(j == i, juce::dontSendNotification);
            updateSliderVisibility();
            repaint();
            
            // 右クリックまたはエフェクトがNoneの場合はメニュー表示
            if (e.mods.isPopupMenu() || slots[i].type == EffectType::None)
            {
               showEffectMenu(i);
            }
            return;
        }
    }
}

void FXPanel::showEffectMenu(int slotIndex)
{
    juce::PopupMenu m;
    m.addItem(1, "Filter", true, slots[slotIndex].type == EffectType::Filter);
    m.addItem(2, "Compressor", true, slots[slotIndex].type == EffectType::Compressor);
    m.addItem(3, "Delay", true, slots[slotIndex].type == EffectType::Delay);
    m.addItem(4, "Reverb", true, slots[slotIndex].type == EffectType::Reverb);
    m.addItem(5, "Beat Repeat", true, slots[slotIndex].type == EffectType::BeatRepeat);
    m.addSeparator();
    m.addItem(99, "Clear", true, false);

    m.showMenuAsync(juce::PopupMenu::Options(), [this, slotIndex](int result)
    {
        if (result == 0) return;
        
        EffectType oldType = slots[slotIndex].type;
        EffectType newType = EffectType::None;
        
        if (result == 99) newType = EffectType::None;
        else if (result == 1) newType = EffectType::Filter;
        else if (result == 2) newType = EffectType::Compressor;
        else if (result == 3) newType = EffectType::Delay;
        else if (result == 4) newType = EffectType::Reverb;
        else if (result == 5) newType = EffectType::BeatRepeat;
        
        // Disable old effect
        if (oldType != newType && currentTrackId >= 0)
        {
            switch (oldType) {
                case EffectType::Filter: looper.setTrackFilterEnabled(currentTrackId, false); break;
                case EffectType::Delay: looper.setTrackDelayEnabled(currentTrackId, false); break;
                case EffectType::Reverb: looper.setTrackReverbEnabled(currentTrackId, false); break;
                case EffectType::BeatRepeat: looper.setTrackBeatRepeatActive(currentTrackId, false); break;
                default: break;
            }
        }
        
        slots[slotIndex].type = newType;
        
        // スロットボタンのテキストを更新
        juce::String typeStr;
        switch(newType) {
            case EffectType::Filter: typeStr = "Filter"; break;
            case EffectType::Compressor: typeStr = "Comp"; break;
            case EffectType::Delay: typeStr = "Delay"; break;
            case EffectType::Reverb: typeStr = "Reverb"; break;
            case EffectType::BeatRepeat: typeStr = "Repeat"; break;
            default: typeStr = "Empty"; break;
        }
        slotButtons[slotIndex].setButtonText(typeStr);
        
        // Enable new effect
        if (newType != EffectType::None && currentTrackId >= 0)
        {
            switch (newType) {
                case EffectType::Filter: looper.setTrackFilterEnabled(currentTrackId, true); break;
                case EffectType::Delay: looper.setTrackDelayEnabled(currentTrackId, true); break;
                case EffectType::Reverb: looper.setTrackReverbEnabled(currentTrackId, true); break;
                default: break;
            }
        }
        
        updateSliderVisibility();
        repaint();
    });
}

void FXPanel::timerCallback()
{
    if (visualizer.isVisible())
    {
        juce::AudioBuffer<float> buffer(1, 1024); // Temp buffer
        looper.popMonitorSamples(buffer);
        visualizer.pushBuffer(buffer);
    }
}

// =====================================================
// MIDI Learn 対応
// =====================================================

void FXPanel::setMidiLearnManager(MidiLearnManager* manager)
{
    midiManager = manager;
}

juce::String FXPanel::getControlIdForSlider(juce::Slider* slider)
{
    if (slider == &filterSlider)        return "fx_filter_cutoff";
    if (slider == &filterResSlider)     return "fx_filter_res";
    if (slider == &compThreshSlider)    return "fx_comp_thresh";
    if (slider == &compRatioSlider)     return "fx_comp_ratio";
    if (slider == &delaySlider)         return "fx_delay_time";
    if (slider == &delayFeedbackSlider) return "fx_delay_feedback";
    if (slider == &delayMixSlider)      return "fx_delay_mix";
    if (slider == &reverbSlider)        return "fx_reverb_mix";
    if (slider == &reverbDecaySlider)   return "fx_reverb_decay";
    if (slider == &repeatDivSlider)     return "fx_repeat_div";
    if (slider == &repeatThreshSlider)  return "fx_repeat_thresh";
    return "";
}

juce::Slider* FXPanel::getSliderForControlId(const juce::String& controlId)
{
    if (controlId == "fx_filter_cutoff")    return &filterSlider;
    if (controlId == "fx_filter_res")       return &filterResSlider;
    if (controlId == "fx_comp_thresh")      return &compThreshSlider;
    if (controlId == "fx_comp_ratio")       return &compRatioSlider;
    if (controlId == "fx_delay_time")       return &delaySlider;
    if (controlId == "fx_delay_feedback")   return &delayFeedbackSlider;
    if (controlId == "fx_delay_mix")        return &delayMixSlider;
    if (controlId == "fx_reverb_mix")       return &reverbSlider;
    if (controlId == "fx_reverb_decay")     return &reverbDecaySlider;
    if (controlId == "fx_repeat_div")       return &repeatDivSlider;
    if (controlId == "fx_repeat_thresh")    return &repeatThreshSlider;
    return nullptr;
}

void FXPanel::handleMidiControl(const juce::String& controlId, float value)
{
    // UIスレッドで実行する
    juce::MessageManager::callAsync([this, controlId, value]()
    {
        auto* slider = getSliderForControlId(controlId);
        if (slider != nullptr)
        {
            // 正規化された値 (0.0-1.0) をスライダーの範囲に変換
            auto range = slider->getRange();
            // Skew factorを考慮
            double proportionalValue = slider->proportionOfLengthToValue(value);
            slider->setValue(proportionalValue, juce::sendNotificationSync);
        }
        // トグルボタン対応
        else if (controlId == "fx_filter_type")
        {
            filterTypeButton.setToggleState(value > 0.5f, juce::sendNotificationSync);
        }
        else if (controlId == "fx_repeat_active")
        {
            repeatActiveButton.setToggleState(value > 0.5f, juce::sendNotificationSync);
        }
        // FXスロットバイパスボタン対応
        else if (controlId.startsWith("fx_slot") && controlId.endsWith("_bypass"))
        {
            // fx_slot1_bypass -> slot index 0
            int slotIndex = controlId.substring(7, 8).getIntValue() - 1;
            if (slotIndex >= 0 && slotIndex < 4)
            {
                // トグル動作（MIDIトリガーで切り替え）
                bool newState = !bypassButtons[slotIndex].getToggleState();
                bypassButtons[slotIndex].setToggleState(newState, juce::sendNotificationSync);
                bypassButtons[slotIndex].onClick(); // コールバックを呼び出してエフェクト状態も更新
            }
        }
    });
}

void FXPanel::paintOverChildren(juce::Graphics& g)
{
    // 縦型トグルスイッチのカスタム描画
    for (int i = 0; i < 4; ++i)
    {
        if (!bypassButtons[i].isVisible()) continue;
        
        auto bounds = bypassButtons[i].getBounds().toFloat();
        bool isOn = bypassButtons[i].getToggleState();
        
        // スイッチの背景（角丸長方形）
        float cornerRadius = bounds.getWidth() * 0.3f;
        
        // 背景色（暗いグレー）
        g.setColour(juce::Colour(30, 30, 35));
        g.fillRoundedRectangle(bounds, cornerRadius);
        
        // 枠線
        g.setColour(isOn ? ThemeColours::PlayingGreen.withAlpha(0.8f) : ThemeColours::Silver.withAlpha(0.3f));
        g.drawRoundedRectangle(bounds.reduced(1.0f), cornerRadius, 1.5f);
        
        // スイッチノブ（上下に動く）
        float knobHeight = bounds.getHeight() * 0.45f;
        float knobWidth = bounds.getWidth() - 6.0f;
        float knobX = bounds.getX() + 3.0f;
        float knobY = isOn ? (bounds.getY() + 3.0f) : (bounds.getBottom() - knobHeight - 3.0f);
        
        juce::Rectangle<float> knobRect(knobX, knobY, knobWidth, knobHeight);
        
        // ノブのグラデーション
        juce::Colour knobColour = isOn ? ThemeColours::PlayingGreen : juce::Colour(80, 80, 85);
        g.setColour(knobColour);
        g.fillRoundedRectangle(knobRect, cornerRadius * 0.7f);
        
        // ノブのハイライト
        g.setColour(knobColour.brighter(0.3f));
        g.drawRoundedRectangle(knobRect.reduced(1.0f), cornerRadius * 0.7f, 1.0f);
        
        // ON/OFFインジケータ（小さなドット）
        float dotSize = 4.0f;
        float dotX = bounds.getCentreX() - dotSize / 2.0f;
        float dotY = isOn ? (bounds.getBottom() - 8.0f) : (bounds.getY() + 4.0f);
        g.setColour(isOn ? ThemeColours::PlayingGreen.withAlpha(0.5f) : ThemeColours::Silver.withAlpha(0.3f));
        g.fillEllipse(dotX, dotY, dotSize, dotSize);
    }
    
    // MIDI Learnモード時のオーバーレイ
    if (midiManager == nullptr || !midiManager->isLearnModeActive())
        return;
    // 可視状態のスライダーに対してのみオーバーレイを描画
    std::vector<juce::Slider*> sliders = {
        &filterSlider, &filterResSlider,
        &compThreshSlider, &compRatioSlider,
        &delaySlider, &delayFeedbackSlider, &delayMixSlider,
        &reverbSlider, &reverbDecaySlider,
        &repeatDivSlider, &repeatThreshSlider
    };
    
    float alpha = juce::jlimit(0.0f, 1.0f, 0.5f + 0.2f * std::sin(juce::Time::getMillisecondCounter() * 0.01f));
    
    for (auto* slider : sliders)
    {
        if (!slider->isVisible()) continue;
        
        juce::String controlId = getControlIdForSlider(slider);
        if (controlId.isEmpty()) continue;
        
        auto bounds = slider->getBounds().toFloat().expanded(3.0f);
        
        // 1. 学習対象として選択されている場合（黄色点滅）
        if (midiManager->getLearnTarget() == controlId)
        {
            g.setColour(juce::Colours::yellow.withAlpha(alpha));
            g.drawRoundedRectangle(bounds, 6.0f, 3.0f);
            
            g.setColour(juce::Colours::yellow.withAlpha(0.2f));
            g.fillRoundedRectangle(bounds, 6.0f);
        }
        // 2. 既にマッピングされている場合（緑枠）
        else if (midiManager->hasMapping(controlId))
        {
            g.setColour(ThemeColours::PlayingGreen.withAlpha(0.8f));
            g.drawRoundedRectangle(bounds, 6.0f, 2.0f);
        }
        // 3. マッピングされていないが対象可能な場合（薄い白枠でヒント）
        else
        {
            g.setColour(ThemeColours::Silver.withAlpha(0.2f));
            g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
        }
    }
    
    // トグルボタンも同様に処理
    auto drawButtonOverlay = [&](juce::TextButton& btn, const juce::String& ctrlId) {
        if (!btn.isVisible()) return;
        auto bounds = btn.getBounds().toFloat().expanded(2.0f);
        
        if (midiManager->getLearnTarget() == ctrlId)
        {
            g.setColour(juce::Colours::yellow.withAlpha(alpha));
            g.drawRoundedRectangle(bounds, 4.0f, 2.0f);
        }
        else if (midiManager->hasMapping(ctrlId))
        {
            g.setColour(ThemeColours::PlayingGreen.withAlpha(0.8f));
            g.drawRoundedRectangle(bounds, 4.0f, 2.0f);
        }
    };
    drawButtonOverlay(filterTypeButton, "fx_filter_type");
    drawButtonOverlay(repeatActiveButton, "fx_repeat_active");
    
    // FXスロットバイパスボタン
    for (int i = 0; i < 4; ++i)
    {
        juce::String ctrlId = "fx_slot" + juce::String(i + 1) + "_bypass";
        drawButtonOverlay(bypassButtons[i], ctrlId);
    }
}

void FXPanel::sliderValueChanged(juce::Slider* slider)
{
    // 何もしない (既存のonValueChangeコールバックで処理されている)
}

void FXPanel::sliderDragStarted(juce::Slider* slider)
{
    // MIDI Learnモード時はスライダーの操作をマッピング登録に変える
    if (midiManager != nullptr && midiManager->isLearnModeActive())
    {
        juce::String controlId = getControlIdForSlider(slider);
        if (controlId.isNotEmpty())
        {
            midiManager->setLearnTarget(controlId);
            DBG("MIDI Learn: Waiting for input - " << controlId);
            repaint();
        }
    }
}

// =====================================================
// トラック別FXトグルメソッド
// =====================================================

void FXPanel::toggleSlotBypass(int trackId, int slotIndex)
{
    if (trackId < 1 || trackId > 8 || slotIndex < 0 || slotIndex >= 4)
        return;
    
    auto& slot = trackSlots[trackId - 1][slotIndex];
    slot.isBypassed = !slot.isBypassed;
    
    // エフェクトの有効/無効を切り替え
    bool isActive = !slot.isBypassed;
    
    switch (slot.type) {
        case EffectType::Filter: 
            looper.setTrackFilterEnabled(trackId, isActive); 
            break;
        case EffectType::Delay: 
            looper.setTrackDelayEnabled(trackId, isActive); 
            break;
        case EffectType::Reverb: 
            looper.setTrackReverbEnabled(trackId, isActive); 
            break;
        case EffectType::BeatRepeat: 
            looper.setTrackBeatRepeatActive(trackId, isActive); 
            break;
        default: break;
    }
    
    // 現在表示中のトラックならUIを更新
    if (trackId == currentTrackId)
    {
        bypassButtons[slotIndex].setToggleState(isActive, juce::dontSendNotification);
        updateSliderVisibility();
        repaint();
    }
    
    DBG("FX Slot Toggle: Track " << trackId << " Slot " << (slotIndex + 1) << " -> " << (isActive ? "ON" : "OFF"));
}

void FXPanel::toggleFilterType(int trackId)
{
    // フィルタータイプは現在グローバルなので、選択中のトラックに対して操作
    if (trackId < 1 || trackId > 8)
        return;
    
    // 現在の状態を反転
    bool newState = !filterTypeButton.getToggleState();
    filterTypeButton.setToggleState(newState, juce::sendNotificationSync);
    
    DBG("Filter Type Toggle: Track " << trackId << " -> " << (newState ? "HPF" : "LPF"));
}

void FXPanel::toggleRepeatActive(int trackId)
{
    if (trackId < 1 || trackId > 8)
        return;
    
    // 現在の状態を反転
    bool newState = !repeatActiveButton.getToggleState();
    repeatActiveButton.setToggleState(newState, juce::sendNotificationSync);
    
    // looperにも反映
    looper.setTrackBeatRepeatActive(trackId, newState);
    
    DBG("Beat Repeat Toggle: Track " << trackId << " -> " << (newState ? "ON" : "OFF"));
}

bool FXPanel::isSlotBypassed(int trackId, int slotIndex) const
{
    if (trackId < 1 || trackId > 8 || slotIndex < 0 || slotIndex >= 4)
        return false;
    
    return trackSlots[trackId - 1][slotIndex].isBypassed;
}
