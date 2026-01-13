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
                    case EffectType::Flanger:
                        looper.setTrackFlangerEnabled(currentTrackId, isActive);
                        break;
                    case EffectType::Chorus:
                        looper.setTrackChorusEnabled(currentTrackId, isActive);
                        break;
                    case EffectType::Tremolo:
                        looper.setTrackTremoloEnabled(currentTrackId, isActive);
                        break;
                    case EffectType::Slicer:
                        looper.setTrackSlicerEnabled(currentTrackId, isActive);
                        break;
                    case EffectType::Bitcrusher:
                        looper.setTrackBitcrusherEnabled(currentTrackId, isActive);
                        break;
                    case EffectType::GranularCloud:
                        looper.setTrackGranularEnabled(currentTrackId, isActive);
                        break;
                    case EffectType::Autotune:
                        looper.setTrackAutotuneEnabled(currentTrackId, isActive);
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
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&filterSlider))
                filterSlider.setValue(lastSliderValues[&filterSlider], juce::dontSendNotification);
            return;
        }
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
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&filterResSlider))
                filterResSlider.setValue(lastSliderValues[&filterResSlider], juce::dontSendNotification);
            return;
        }
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
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&compThreshSlider))
                compThreshSlider.setValue(lastSliderValues[&compThreshSlider], juce::dontSendNotification);
            return;
        }
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
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&compRatioSlider))
                compRatioSlider.setValue(lastSliderValues[&compRatioSlider], juce::dontSendNotification);
            return;
        }
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
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&delaySlider))
                delaySlider.setValue(lastSliderValues[&delaySlider], juce::dontSendNotification);
            return;
        }
        looper.setTrackDelayMix(currentTrackId, (float)delayMixSlider.getValue() / 100.0f, (float)delaySlider.getValue() / 1000.0f);
    };
    
    setupSlider(delayFeedbackSlider, delayFeedbackLabel, "F.BACK", "BlackHole");
    delayFeedbackSlider.setRange(0.0, 95.0, 1.0);  // 0〜95%
    delayFeedbackSlider.setValue(0.0);
    delayFeedbackSlider.textFromValueFunction = [](double value) {
        return juce::String(static_cast<int>(value)) + "%";
    };
    delayFeedbackSlider.onValueChange = [this]() {
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&delayFeedbackSlider))
                delayFeedbackSlider.setValue(lastSliderValues[&delayFeedbackSlider], juce::dontSendNotification);
            return;
        }
        looper.setTrackDelayFeedback(currentTrackId, (float)delayFeedbackSlider.getValue() / 100.0f);
    };
    
    setupSlider(delayMixSlider, delayMixLabel, "MIX", "BlackHole");
    delayMixSlider.setRange(0.0, 80.0, 1.0);  // 0〜80%
    delayMixSlider.setValue(0.0);
    delayMixSlider.textFromValueFunction = [](double value) {
        return juce::String(static_cast<int>(value)) + "%";
    };
    delayMixSlider.onValueChange = [this]() {
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&delayMixSlider))
                delayMixSlider.setValue(lastSliderValues[&delayMixSlider], juce::dontSendNotification);
            return;
        }
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
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&reverbSlider))
                reverbSlider.setValue(lastSliderValues[&reverbSlider], juce::dontSendNotification);
            return;
        }
        looper.setTrackReverbMix(currentTrackId, (float)reverbSlider.getValue() / 100.0f);
    };
    
    setupSlider(reverbDecaySlider, reverbDecayLabel, "DECAY", "GasGiant");
    reverbDecaySlider.setRange(0.0, 100.0, 1.0);  // 0〜100%
    reverbDecaySlider.setValue(50.0);
    reverbDecaySlider.textFromValueFunction = [](double value) {
        return juce::String(static_cast<int>(value)) + "%";
    };
    reverbDecaySlider.onValueChange = [this]() {
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&reverbDecaySlider))
                reverbDecaySlider.setValue(lastSliderValues[&reverbDecaySlider], juce::dontSendNotification);
            return;
        }
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
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&repeatDivSlider))
                repeatDivSlider.setValue(lastSliderValues[&repeatDivSlider], juce::dontSendNotification);
            return;
        }
        int div = 1 << static_cast<int>(repeatDivSlider.getValue());
        looper.setTrackBeatRepeatDiv(currentTrackId, div);
    };
    
    setupSlider(repeatThreshSlider, repeatThreshLabel, "THRESHOLD", "MagmaRed");
    repeatThreshSlider.setRange(0.01, 0.5, 0.01);
    repeatThreshSlider.setValue(0.1);
    repeatThreshSlider.onValueChange = [this]() {
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&repeatThreshSlider))
                repeatThreshSlider.setValue(lastSliderValues[&repeatThreshSlider], juce::dontSendNotification);
            return;
        }
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

    // --- FLANGER ---
    setupSlider(flangerRateSlider, flangerRateLabel, "RATE", "IceBlue");
    flangerRateSlider.setRange(0.1, 5.0, 0.1);
    flangerRateSlider.setValue(0.5);
    flangerRateSlider.textFromValueFunction = [](double value) {
        return juce::String(value, 1) + "Hz";
    };
    flangerRateSlider.onValueChange = [this]() {
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&flangerRateSlider))
                flangerRateSlider.setValue(lastSliderValues[&flangerRateSlider], juce::dontSendNotification);
            return;
        }
        looper.setTrackFlangerRate(currentTrackId, (float)flangerRateSlider.getValue());
    };

    setupSlider(flangerDepthSlider, flangerDepthLabel, "DEPTH", "IceBlue");
    flangerDepthSlider.setRange(0.0, 100.0, 1.0);
    flangerDepthSlider.setValue(50.0);
    flangerDepthSlider.textFromValueFunction = [](double value) {
        return juce::String(static_cast<int>(value)) + "%";
    };
    flangerDepthSlider.onValueChange = [this]() {
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&flangerDepthSlider))
                flangerDepthSlider.setValue(lastSliderValues[&flangerDepthSlider], juce::dontSendNotification);
            return;
        }
        looper.setTrackFlangerDepth(currentTrackId, (float)flangerDepthSlider.getValue() / 100.0f);
    };

    setupSlider(flangerFeedbackSlider, flangerFeedbackLabel, "F.BACK", "IceBlue");
    flangerFeedbackSlider.setRange(0.0, 95.0, 1.0);
    flangerFeedbackSlider.setValue(0.0);
    flangerFeedbackSlider.textFromValueFunction = [](double value) {
        return juce::String(static_cast<int>(value)) + "%";
    };
    flangerFeedbackSlider.onValueChange = [this]() {
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&flangerFeedbackSlider))
                flangerFeedbackSlider.setValue(lastSliderValues[&flangerFeedbackSlider], juce::dontSendNotification);
            return;
        }
        looper.setTrackFlangerFeedback(currentTrackId, (float)flangerFeedbackSlider.getValue() / 100.0f);
    };

    addChildComponent(flangerSyncButton);
    flangerSyncButton.setClickingTogglesState(true);
    flangerSyncButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    flangerSyncButton.setColour(juce::TextButton::buttonOnColourId, ThemeColours::ElectricBlue);
    flangerSyncButton.onClick = [this]() {
        bool sync = flangerSyncButton.getToggleState();
        slots[selectedSlotIndex].flangerSync = sync;
        looper.setTrackFlangerSync(currentTrackId, sync);
        updateSliderVisibility();
    };

    // --- CHORUS ---
    setupSlider(chorusRateSlider, chorusRateLabel, "RATE", "IceBlue");
    chorusRateSlider.setRange(0.1, 2.0, 0.1);
    chorusRateSlider.setValue(0.3);
    chorusRateSlider.textFromValueFunction = [](double value) {
        return juce::String(value, 1) + "Hz";
    };
    chorusRateSlider.onValueChange = [this]() {
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&chorusRateSlider))
                chorusRateSlider.setValue(lastSliderValues[&chorusRateSlider], juce::dontSendNotification);
            return;
        }
        looper.setTrackChorusRate(currentTrackId, (float)chorusRateSlider.getValue());
    };

    setupSlider(chorusDepthSlider, chorusDepthLabel, "DEPTH", "IceBlue");
    chorusDepthSlider.setRange(0.0, 100.0, 1.0);
    chorusDepthSlider.setValue(50.0);
    chorusDepthSlider.textFromValueFunction = [](double value) {
        return juce::String(static_cast<int>(value)) + "%";
    };
    chorusDepthSlider.onValueChange = [this]() {
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&chorusDepthSlider))
                chorusDepthSlider.setValue(lastSliderValues[&chorusDepthSlider], juce::dontSendNotification);
            return;
        }
        looper.setTrackChorusDepth(currentTrackId, (float)chorusDepthSlider.getValue() / 100.0f);
    };

    setupSlider(chorusMixSlider, chorusMixLabel, "MIX", "IceBlue");
    chorusMixSlider.setRange(0.0, 100.0, 1.0);
    chorusMixSlider.setValue(50.0);
    chorusMixSlider.textFromValueFunction = [](double value) {
        return juce::String(static_cast<int>(value)) + "%";
    };
    chorusMixSlider.onValueChange = [this]() {
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&chorusMixSlider))
                chorusMixSlider.setValue(lastSliderValues[&chorusMixSlider], juce::dontSendNotification);
            return;
        }
        looper.setTrackChorusMix(currentTrackId, (float)chorusMixSlider.getValue() / 100.0f);
    };

    addChildComponent(chorusSyncButton);
    chorusSyncButton.setClickingTogglesState(true);
    chorusSyncButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    chorusSyncButton.setColour(juce::TextButton::buttonOnColourId, ThemeColours::ElectricBlue);
    chorusSyncButton.onClick = [this]() {
        bool sync = chorusSyncButton.getToggleState();
        slots[selectedSlotIndex].chorusSync = sync;
        looper.setTrackChorusSync(currentTrackId, sync);
        updateSliderVisibility();
    };

    // --- TREMOLO ---
    setupSlider(tremoloRateSlider, tremoloRateLabel, "RATE", "IceBlue");
    tremoloRateSlider.setRange(0.5, 20.0, 0.5);
    tremoloRateSlider.setValue(4.0);
    tremoloRateSlider.textFromValueFunction = [](double value) {
        return juce::String(value, 1) + "Hz";
    };
    tremoloRateSlider.onValueChange = [this]() {
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&tremoloRateSlider))
                tremoloRateSlider.setValue(lastSliderValues[&tremoloRateSlider], juce::dontSendNotification);
            return;
        }
        looper.setTrackTremoloRate(currentTrackId, (float)tremoloRateSlider.getValue());
    };

    setupSlider(tremoloDepthSlider, tremoloDepthLabel, "DEPTH", "IceBlue");
    tremoloDepthSlider.setRange(0.0, 100.0, 1.0);
    tremoloDepthSlider.setValue(50.0);
    tremoloDepthSlider.textFromValueFunction = [](double value) {
        return juce::String(static_cast<int>(value)) + "%";
    };
    tremoloDepthSlider.onValueChange = [this]() {
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&tremoloDepthSlider))
                tremoloDepthSlider.setValue(lastSliderValues[&tremoloDepthSlider], juce::dontSendNotification);
            return;
        }
        looper.setTrackTremoloDepth(currentTrackId, (float)tremoloDepthSlider.getValue() / 100.0f);
    };

    // Shape Button (Sine -> Square -> Triangle cycle)
    addChildComponent(tremoloShapeButton);
    tremoloShapeButton.setClickingTogglesState(false);
    tremoloShapeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    tremoloShapeButton.onClick = [this]() {
        static int currentShape = 0;
        currentShape = (currentShape + 1) % 3;
        juce::String shapes[] = { "SINE", "SQUARE", "TRI" };
        tremoloShapeButton.setButtonText(shapes[currentShape]);
        looper.setTrackTremoloShape(currentTrackId, currentShape);
    };

    addChildComponent(tremoloSyncButton);
    tremoloSyncButton.setClickingTogglesState(true);
    tremoloSyncButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    tremoloSyncButton.setColour(juce::TextButton::buttonOnColourId, ThemeColours::ElectricBlue);
    tremoloSyncButton.onClick = [this]() {
        bool sync = tremoloSyncButton.getToggleState();
        slots[selectedSlotIndex].tremoloSync = sync;
        looper.setTrackTremoloSync(currentTrackId, sync);
        updateSliderVisibility();
    };

    // --- SLICER / TRANCE GATE ---
    setupSlider(slicerRateSlider, slicerRateLabel, "RATE", "Amber");
    slicerRateSlider.setRange(0.5, 20.0, 0.5);
    slicerRateSlider.setValue(4.0);
    slicerRateSlider.textFromValueFunction = [](double value) {
        return juce::String(value, 1) + "Hz";
    };
    slicerRateSlider.onValueChange = [this]() {
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&slicerRateSlider))
                slicerRateSlider.setValue(lastSliderValues[&slicerRateSlider], juce::dontSendNotification);
            return;
        }
        looper.setTrackSlicerRate(currentTrackId, (float)slicerRateSlider.getValue());
    };

    setupSlider(slicerDepthSlider, slicerDepthLabel, "DEPTH", "Amber");
    slicerDepthSlider.setRange(0.0, 100.0, 1.0);
    slicerDepthSlider.setValue(100.0);
    slicerDepthSlider.textFromValueFunction = [](double value) {
        return juce::String(static_cast<int>(value)) + "%";
    };
    slicerDepthSlider.onValueChange = [this]() {
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&slicerDepthSlider))
                slicerDepthSlider.setValue(lastSliderValues[&slicerDepthSlider], juce::dontSendNotification);
            return;
        }
        looper.setTrackSlicerDepth(currentTrackId, (float)slicerDepthSlider.getValue() / 100.0f);
    };

    setupSlider(slicerDutySlider, slicerDutyLabel, "DUTY", "Amber");
    slicerDutySlider.setRange(10.0, 90.0, 5.0);
    slicerDutySlider.setValue(50.0);
    slicerDutySlider.textFromValueFunction = [](double value) {
        return juce::String(static_cast<int>(value)) + "%";
    };
    slicerDutySlider.onValueChange = [this]() {
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&slicerDutySlider))
                slicerDutySlider.setValue(lastSliderValues[&slicerDutySlider], juce::dontSendNotification);
            return;
        }
        looper.setTrackSlicerDuty(currentTrackId, (float)slicerDutySlider.getValue() / 100.0f);
    };

    // Shape Button (Square -> Smooth cycle)
    addChildComponent(slicerShapeButton);
    slicerShapeButton.setClickingTogglesState(false);
    slicerShapeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    slicerShapeButton.onClick = [this]() {
        static int currentShape = 0;
        currentShape = (currentShape + 1) % 2;
        juce::String shapes[] = { "SQUARE", "SMOOTH" };
        slicerShapeButton.setButtonText(shapes[currentShape]);
        looper.setTrackSlicerShape(currentTrackId, currentShape);
    };

    addChildComponent(slicerSyncButton);
    slicerSyncButton.setClickingTogglesState(true);
    slicerSyncButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    slicerSyncButton.setColour(juce::TextButton::buttonOnColourId, ThemeColours::ElectricBlue);
    slicerSyncButton.onClick = [this]() {
        bool sync = slicerSyncButton.getToggleState();
        slots[selectedSlotIndex].slicerSync = sync;
        looper.setTrackSlicerSync(currentTrackId, sync);
        updateSliderVisibility();
    };

    // --- BITCRUISHER ---
    setupSlider(bitcrusherBitsSlider, bitcrusherBitsLabel, "BITS", "Amber");
    bitcrusherBitsSlider.setRange(0.0, 1.0, 0.01);
    bitcrusherBitsSlider.setValue(0.0);
    bitcrusherBitsSlider.textFromValueFunction = [](double value) {
        // 0.0=24bit, 1.0=4bit
        int bits = (int)(24.0 - (value * 20.0));
        return juce::String(bits) + "bit";
    };
    bitcrusherBitsSlider.onValueChange = [this]() {
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&bitcrusherBitsSlider))
                bitcrusherBitsSlider.setValue(lastSliderValues[&bitcrusherBitsSlider], juce::dontSendNotification);
            return;
        }
        looper.setTrackBitcrusherDepth(currentTrackId, (float)bitcrusherBitsSlider.getValue());
    };

    setupSlider(bitcrusherRateSlider, bitcrusherRateLabel, "DS RATE", "Amber");
    bitcrusherRateSlider.setRange(0.0, 1.0, 0.01);
    bitcrusherRateSlider.setValue(0.0);
    bitcrusherRateSlider.textFromValueFunction = [](double value) {
        // 0.0=1x, 1.0=40x
        int rate = 1 + (int)(value * 40.0);
        return "1/" + juce::String(rate);
    };
    bitcrusherRateSlider.onValueChange = [this]() {
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&bitcrusherRateSlider))
                bitcrusherRateSlider.setValue(lastSliderValues[&bitcrusherRateSlider], juce::dontSendNotification);
            return;
        }
        looper.setTrackBitcrusherRate(currentTrackId, (float)bitcrusherRateSlider.getValue());
    };

    // --- GRANULAR CLOUD ---
    setupSlider(granSizeSlider, granSizeLabel, "SIZE", "GalaxyPurple");
    granSizeSlider.setRange(20.0, 500.0, 1.0);
    granSizeSlider.setValue(100.0);
    granSizeSlider.textFromValueFunction = [](double value) { return juce::String((int)value) + "ms"; };
    granSizeSlider.onValueChange = [this]() {
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&granSizeSlider))
                granSizeSlider.setValue(lastSliderValues[&granSizeSlider], juce::dontSendNotification);
            return;
        }
        looper.setTrackGranularSize(currentTrackId, (float)granSizeSlider.getValue());
    };

    setupSlider(granDenseSlider, granDenseLabel, "DENSE", "GalaxyPurple");
    granDenseSlider.setRange(0.0, 1.0, 0.01);
    granDenseSlider.setValue(0.5);
    granDenseSlider.textFromValueFunction = [](double value) { return juce::String((int)(value * 100)) + "%"; };
    granDenseSlider.onValueChange = [this]() {
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&granDenseSlider))
                granDenseSlider.setValue(lastSliderValues[&granDenseSlider], juce::dontSendNotification);
            return;
        }
        looper.setTrackGranularDensity(currentTrackId, (float)granDenseSlider.getValue());
    };

    setupSlider(granPitchSlider, granPitchLabel, "PITCH", "GalaxyPurple");
    granPitchSlider.setRange(0.5, 2.0, 0.01);
    granPitchSlider.setValue(1.0);
    granPitchSlider.textFromValueFunction = [](double value) { return juce::String(value, 2) + "x"; };
    granPitchSlider.onValueChange = [this]() {
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&granPitchSlider))
                granPitchSlider.setValue(lastSliderValues[&granPitchSlider], juce::dontSendNotification);
            return;
        }
    looper.setTrackGranularPitch(currentTrackId, (float)granPitchSlider.getValue());
    };
    
    setupSlider(granJitterSlider, granJitterLabel, "JITTER", "GalaxyPurple");
    granJitterSlider.setRange(0.0, 1.0, 0.01);
    granJitterSlider.setValue(0.5);
    granJitterSlider.textFromValueFunction = [](double value) { return juce::String((int)(value * 100)) + "%"; };
    granJitterSlider.onValueChange = [this]() {
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&granJitterSlider))
                granJitterSlider.setValue(lastSliderValues[&granJitterSlider], juce::dontSendNotification);
            return;
        }
        looper.setTrackGranularJitter(currentTrackId, (float)granJitterSlider.getValue());
    };

    setupSlider(granMixSlider, granMixLabel, "MIX", "GalaxyPurple");
    granMixSlider.setRange(0.0, 1.0, 0.01);
    granMixSlider.setValue(0.5);
    granMixSlider.textFromValueFunction = [](double value) { return juce::String((int)(value * 100)) + "%"; };
    granMixSlider.onValueChange = [this]() {
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&granMixSlider))
                granMixSlider.setValue(lastSliderValues[&granMixSlider], juce::dontSendNotification);
            return;
        }
        looper.setTrackGranularMix(currentTrackId, (float)granMixSlider.getValue());
    };

    // --- AUTOTUNE ---
    addChildComponent(autotuneKeyCombo);
    autotuneKeyCombo.addItem("C", 1);
    autotuneKeyCombo.addItem("C#", 2);
    autotuneKeyCombo.addItem("D", 3);
    autotuneKeyCombo.addItem("D#", 4);
    autotuneKeyCombo.addItem("E", 5);
    autotuneKeyCombo.addItem("F", 6);
    autotuneKeyCombo.addItem("F#", 7);
    autotuneKeyCombo.addItem("G", 8);
    autotuneKeyCombo.addItem("G#", 9);
    autotuneKeyCombo.addItem("A", 10);
    autotuneKeyCombo.addItem("A#", 11);
    autotuneKeyCombo.addItem("B", 12);
    autotuneKeyCombo.setSelectedId(1);
    autotuneKeyCombo.onChange = [this]() {
        looper.setTrackAutotuneKey(currentTrackId, autotuneKeyCombo.getSelectedId() - 1);
    };
    addChildComponent(autotuneKeyLabel);
    autotuneKeyLabel.setText("KEY", juce::dontSendNotification);
    autotuneKeyLabel.setJustificationType(juce::Justification::centred);
    autotuneKeyLabel.setColour(juce::Label::textColourId, ThemeColours::NeonCyan);

    addChildComponent(autotuneScaleCombo);
    autotuneScaleCombo.addItem("Chromatic", 1);
    autotuneScaleCombo.addItem("Major", 2);
    autotuneScaleCombo.addItem("Minor", 3);
    autotuneScaleCombo.setSelectedId(1);
    autotuneScaleCombo.onChange = [this]() {
        looper.setTrackAutotuneScale(currentTrackId, autotuneScaleCombo.getSelectedId() - 1);
    };
    addChildComponent(autotuneScaleLabel);
    autotuneScaleLabel.setText("SCALE", juce::dontSendNotification);
    autotuneScaleLabel.setJustificationType(juce::Justification::centred);
    autotuneScaleLabel.setColour(juce::Label::textColourId, ThemeColours::NeonCyan);

    setupSlider(autotuneAmountSlider, autotuneAmountLabel, "AMOUNT", "NeonCyan");
    autotuneAmountSlider.setRange(0.0, 100.0, 1.0);
    autotuneAmountSlider.setValue(100.0);
    autotuneAmountSlider.textFromValueFunction = [](double value) {
        return juce::String(static_cast<int>(value)) + "%";
    };
    autotuneAmountSlider.onValueChange = [this]() {
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&autotuneAmountSlider))
                autotuneAmountSlider.setValue(lastSliderValues[&autotuneAmountSlider], juce::dontSendNotification);
            return;
        }
        looper.setTrackAutotuneAmount(currentTrackId, (float)autotuneAmountSlider.getValue() / 100.0f);
    };

    setupSlider(autotuneSpeedSlider, autotuneSpeedLabel, "SPEED", "NeonCyan");
    autotuneSpeedSlider.setRange(0.0, 100.0, 1.0);
    autotuneSpeedSlider.setValue(10.0);
    autotuneSpeedSlider.textFromValueFunction = [](double value) {
        return juce::String(static_cast<int>(value)) + "%";
    };
    autotuneSpeedSlider.onValueChange = [this]() {
        if (midiManager && midiManager->isLearnModeActive()) {
            if (lastSliderValues.count(&autotuneSpeedSlider))
                autotuneSpeedSlider.setValue(lastSliderValues[&autotuneSpeedSlider], juce::dontSendNotification);
            return;
        }
        looper.setTrackAutotuneSpeed(currentTrackId, (float)autotuneSpeedSlider.getValue() / 100.0f);
    };

    // 初期状態のUI更新
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
    flangerRateSlider.setLookAndFeel(nullptr);
    flangerDepthSlider.setLookAndFeel(nullptr);
    flangerFeedbackSlider.setLookAndFeel(nullptr);
    chorusRateSlider.setLookAndFeel(nullptr);
    chorusDepthSlider.setLookAndFeel(nullptr);
    chorusMixSlider.setLookAndFeel(nullptr);
    tremoloRateSlider.setLookAndFeel(nullptr);
    tremoloDepthSlider.setLookAndFeel(nullptr);
    slicerRateSlider.setLookAndFeel(nullptr);
    slicerDepthSlider.setLookAndFeel(nullptr);
    slicerDutySlider.setLookAndFeel(nullptr);
    bitcrusherBitsSlider.setLookAndFeel(nullptr);
    bitcrusherRateSlider.setLookAndFeel(nullptr);
    granSizeSlider.setLookAndFeel(nullptr);
    granDenseSlider.setLookAndFeel(nullptr);
    granPitchSlider.setLookAndFeel(nullptr);
    granJitterSlider.setLookAndFeel(nullptr);
    granMixSlider.setLookAndFeel(nullptr);
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
            case EffectType::Flanger: typeStr = "Flanger"; break;
            case EffectType::Chorus: typeStr = "Chorus"; break;
            case EffectType::Tremolo: typeStr = "Tremolo"; break;
            case EffectType::Slicer: typeStr = "Slicer"; break;
            case EffectType::BeatRepeat: typeStr = "Repeat"; break;
            case EffectType::Bitcrusher: typeStr = "Bitcrush"; break;
            case EffectType::GranularCloud: typeStr = "Cloud"; break;
            case EffectType::Autotune: typeStr = "Autotune"; break;
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
    hide(flangerRateSlider); hide(flangerRateLabel);
    hide(flangerDepthSlider); hide(flangerDepthLabel);
    hide(flangerFeedbackSlider); hide(flangerFeedbackLabel);
    hide(flangerSyncButton);
    hide(chorusRateSlider); hide(chorusRateLabel);
    hide(chorusDepthSlider); hide(chorusDepthLabel);
    hide(chorusMixSlider); hide(chorusMixLabel);
    hide(chorusSyncButton);
    hide(tremoloRateSlider); hide(tremoloRateLabel);
    hide(tremoloDepthSlider); hide(tremoloDepthLabel);
    hide(tremoloShapeButton);
    hide(tremoloSyncButton);
    hide(slicerRateSlider); hide(slicerRateLabel);
    hide(slicerDepthSlider); hide(slicerDepthLabel);
    hide(slicerDutySlider); hide(slicerDutyLabel);
    hide(slicerShapeButton);
    hide(slicerSyncButton);
    hide(bitcrusherBitsSlider); hide(bitcrusherBitsLabel);
    hide(bitcrusherRateSlider); hide(bitcrusherRateLabel);
    hide(granSizeSlider); hide(granSizeLabel);
    hide(granDenseSlider); hide(granDenseLabel);
    hide(granPitchSlider); hide(granPitchLabel);
    hide(granJitterSlider); hide(granJitterLabel);
    hide(granMixSlider); hide(granMixLabel);
    hide(autotuneKeyCombo); hide(autotuneKeyLabel);
    hide(autotuneScaleCombo); hide(autotuneScaleLabel);
    hide(autotuneAmountSlider); hide(autotuneAmountLabel);
    hide(autotuneSpeedSlider); hide(autotuneSpeedLabel);

    if (type == EffectType::Filter) {
        filterSlider.setVisible(true); filterLabel.setVisible(true);
        filterResSlider.setVisible(true); filterResLabel.setVisible(true);
        filterTypeButton.setVisible(true);
    }
    // ... (rest of the visibility logic) ...
    
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
            repeatDivSlider.setVisible(true); repeatDivLabel.setVisible(true);
            repeatThreshSlider.setVisible(true); repeatThreshLabel.setVisible(true);
            repeatActiveButton.setVisible(true);
            break;
        case EffectType::Flanger:
            flangerRateSlider.setVisible(true); flangerRateLabel.setVisible(true);
            flangerDepthSlider.setVisible(true); flangerDepthLabel.setVisible(true);
            flangerFeedbackSlider.setVisible(true); flangerFeedbackLabel.setVisible(true);
            flangerSyncButton.setVisible(true);
            flangerSyncButton.setToggleState(slots[selectedSlotIndex].flangerSync, juce::dontSendNotification);
            flangerRateSlider.setEnabled(!flangerSyncButton.getToggleState());
            break;
        case EffectType::Chorus:
            chorusRateSlider.setVisible(true); chorusRateLabel.setVisible(true);
            chorusDepthSlider.setVisible(true); chorusDepthLabel.setVisible(true);
            chorusMixSlider.setVisible(true); chorusMixLabel.setVisible(true);
            chorusSyncButton.setVisible(true);
            chorusSyncButton.setToggleState(slots[selectedSlotIndex].chorusSync, juce::dontSendNotification);
            chorusRateSlider.setEnabled(!chorusSyncButton.getToggleState());
            break;
        case EffectType::Tremolo:
            tremoloRateSlider.setVisible(true); tremoloRateLabel.setVisible(true);
            tremoloDepthSlider.setVisible(true); tremoloDepthLabel.setVisible(true);
            tremoloShapeButton.setVisible(true);
            tremoloSyncButton.setVisible(true);
            tremoloSyncButton.setToggleState(slots[selectedSlotIndex].tremoloSync, juce::dontSendNotification);
            tremoloRateSlider.setEnabled(!tremoloSyncButton.getToggleState());
            break;
        case EffectType::Slicer:
            slicerRateSlider.setVisible(true); slicerRateLabel.setVisible(true);
            slicerDepthSlider.setVisible(true); slicerDepthLabel.setVisible(true);
            slicerDutySlider.setVisible(true); slicerDutyLabel.setVisible(true);
            slicerShapeButton.setVisible(true);
            slicerSyncButton.setVisible(true);
            slicerSyncButton.setToggleState(slots[selectedSlotIndex].slicerSync, juce::dontSendNotification);
            slicerRateSlider.setEnabled(!slicerSyncButton.getToggleState());
            break;
        case EffectType::Bitcrusher:
            bitcrusherBitsSlider.setVisible(true); bitcrusherBitsLabel.setVisible(true);
            bitcrusherRateSlider.setVisible(true); bitcrusherRateLabel.setVisible(true);
            break;
        case EffectType::GranularCloud:
            granSizeSlider.setVisible(true); granSizeLabel.setVisible(true);
            granDenseSlider.setVisible(true); granDenseLabel.setVisible(true);
            granPitchSlider.setVisible(true); granPitchLabel.setVisible(true);
            granJitterSlider.setVisible(true); granJitterLabel.setVisible(true);
            granMixSlider.setVisible(true); granMixLabel.setVisible(true);
            break;
        case EffectType::Autotune:
            autotuneKeyCombo.setVisible(true); autotuneKeyLabel.setVisible(true);
            autotuneScaleCombo.setVisible(true); autotuneScaleLabel.setVisible(true);
            autotuneAmountSlider.setVisible(true); autotuneAmountLabel.setVisible(true);
            autotuneSpeedSlider.setVisible(true); autotuneSpeedLabel.setVisible(true);
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

    // FLANGER
    if(flangerRateSlider.isVisible()) {
        placeControls({ {&flangerRateSlider, &flangerRateLabel}, {&flangerDepthSlider, &flangerDepthLabel}, {&flangerFeedbackSlider, &flangerFeedbackLabel} }, &flangerSyncButton);
    }

    // CHORUS
    if(chorusRateSlider.isVisible()) {
        placeControls({ {&chorusRateSlider, &chorusRateLabel}, {&chorusDepthSlider, &chorusDepthLabel}, {&chorusMixSlider, &chorusMixLabel} }, &chorusSyncButton);
    }

    // TREMOLO
    if(tremoloRateSlider.isVisible()) {
        placeControls({ {&tremoloRateSlider, &tremoloRateLabel}, {&tremoloDepthSlider, &tremoloDepthLabel} }, &tremoloShapeButton);
        // Position sync button below shape button or right of it
        auto b = tremoloShapeButton.getBounds();
        tremoloSyncButton.setBounds(b.getX(), b.getBottom() + 5, b.getWidth(), 30);
    }

    // SLICER
    if(slicerRateSlider.isVisible()) {
        placeControls({ {&slicerRateSlider, &slicerRateLabel}, {&slicerDepthSlider, &slicerDepthLabel}, {&slicerDutySlider, &slicerDutyLabel} }, &slicerShapeButton);
        // Position sync button below shape button
        auto b = slicerShapeButton.getBounds();
        slicerSyncButton.setBounds(b.getX(), b.getBottom() + 5, b.getWidth(), 30);
    }

    // BITCRUSHER
    if(bitcrusherBitsSlider.isVisible()) {
        placeControls({ {&bitcrusherBitsSlider, &bitcrusherBitsLabel}, {&bitcrusherRateSlider, &bitcrusherRateLabel} });
    }

    // GRANULAR CLOUD
    if(granSizeSlider.isVisible()) {
        placeControls({
            {&granSizeSlider, &granSizeLabel},
            {&granDenseSlider, &granDenseLabel},
            {&granPitchSlider, &granPitchLabel},
            {&granJitterSlider, &granJitterLabel},
            {&granMixSlider, &granMixLabel}
        });
    }

    // AUTOTUNE
    if(autotuneKeyCombo.isVisible()) {
        // Layout: [KEY ComboBox] [SCALE ComboBox] [AMOUNT Knob] [SPEED Knob]
        int comboWidth = 80;
        int comboHeight = 28;
        int totalWidth = (comboWidth * 2) + (knobSize * 2) + (spacing * 3);
        int currentX = rightArea.getCentreX() - (totalWidth / 2);

        // Key ComboBox
        autotuneKeyLabel.setBounds(currentX, startY, comboWidth, labelHeight);
        autotuneKeyCombo.setBounds(currentX, startY + labelHeight + 2, comboWidth, comboHeight);

        currentX += comboWidth + spacing;

        // Scale ComboBox
        autotuneScaleLabel.setBounds(currentX, startY, comboWidth, labelHeight);
        autotuneScaleCombo.setBounds(currentX, startY + labelHeight + 2, comboWidth, comboHeight);

        currentX += comboWidth + spacing;

        // Amount Slider
        autotuneAmountSlider.setBounds(currentX, startY, knobSize, knobSize + textBoxHeight);
        autotuneAmountLabel.setBounds(currentX, startY + knobSize + textBoxHeight + 2, knobSize, labelHeight);

        currentX += knobSize + spacing;

        // Speed Slider
        autotuneSpeedSlider.setBounds(currentX, startY, knobSize, knobSize + textBoxHeight);
        autotuneSpeedLabel.setBounds(currentX, startY + knobSize + textBoxHeight + 2, knobSize, labelHeight);
    }
}

void FXPanel::mouseDown(const juce::MouseEvent& e)
{
    // MIDI Learnモード時はコンポーネント選択を優先
    if (midiManager != nullptr && midiManager->isLearnModeActive())
    {
        juce::String controlId = getControlIdForComponent(e.eventComponent);
        if (!controlId.isEmpty())
        {
            midiManager->setLearnTarget(controlId);
            DBG("MIDI Learn: Waiting for input - " << controlId);
            repaint();
            return; // 以降の通常クリック処理（スロット選択等）をスキップ
        }
    }

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
    m.addItem(5, "Flanger", true, slots[slotIndex].type == EffectType::Flanger);
    m.addItem(6, "Chorus", true, slots[slotIndex].type == EffectType::Chorus);
    m.addItem(7, "Tremolo", true, slots[slotIndex].type == EffectType::Tremolo);
    m.addItem(8, "Slicer", true, slots[slotIndex].type == EffectType::Slicer);
    m.addItem(9, "Bitcrusher", true, slots[slotIndex].type == EffectType::Bitcrusher);
    m.addItem(10, "Beat Repeat", true, slots[slotIndex].type == EffectType::BeatRepeat);
    m.addItem(11, "Granular Cloud", true, slots[slotIndex].type == EffectType::GranularCloud);
    m.addItem(12, "Autotune", true, slots[slotIndex].type == EffectType::Autotune);
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
        else if (result == 5) newType = EffectType::Flanger;
        else if (result == 6) newType = EffectType::Chorus;
        else if (result == 7) newType = EffectType::Tremolo;
        else if (result == 8) newType = EffectType::Slicer;
        else if (result == 9) newType = EffectType::Bitcrusher;
        else if (result == 10) newType = EffectType::BeatRepeat;
        else if (result == 11) newType = EffectType::GranularCloud;
        else if (result == 12) newType = EffectType::Autotune;

        // Disable old effect
        if (oldType != newType && currentTrackId >= 0)
        {
            switch (oldType) {
                case EffectType::Filter: looper.setTrackFilterEnabled(currentTrackId, false); break;
                case EffectType::Delay: looper.setTrackDelayEnabled(currentTrackId, false); break;
                case EffectType::Reverb: looper.setTrackReverbEnabled(currentTrackId, false); break;
                case EffectType::Flanger: looper.setTrackFlangerEnabled(currentTrackId, false); break;
                case EffectType::Chorus: looper.setTrackChorusEnabled(currentTrackId, false); break;
                case EffectType::Tremolo: looper.setTrackTremoloEnabled(currentTrackId, false); break;
                case EffectType::Slicer: looper.setTrackSlicerEnabled(currentTrackId, false); break;
                case EffectType::Bitcrusher: looper.setTrackBitcrusherEnabled(currentTrackId, false); break;
                case EffectType::BeatRepeat: looper.setTrackBeatRepeatActive(currentTrackId, false); break;
                case EffectType::GranularCloud: looper.setTrackGranularEnabled(currentTrackId, false); break;
                case EffectType::Autotune: looper.setTrackAutotuneEnabled(currentTrackId, false); break;
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
            case EffectType::Flanger: typeStr = "Flanger"; break;
            case EffectType::Chorus: typeStr = "Chorus"; break;
            case EffectType::Tremolo: typeStr = "Tremolo"; break;
            case EffectType::Slicer: typeStr = "Slicer"; break;
            case EffectType::Bitcrusher: typeStr = "Bitcrush"; break;
            case EffectType::BeatRepeat: typeStr = "Repeat"; break;
            case EffectType::GranularCloud: typeStr = "Cloud"; break;
            case EffectType::Autotune: typeStr = "Autotune"; break;
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
                case EffectType::Flanger: looper.setTrackFlangerEnabled(currentTrackId, true); break;
                case EffectType::Chorus: looper.setTrackChorusEnabled(currentTrackId, true); break;
                case EffectType::Tremolo: looper.setTrackTremoloEnabled(currentTrackId, true); break;
                case EffectType::Slicer: looper.setTrackSlicerEnabled(currentTrackId, true); break;
                case EffectType::Bitcrusher: looper.setTrackBitcrusherEnabled(currentTrackId, true); break;
                case EffectType::BeatRepeat: looper.setTrackBeatRepeatActive(currentTrackId, true); break;
                case EffectType::GranularCloud: looper.setTrackGranularEnabled(currentTrackId, true); break;
                case EffectType::Autotune: looper.setTrackAutotuneEnabled(currentTrackId, true); break;
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
    
    if (slider == &flangerRateSlider)     return "fx_flanger_rate";
    if (slider == &flangerDepthSlider)    return "fx_flanger_depth";
    if (slider == &flangerFeedbackSlider) return "fx_flanger_feedback";
    
    if (slider == &chorusRateSlider)     return "fx_chorus_rate";
    if (slider == &chorusDepthSlider)    return "fx_chorus_depth";
    if (slider == &chorusMixSlider)      return "fx_chorus_mix";
    
    if (slider == &tremoloRateSlider)     return "fx_tremolo_rate";
    if (slider == &tremoloDepthSlider)    return "fx_tremolo_depth";

    if (slider == &slicerRateSlider)      return "fx_slicer_rate";
    if (slider == &slicerDepthSlider)     return "fx_slicer_depth";
    if (slider == &slicerDutySlider)      return "fx_slicer_duty";

    if (slider == &bitcrusherBitsSlider)  return "fx_bit_depth";
    if (slider == &bitcrusherRateSlider)  return "fx_bit_rate";
    
    if (slider == &granSizeSlider)        return "fx_gran_size";
    if (slider == &granDenseSlider)       return "fx_gran_dense";
    if (slider == &granPitchSlider)       return "fx_gran_pitch";
    if (slider == &granJitterSlider)      return "fx_gran_jitter";
    if (slider == &granMixSlider)         return "fx_gran_mix";
    return "";
}

juce::String FXPanel::getControlIdForComponent(juce::Component* comp)
{
    if (auto* s = dynamic_cast<juce::Slider*>(comp)) return getControlIdForSlider(s);
    
    if (comp == &flangerSyncButton) return "fx_flanger_sync";
    if (comp == &chorusSyncButton)  return "fx_chorus_sync";
    if (comp == &tremoloSyncButton) return "fx_tremolo_sync";
    if (comp == &tremoloShapeButton) return "fx_tremolo_shape";
    if (comp == &slicerSyncButton) return "fx_slicer_sync";
    if (comp == &slicerShapeButton) return "fx_slicer_shape";
    if (comp == &filterTypeButton)   return "fx_filter_type";
    if (comp == &repeatActiveButton) return "fx_repeat_active";
    
    for (int i = 0; i < 4; ++i)
        if (comp == &bypassButtons[i]) return "fx_slot" + juce::String(i + 1) + "_bypass";

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

    if (controlId == "fx_flanger_rate")       return &flangerRateSlider;
    if (controlId == "fx_flanger_depth")      return &flangerDepthSlider;
    if (controlId == "fx_flanger_feedback")   return &flangerFeedbackSlider;
    
    if (controlId == "fx_chorus_rate")       return &chorusRateSlider;
    if (controlId == "fx_chorus_depth")      return &chorusDepthSlider;
    if (controlId == "fx_chorus_mix")        return &chorusMixSlider;
    
    if (controlId == "fx_tremolo_rate")       return &tremoloRateSlider;
    if (controlId == "fx_tremolo_depth")      return &tremoloDepthSlider;

    if (controlId == "fx_slicer_rate")        return &slicerRateSlider;
    if (controlId == "fx_slicer_depth")       return &slicerDepthSlider;
    if (controlId == "fx_slicer_duty")        return &slicerDutySlider;

    if (controlId == "fx_bit_depth")          return &bitcrusherBitsSlider;
    if (controlId == "fx_bit_rate")           return &bitcrusherRateSlider;
    
    if (controlId == "fx_gran_size")          return &granSizeSlider;
    if (controlId == "fx_gran_dense")         return &granDenseSlider;
    if (controlId == "fx_gran_pitch")         return &granPitchSlider;
    if (controlId == "fx_gran_jitter")        return &granJitterSlider;
    if (controlId == "fx_gran_mix")           return &granMixSlider;
    
    return nullptr;
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
        &repeatDivSlider, &repeatThreshSlider,
        &flangerRateSlider, &flangerDepthSlider, &flangerFeedbackSlider,
        &chorusRateSlider, &chorusDepthSlider, &chorusMixSlider,
        &tremoloRateSlider, &tremoloDepthSlider,
        &slicerRateSlider, &slicerDepthSlider, &slicerDutySlider,
        &bitcrusherBitsSlider, &bitcrusherRateSlider,
        &granSizeSlider, &granDenseSlider, &granPitchSlider, &granJitterSlider, &granMixSlider
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
    auto drawButtonOverlay = [&](juce::Component& btn, const juce::String& ctrlId) {
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
    drawButtonOverlay(tremoloShapeButton, "fx_tremolo_shape");
    drawButtonOverlay(slicerShapeButton, "fx_slicer_shape");
    drawButtonOverlay(flangerSyncButton, "fx_flanger_sync");
    drawButtonOverlay(chorusSyncButton, "fx_chorus_sync");
    drawButtonOverlay(tremoloSyncButton, "fx_tremolo_sync");
    drawButtonOverlay(slicerSyncButton, "fx_slicer_sync");
    
    // FXスロットバイパスボタン
    for (int i = 0; i < 4; ++i)
    {
        juce::String ctrlId = "fx_slot" + juce::String(i + 1) + "_bypass";
        drawButtonOverlay(bypassButtons[i], ctrlId);
    }
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
            repeatActiveButton.onClick();
        }
        else if (controlId == "fx_flanger_sync")
        {
            flangerSyncButton.setToggleState(value > 0.5f, juce::sendNotificationSync);
            flangerSyncButton.onClick();
        }
        else if (controlId == "fx_chorus_sync")
        {
            chorusSyncButton.setToggleState(value > 0.5f, juce::sendNotificationSync);
            chorusSyncButton.onClick();
        }
        else if (controlId == "fx_tremolo_sync")
        {
            tremoloSyncButton.setToggleState(value > 0.5f, juce::sendNotificationSync);
            tremoloSyncButton.onClick();
        }
        else if (controlId == "fx_slicer_sync")
        {
            slicerSyncButton.setToggleState(value > 0.5f, juce::sendNotificationSync);
            slicerSyncButton.onClick();
        }
        else if (controlId == "fx_slicer_shape")
        {
            // シェイプサイクル (0=Square, 1=Smooth)
            slicerShapeButton.onClick();
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


void FXPanel::sliderValueChanged(juce::Slider* slider)
{
    // 何もしない (既存のonValueChangeコールバックで処理されている)
}

void FXPanel::sliderDragStarted(juce::Slider* slider)
{
    // MIDI Learnモード時はスライダーの操作をマッピング登録に変える
    if (midiManager != nullptr && midiManager->isLearnModeActive())
    {
        // 現在値を保存（onValueChangeで復元するため）
        lastSliderValues[slider] = slider->getValue();
        
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
        case EffectType::Flanger:
            looper.setTrackFlangerEnabled(trackId, isActive);
            break;
        case EffectType::Chorus:
            looper.setTrackChorusEnabled(trackId, isActive);
            break;
        case EffectType::Tremolo:
            looper.setTrackTremoloEnabled(trackId, isActive);
            break;
        case EffectType::Slicer:
            looper.setTrackSlicerEnabled(trackId, isActive);
            break;
        case EffectType::Bitcrusher:
            looper.setTrackBitcrusherEnabled(trackId, isActive);
            break;
        case EffectType::GranularCloud:
            looper.setTrackGranularEnabled(trackId, isActive);
            break;
        case EffectType::BeatRepeat:
            looper.setTrackBeatRepeatActive(trackId, isActive);
            break;
        case EffectType::Autotune:
            looper.setTrackAutotuneEnabled(trackId, isActive);
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
