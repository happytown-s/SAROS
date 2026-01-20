/*
  ==============================================================================

    AppSelectionPanel.h
    Created: 19 Jan 2026
    Author:  Antigravity

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SystemAudioCapturer.h"

class AppSelectionPanel : public juce::Component,
                          public juce::ListBoxModel,
                          private juce::Timer
{
public:
    AppSelectionPanel(SystemAudioCapturer& capturer);
    ~AppSelectionPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // ListBoxModel implementation
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked(int rowNumber, const juce::MouseEvent&) override;
    void backgroundClicked(const juce::MouseEvent&) override {}

    // Timer
    void timerCallback() override;

    // Refresh the list of apps
    void refreshAppList();

private:
    SystemAudioCapturer& systemCapturer;
    juce::ListBox appList;
    std::vector<ShareableApp> apps;
    
    juce::TextButton refreshButton { "Refresh" };
    juce::TextButton selectAllButton { "Select All" };
    juce::TextButton deselectAllButton { "Deselect All" };

    void updateCapturerFilter();
    void toggleSelection(int row);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AppSelectionPanel)
};
