/*
  ==============================================================================

    FXPanel.cpp
    Created: 31 Dec 2025
    Author:  mt sh

  ==============================================================================
*/

#include "FXPanel.h"

FXPanel::FXPanel(LooperAudio& looperRef) : looper(looperRef)
{
    addAndMakeVisible(titleLabel);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, ThemeColours::NeonCyan);
    titleLabel.setText("Track FX Rack", juce::dontSendNotification);

    // --- FILTER ---
    setupSlider(filterSlider, filterLabel, "CUTOFF", "IceBlue");
    filterSlider.setRange(20.0, 20000.0, 1.0);
    filterSlider.setSkewFactorFromMidPoint(1000.0);
    filterSlider.setValue(20000.0);
    filterSlider.onValueChange = [this]() {
        looper.setTrackFilterCutoff(currentTrackId, (float)filterSlider.getValue());
    };
    
    setupSlider(filterResSlider, filterResLabel, "RES", "IceBlue"); 
    filterResSlider.setRange(0.1, 10.0, 0.1);
    filterResSlider.setValue(0.707);
    filterResSlider.onValueChange = [this]() {
        looper.setTrackFilterResonance(currentTrackId, (float)filterResSlider.getValue());
    };
    
    addChildComponent(filterTypeButton);
    filterTypeButton.setClickingTogglesState(true);
    filterTypeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    filterTypeButton.setColour(juce::TextButton::buttonOnColourId, ThemeColours::NeonCyan);
    filterTypeButton.onClick = [this]() {
        bool isHPF = filterTypeButton.getToggleState();
        filterTypeButton.setButtonText(isHPF ? "HPF" : "LPF");
        looper.setTrackFilterType(currentTrackId, isHPF ? 1 : 0);
    };

    // --- COMP ---
    setupSlider(compSlider, compLabel, "AMOUNT", "MagmaRed");
    compSlider.setRange(0.0, 1.0, 0.01);
    compSlider.setValue(0.0);
    compSlider.onValueChange = [this]() {
        float amt = (float)compSlider.getValue();
        float thresh = -40.0f * amt;
        float ratio = 1.0f + (7.0f * amt);
        looper.setTrackCompressor(currentTrackId, thresh, ratio);
    };

    // --- DELAY ---
    setupSlider(delaySlider, delayLabel, "TIME", "BlackHole");
    delaySlider.setRange(0.0, 1.0, 0.01);
    delaySlider.setValue(0.5);
    delaySlider.onValueChange = [this]() {
        looper.setTrackDelayMix(currentTrackId, (float)delayMixSlider.getValue(), (float)delaySlider.getValue());
    };
    
    setupSlider(delayFeedbackSlider, delayFeedbackLabel, "F.BACK", "BlackHole");
    delayFeedbackSlider.setRange(0.0, 0.95, 0.01);
    delayFeedbackSlider.setValue(0.0);
    delayFeedbackSlider.onValueChange = [this]() {
        looper.setTrackDelayFeedback(currentTrackId, (float)delayFeedbackSlider.getValue());
    };
    
    setupSlider(delayMixSlider, delayMixLabel, "MIX", "BlackHole");
    delayMixSlider.setRange(0.0, 0.8, 0.01); 
    delayMixSlider.setValue(0.0);
    delayMixSlider.onValueChange = [this]() {
        looper.setTrackDelayMix(currentTrackId, (float)delayMixSlider.getValue(), (float)delaySlider.getValue()); 
    };

    // --- REVERB ---
    setupSlider(reverbSlider, reverbLabel, "MIX", "GasGiant");
    reverbSlider.setRange(0.0, 1.0, 0.01);
    reverbSlider.setValue(0.0);
    reverbSlider.onValueChange = [this]() {
        looper.setTrackReverbMix(currentTrackId, (float)reverbSlider.getValue());
    };
    
    setupSlider(reverbDecaySlider, reverbDecayLabel, "DECAY", "GasGiant");
    reverbDecaySlider.setRange(0.0, 1.0, 0.01);
    reverbDecaySlider.setValue(0.5);
    reverbDecaySlider.onValueChange = [this]() {
        looper.setTrackReverbRoomSize(currentTrackId, (float)reverbDecaySlider.getValue());
    };
    
    updateSliderVisibility();
}

FXPanel::~FXPanel()
{
    filterSlider.setLookAndFeel(nullptr);
    filterResSlider.setLookAndFeel(nullptr);
    compSlider.setLookAndFeel(nullptr);
    delaySlider.setLookAndFeel(nullptr);
    delayFeedbackSlider.setLookAndFeel(nullptr);
    delayMixSlider.setLookAndFeel(nullptr);
    reverbSlider.setLookAndFeel(nullptr);
    reverbDecaySlider.setLookAndFeel(nullptr);
}

void FXPanel::setupSlider(juce::Slider& slider, juce::Label& label, const juce::String& name, const juce::String& style)
{
    addChildComponent(slider);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setLookAndFeel(&planetLnF);
    slider.getProperties().set("PlanetStyle", style);

    addChildComponent(label);
    label.setText(name, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, ThemeColours::Silver);
    label.setFont(juce::FontOptions(14.0f));
}

void FXPanel::setTargetTrackId(int trackId)
{
    currentTrackId = trackId;
    // titleLabel.setText("Track " + juce::String(trackId) + " FX", juce::dontSendNotification);
    // No longer updating title per track
    repaint();
}

void FXPanel::updateSliderVisibility()
{
    // Hide all
    auto hide = [&](juce::Component& c) { c.setVisible(false); };
    hide(filterSlider); hide(filterLabel);
    hide(filterResSlider); hide(filterResLabel);
    hide(filterTypeButton);
    
    hide(compSlider); hide(compLabel);
    
    hide(delaySlider); hide(delayLabel);
    hide(delayFeedbackSlider); hide(delayFeedbackLabel);
    hide(delayMixSlider); hide(delayMixLabel);
    
    hide(reverbSlider); hide(reverbLabel);
    hide(reverbDecaySlider); hide(reverbDecayLabel);

    if (selectedSlotIndex < 0 || selectedSlotIndex >= 4) return;
    
    EffectSlot& slot = slots[selectedSlotIndex];
    
    switch (slot.type)
    {
        case EffectType::Filter:
            filterSlider.setVisible(true); filterLabel.setVisible(true);
            filterResSlider.setVisible(true); filterResLabel.setVisible(true);
            filterTypeButton.setVisible(true);
            break;
        case EffectType::Compressor:
            compSlider.setVisible(true); compLabel.setVisible(true);
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
        default:
            break;
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
    if (fullArea.getWidth() <= 10 || fullArea.getHeight() <= 40) return; // Prevent negative sizes

    fullArea.removeFromTop(40); // Title
    
    auto leftCol = fullArea.removeFromLeft(fullArea.getWidth() * 0.25f);
    auto rightArea = fullArea;
    rightArea.removeFromLeft(10); // Spacer
    
    // Draw Divider
    g.setColour(ThemeColours::Silver.withAlpha(0.3f));
    g.drawVerticalLine(leftCol.getRight() + 5, (float)fullArea.getY(), (float)fullArea.getBottom());
    
    // Draw Slots in Left Column
    int slotHeight = 60;
    int slotSpacing = 10;
    
    for(int i=0; i<4; ++i)
    {
        auto slotRect = leftCol.removeFromTop(slotHeight);
        
        // Ensure valid rectangle for rounded rect
        if (slotRect.getWidth() < 4) continue;
        auto reducedRect = slotRect.reduced(2, 0);

        leftCol.removeFromTop(slotSpacing);
        
        bool isSelected = (i == selectedSlotIndex);
        
        // Slot Box
        g.setColour(isSelected ? ThemeColours::NeonMagenta.withAlpha(0.3f) : juce::Colours::darkgrey.withAlpha(0.3f));
        g.fillRoundedRectangle(reducedRect.toFloat(), 5.0f);
        
        g.setColour(isSelected ? ThemeColours::NeonMagenta : ThemeColours::Silver.withAlpha(0.5f));
        g.drawRoundedRectangle(reducedRect.toFloat(), 5.0f, isSelected ? 2.0f : 1.0f);
        
        // Text
        juce::String typeStr = "";
        EffectSlot& slot = slots[i];
        
        switch(slot.type) {
            case EffectType::Filter: typeStr = "Filter"; break;
            case EffectType::Compressor: typeStr = "Comp"; break;
            case EffectType::Delay: typeStr = "Delay"; break;
            case EffectType::Reverb: typeStr = "Reverb"; break;
            default: typeStr = "Empty"; break;
        }
        
        g.setColour(juce::Colours::white);
        // g.setFont(juce::Font(16.0f)); // Deprecated
        g.setFont(juce::FontOptions(16.0f));
        g.drawText(typeStr, slotRect, juce::Justification::centred, true);
    }
    
    // Hint
    if(slots[selectedSlotIndex].type == EffectType::None)
    {
        g.setColour(juce::Colours::grey);
        // g.setFont(18.0f); // Deprecated implicit float
        g.setFont(juce::FontOptions(18.0f));
        g.drawText("No Effect Selected\nClick slot to assign", rightArea, juce::Justification::centred, true);
    }
}

void FXPanel::resized()
{
    auto area = getLocalBounds();
    titleLabel.setBounds(area.removeFromTop(40));
    
    area.reduce(10, 10);
    int dividerX = area.getX() + (area.getWidth() * 0.25f);
    
    auto rightArea = area;
    rightArea.setLeft(dividerX + 20); // Add margin
    
    // Uniform Knob Layout
    int knobSize = 110; // Standard size for all
    int labelHeight = 20;
    int spacing = 30;
    int startX = rightArea.getCentreX(); // We will center the group based on count
    int startY = rightArea.getCentreY() - (knobSize / 2);

    // Helper to place a list of sliders centered
    auto placeControls = [&](std::vector<std::pair<juce::Slider*, juce::Label*>> controls, juce::Component* extra = nullptr) 
    {
        int totalWidth = (controls.size() * knobSize) + ((controls.size() - 1) * spacing);
        int currentX = rightArea.getCentreX() - (totalWidth / 2);
        
        for(auto& p : controls)
        {
            auto* slider = p.first;
            auto* label = p.second;
            
            slider->setBounds(currentX, startY, knobSize, knobSize);
            label->setBounds(currentX, startY + knobSize + 5, knobSize, labelHeight);
            
            currentX += knobSize + spacing;
        }
        
        if (extra)
        {
            // Place extra component (like button) below the middle or first knob
            int extraH = 40;
            extra->setBounds(rightArea.getCentreX() - 40, startY + knobSize + labelHeight + 20, 80, extraH);
        }
    };

    // FILTER
    if(filterSlider.isVisible()) {
        placeControls({ {&filterSlider, &filterLabel}, {&filterResSlider, &filterResLabel} }, &filterTypeButton);
    }
    
    // COMP
    if(compSlider.isVisible()) {
        placeControls({ {&compSlider, &compLabel} });
    }
    
    // DELAY
    if(delaySlider.isVisible()) {
        placeControls({ {&delaySlider, &delayLabel}, {&delayFeedbackSlider, &delayFeedbackLabel}, {&delayMixSlider, &delayMixLabel} });
    }
    
    // REVERB
    if(reverbSlider.isVisible()) {
        placeControls({ {&reverbSlider, &reverbLabel}, {&reverbDecaySlider, &reverbDecayLabel} });
    }
}

void FXPanel::mouseDown(const juce::MouseEvent& e)
{
    auto area = getLocalBounds().reduced(10);
    area.removeFromTop(40);
    
    auto leftCol = area.removeFromLeft(area.getWidth() * 0.25f);
    int slotHeight = 60;
    int slotSpacing = 10;
    
    for(int i=0; i<4; ++i)
    {
        auto slotRect = leftCol.removeFromTop(slotHeight);
        leftCol.removeFromTop(slotSpacing);
        
        if(slotRect.contains(e.getPosition()))
        {
            selectedSlotIndex = i;
            updateSliderVisibility();
            repaint();
            
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
    m.addSeparator();
    m.addItem(99, "Clear", true, false);

    m.showMenuAsync(juce::PopupMenu::Options(), [this, slotIndex](int result)
    {
        if (result == 0) return;
        
        if (result == 99) slots[slotIndex].type = EffectType::None;
        else if (result == 1) slots[slotIndex].type = EffectType::Filter;
        else if (result == 2) slots[slotIndex].type = EffectType::Compressor;
        else if (result == 3) slots[slotIndex].type = EffectType::Delay;
        else if (result == 4) slots[slotIndex].type = EffectType::Reverb;
        
        updateSliderVisibility();
        repaint();
    });
}
