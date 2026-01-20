/*
  ==============================================================================

    AppSelectionPanel.cpp
    Created: 19 Jan 2026
    Author:  Antigravity

  ==============================================================================
*/

#include "AppSelectionPanel.h"

AppSelectionPanel::AppSelectionPanel(SystemAudioCapturer& capturer)
    : systemCapturer(capturer)
{
    addAndMakeVisible(appList);
    appList.setModel(this);
    appList.setRowHeight(40);
    appList.setMultipleSelectionEnabled(false);
    
    addAndMakeVisible(refreshButton);
    refreshButton.onClick = [this] { refreshAppList(); };
    
    addAndMakeVisible(selectAllButton);
    selectAllButton.onClick = [this] {
        for (auto& app : apps) app.isSelected = true;
        updateCapturerFilter();
        appList.repaint();
    };

    addAndMakeVisible(deselectAllButton);
    deselectAllButton.onClick = [this] {
        for (auto& app : apps) app.isSelected = false;
        updateCapturerFilter();
        appList.repaint();
    };

    setSize(400, 520); // Slightly taller for meter
    refreshAppList();
    
    // Start timer for meter updates (30 FPS)
    startTimerHz(30);
}

AppSelectionPanel::~AppSelectionPanel()
{
    stopTimer();
    appList.setModel(nullptr);
}

void AppSelectionPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey.darker());
    
    // Audio Level Meter
    auto meterArea = getLocalBounds().removeFromTop(20).reduced(5);
    
    // Background
    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(meterArea.toFloat(), 3.0f);
    
    // Level bar
    float level = systemCapturer.getCurrentLevel();
    float barWidth = meterArea.getWidth() * std::min(level * 5.0f, 1.0f); // Scale x5 for visibility
    
    auto barArea = meterArea.withWidth((int)barWidth);
    if (level > 0.2f)
        g.setColour(juce::Colours::orange);
    else if (level > 0.01f)
        g.setColour(juce::Colours::green);
    else
        g.setColour(juce::Colours::darkgreen);
    
    g.fillRoundedRectangle(barArea.toFloat(), 3.0f);
    
    // Label
    g.setColour(juce::Colours::white);
    g.setFont(10.0f);
    g.drawText("SCK Level", meterArea, juce::Justification::centred, false);
}

void AppSelectionPanel::resized()
{
    auto area = getLocalBounds();
    
    // Audio level meter at very top
    auto meterArea = area.removeFromTop(20).reduced(5);
    // (meter drawing is done in paint())
    
    auto topArea = area.removeFromTop(40).reduced(5);
    
    refreshButton.setBounds(topArea.removeFromRight(80));
    deselectAllButton.setBounds(topArea.removeFromLeft(100));
    selectAllButton.setBounds(topArea.removeFromLeft(100));
    
    appList.setBounds(area);
}

int AppSelectionPanel::getNumRows()
{
    return (int)apps.size();
}

void AppSelectionPanel::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (!juce::isPositiveAndBelow(rowNumber, (int)apps.size())) return;

    const auto& app = apps[(size_t)rowNumber];

    if (rowIsSelected)
        g.fillAll(juce::Colours::lightblue.withAlpha(0.2f));

    auto area = juce::Rectangle<int>(0, 0, width, height).reduced(2);
    
    // Checkbox
    auto checkArea = area.removeFromLeft(height); // Square
    g.setColour(juce::Colours::white);
    g.drawRect(checkArea.reduced(10), 1.0f);
    if (app.isSelected)
    {
        g.setColour(juce::Colours::lightgreen);
        g.fillRect(checkArea.reduced(12));
    }
    
    // Icon
    if (app.icon.isValid())
    {
        auto iconArea = area.removeFromLeft(height);
        g.setOpacity(1.0f);
        g.drawImage(app.icon, iconArea.toFloat(), juce::RectanglePlacement::centred);
    }
    
    // Name
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    g.drawText(app.appName, area.removeFromLeft(width - 100).reduced(5), juce::Justification::centredLeft, true);
    
    // PID (Optional, maybe smaller)
    g.setColour(juce::Colours::grey);
    g.setFont(10.0f);
    g.drawText("PID: " + juce::String(app.processID), area, juce::Justification::centredRight, false);
}

void AppSelectionPanel::listBoxItemClicked(int rowNumber, const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return; // Right click ignored for now
    toggleSelection(rowNumber);
}

void AppSelectionPanel::timerCallback()
{
    // Repaint only the meter area (top 20px)
    repaint(getLocalBounds().removeFromTop(20));
}

void AppSelectionPanel::refreshAppList()
{
    // Store current selection state
    std::map<int, bool> selectionMap;
    for (const auto& app : apps)
        selectionMap[app.processID] = app.isSelected;
    
    // Fetch new list
    // Note: getAvailableApps is synchronous and might block for a moment (3s timeout worst case).
    // In a real app, this should be async, but for now we call it directly.
    auto newApps = systemCapturer.getAvailableApps();
    
    // Restore selection and sort
    for (auto& app : newApps)
    {
        if (selectionMap.find(app.processID) != selectionMap.end())
        {
            app.isSelected = selectionMap[app.processID];
        }
        else
        {
            // Default new apps to selected? Or unselected? Implementation plan said "Default to captured".
            app.isSelected = true;
        }
    }
    
    // Sort by name
    std::sort(newApps.begin(), newApps.end(), [](const ShareableApp& a, const ShareableApp& b) {
        return a.appName < b.appName;
    });

    apps = std::move(newApps);
    
    // If list was empty before (first run), maybe we want to sync the filter?
    // But updateCapturerFilter() pushes current UI state to capturer.
    // Let's just update the UI.
    appList.updateContent();
    repaint();
}

void AppSelectionPanel::toggleSelection(int row)
{
    if (juce::isPositiveAndBelow(row, (int)apps.size()))
    {
        apps[(size_t)row].isSelected = !apps[(size_t)row].isSelected;
        updateCapturerFilter();
        appList.repaintRow(row);
    }
}

void AppSelectionPanel::updateCapturerFilter()
{
    std::vector<int> pids;
    for (const auto& app : apps)
    {
        if (app.isSelected)
            pids.push_back(app.processID);
    }
    systemCapturer.updateFilter(pids);
}
