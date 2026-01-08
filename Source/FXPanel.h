/*
  ==============================================================================

    FXPanel.h
    Created: 31 Dec 2025
    Author:  mt sh

  ==============================================================================
*/

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "ThemeColours.h"
#include "PlanetKnobLookAndFeel.h"
#include "LooperAudio.h"
#include "FXSlotButtonLookAndFeel.h"
#include "FilterSpectrumVisualizer.h"

class MidiLearnManager; // 前方宣言

// =====================================================
// 🎛 FXPanel クラス宣言
// =====================================================
class FXPanel: public juce::Component, 
               public juce::Timer,
               public juce::Slider::Listener
{
public:
    enum class EffectType {
        None,
        Filter,
        Compressor,
        Delay,
        Reverb,
        BeatRepeat
    };

    struct EffectSlot {
        EffectType type = EffectType::None;
        bool isBypassed = false;
    };

    FXPanel(LooperAudio& looperRef);
    ~FXPanel() override;

    void setTargetTrackId(int trackId);
    
    // トラック選択時のコールバック
    std::function<void(int)> onTrackSelected;

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;
    
    void timerCallback() override;

    void mouseDown(const juce::MouseEvent& e) override;
    
    // MIDI Learn対応
    void setMidiLearnManager(MidiLearnManager* manager);
    void handleMidiControl(const juce::String& controlId, float value);
    juce::String getControlIdForSlider(juce::Slider* slider);
    juce::Slider* getSliderForControlId(const juce::String& controlId);
    
    // Slider::Listener
    void sliderValueChanged(juce::Slider* slider) override;
    void sliderDragStarted(juce::Slider* slider) override;

private:
    LooperAudio& looper;
    int currentTrackId = -1;
    juce::Label titleLabel;

    PlanetKnobLookAndFeel planetLnF;
    FXSlotButtonLookAndFeel slotLnF;

    // Slots
    EffectSlot slots[4];
    int selectedSlotIndex = 0;
    juce::TextButton slotButtons[4];  // エフェクトスロットボタン
    juce::TextButton bypassButtons[4]; // オン/オフ用の丸いボタン
    
    // トラック選択ボタン（8トラック）
    juce::TextButton trackButtons[8];
    
    // Sliders (All exist, visibility toggled)
    
    FilterSpectrumVisualizer visualizer;

    // Filter
    juce::Slider filterSlider;
    juce::Label filterLabel;
    juce::Slider filterResSlider; // Resonance
    juce::Label filterResLabel;
    juce::TextButton filterTypeButton { "LPF" }; // Toggle LPF/HPF
    
    // Compressor
    juce::Slider compThreshSlider;  // Threshold
    juce::Label compThreshLabel;
    juce::Slider compRatioSlider;   // Ratio
    juce::Label compRatioLabel;
    
    // Delay
    juce::Slider delaySlider; // Time
    juce::Label delayLabel;
    juce::Slider delayFeedbackSlider;
    juce::Label delayFeedbackLabel;
    juce::Slider delayMixSlider;
    juce::Label delayMixLabel;
    
    // Reverb
    juce::Slider reverbSlider; // Mix
    juce::Label reverbLabel;
    juce::Slider reverbDecaySlider; // RoomSize
    juce::Label reverbDecayLabel;
    
    // Beat Repeat
    juce::Slider repeatDivSlider;
    juce::Label repeatDivLabel;
    juce::Slider repeatThreshSlider;
    juce::Label repeatThreshLabel;
    juce::TextButton repeatActiveButton { "REPEAT OFF" };

    void setupSlider(juce::Slider& slider, juce::Label& label, const juce::String& name, const juce::String& style);
    void showEffectMenu(int slotIndex);
    void updateSliderVisibility();
    
    // MIDI Learn
    MidiLearnManager* midiManager = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FXPanel)
};
