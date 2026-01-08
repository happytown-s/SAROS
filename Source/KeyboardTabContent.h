#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "KeyboardMappingManager.h"
#include "ThemeColours.h"

// =====================================================
// キー入力フィールドコンポーネント
// =====================================================
class KeyInputField : public juce::Component
{
public:
    KeyInputField(KeyboardMappingManager& mgr, const juce::String& actionId)
        : manager(mgr), action(actionId)
    {
        setWantsKeyboardFocus(true);
        updateDisplay();
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // 背景
        if (isWaiting)
        {
            g.setColour(ThemeColours::NeonCyan.withAlpha(0.3f));
            g.fillRoundedRectangle(bounds, 4.0f);
            g.setColour(ThemeColours::NeonCyan);
            g.drawRoundedRectangle(bounds.reduced(1), 4.0f, 2.0f);
        }
        else
        {
            g.setColour(juce::Colour(0xff2a2a2a));
            g.fillRoundedRectangle(bounds, 4.0f);
            g.setColour(juce::Colours::grey);
            g.drawRoundedRectangle(bounds.reduced(1), 4.0f, 1.0f);
        }
        
        // テキスト
        g.setColour(isWaiting ? ThemeColours::NeonCyan : juce::Colours::white);
        g.setFont(isCompact ? 11.0f : 14.0f);
        
        juce::String text = isWaiting ? "..." : displayText;
        if (text.isEmpty()) text = isCompact ? "-" : "Not Set";
        
        g.drawText(text, bounds.reduced(2, 0), juce::Justification::centred);
    }
    
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown())
        {
            manager.clearMapping(action);
            updateDisplay();
            repaint();
            if (onMappingChanged) onMappingChanged();
            return;
        }
        
        isWaiting = true;
        repaint();
        grabKeyboardFocus();
    }
    
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (!isWaiting) return false;
        
        if (key.getKeyCode() == juce::KeyPress::escapeKey)
        {
            isWaiting = false;
            repaint();
            return true;
        }
        
        if (key.getKeyCode() == juce::KeyPress::deleteKey || 
            key.getKeyCode() == juce::KeyPress::backspaceKey)
        {
            manager.clearMapping(action);
            updateDisplay();
            isWaiting = false;
            repaint();
            if (onMappingChanged) onMappingChanged();
            return true;
        }
        
        manager.setMapping(action, key.getKeyCode());
        updateDisplay();
        isWaiting = false;
        repaint();
        
        if (onMappingChanged) onMappingChanged();
        return true;
    }
    
    void focusLost(FocusChangeType) override
    {
        isWaiting = false;
        repaint();
    }
    
    void updateDisplay()
    {
        int keyCode = manager.getKeyForAction(action);
        displayText = KeyboardMappingManager::getKeyDescription(keyCode);
    }
    
    void cancelWaiting()
    {
        if (isWaiting)
        {
            isWaiting = false;
            repaint();
        }
    }
    
    void setCompact(bool compact) { isCompact = compact; }
    
    std::function<void()> onMappingChanged;
    
private:
    KeyboardMappingManager& manager;
    juce::String action;
    juce::String displayText;
    bool isWaiting = false;
    bool isCompact = false;
};

// =====================================================
// スクロール可能なコンテンツ本体（通常アクション + トラック/FXグリッド）
// =====================================================
class KeyboardListContent : public juce::Component
{
public:
    KeyboardListContent(KeyboardMappingManager& mgr) : manager(mgr)
    {
        // 通常アクション一覧（トラック選択はグリッドに移動するので除外）
        auto actions = KeyboardMappingManager::getAllActions();
        for (const auto& action : actions)
        {
            // Track 1-8はグリッドで表示するのでスキップ
            if (juce::String(action.id).startsWith("track_"))
                continue;
            
            auto label = std::make_unique<juce::Label>();
            label->setText(action.displayName, juce::dontSendNotification);
            label->setColour(juce::Label::textColourId, juce::Colours::white);
            label->setFont(juce::FontOptions(14.0f));
            addAndMakeVisible(*label);
            actionLabels.add(std::move(label));
            
            auto field = std::make_unique<KeyInputField>(manager, action.id);
            field->onMappingChanged = [this]() {
                for (auto* f : keyFields) f->updateDisplay();
                for (auto* f : gridKeyFields) f->updateDisplay();
                repaint();
            };
            addAndMakeVisible(*field);
            keyFields.add(field.release());
        }
        
        // グリッドセクションヘッダー
        gridHeader.setText("Track & FX Slot Toggle (Grid)", juce::dontSendNotification);
        gridHeader.setFont(juce::FontOptions(16.0f, juce::Font::bold));
        gridHeader.setColour(juce::Label::textColourId, ThemeColours::NeonCyan);
        addAndMakeVisible(gridHeader);
        
        // 行ラベル（Track Select, Slot 1-4）= 5行
        const char* rowLabels[] = {"Select", "Slot 1", "Slot 2", "Slot 3", "Slot 4"};
        for (int r = 0; r < 5; ++r)
        {
            auto label = std::make_unique<juce::Label>();
            label->setText(rowLabels[r], juce::dontSendNotification);
            label->setColour(juce::Label::textColourId, juce::Colours::white);
            label->setFont(juce::FontOptions(12.0f));
            addAndMakeVisible(*label);
            gridRowLabels.add(std::move(label));
        }
        
        // 列ヘッダー（T1-T8）
        for (int c = 0; c < 8; ++c)
        {
            auto label = std::make_unique<juce::Label>();
            label->setText("T" + juce::String(c + 1), juce::dontSendNotification);
            label->setColour(juce::Label::textColourId, ThemeColours::Silver);
            label->setFont(juce::FontOptions(11.0f, juce::Font::bold));
            label->setJustificationType(juce::Justification::centred);
            addAndMakeVisible(*label);
            gridColHeaders.add(std::move(label));
        }
        
        // グリッドのキー入力フィールド（5行 × 8列 = 40個）
        for (int row = 0; row < 5; ++row)
        {
            for (int col = 0; col < 8; ++col)
            {
                juce::String actionId;
                if (row == 0)
                {
                    // Track Select
                    actionId = "track_" + juce::String(col + 1);
                }
                else
                {
                    // Slot 1-4
                    actionId = "fx_t" + juce::String(col + 1) + "_slot" + juce::String(row) + "_bypass";
                }
                
                auto field = std::make_unique<KeyInputField>(manager, actionId);
                field->setCompact(true);
                field->onMappingChanged = [this]() {
                    for (auto* f : keyFields) f->updateDisplay();
                    for (auto* f : gridKeyFields) f->updateDisplay();
                    repaint();
                };
                addAndMakeVisible(*field);
                gridKeyFields.add(field.release());
            }
        }
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff151515));
    }
    
    void resized() override
    {
        int rowHeight = 36;
        int labelWidth = 180;
        int fieldWidth = 160;
        int padding = 10;
        
        auto area = getLocalBounds().reduced(padding, 0);
        
        // 通常アクション
        for (int i = 0; i < actionLabels.size(); ++i)
        {
            auto row = area.removeFromTop(rowHeight);
            actionLabels[i]->setBounds(row.removeFromLeft(labelWidth));
            row.removeFromLeft(20);
            keyFields[i]->setBounds(row.removeFromLeft(fieldWidth).reduced(0, 4));
        }
        
        // スペーサー
        area.removeFromTop(20);
        
        // グリッドセクション
        gridHeader.setBounds(area.removeFromTop(30));
        area.removeFromTop(5);
        
        int gridCellWidth = 42;
        int gridCellHeight = 28;
        int gridRowLabelWidth = 50;
        
        // 列ヘッダー
        auto headerRow = area.removeFromTop(20);
        headerRow.removeFromLeft(gridRowLabelWidth);
        for (int c = 0; c < 8; ++c)
        {
            gridColHeaders[c]->setBounds(headerRow.removeFromLeft(gridCellWidth));
        }
        
        // グリッド本体（5行）
        for (int row = 0; row < 5; ++row)
        {
            auto gridRow = area.removeFromTop(gridCellHeight);
            gridRowLabels[row]->setBounds(gridRow.removeFromLeft(gridRowLabelWidth).reduced(0, 2));
            
            for (int col = 0; col < 8; ++col)
            {
                int idx = row * 8 + col;
                gridKeyFields[idx]->setBounds(gridRow.removeFromLeft(gridCellWidth).reduced(2, 2));
            }
        }
    }
    
    int getPreferredHeight() const
    {
        int normalHeight = (int)actionLabels.size() * 36 + 10;
        int gridHeight = 30 + 5 + 20 + 5 * 28 + 20; // header + spacing + col headers + 5 rows + bottom margin
        return normalHeight + 20 + gridHeight;
    }
    
    void cancelAllWaiting()
    {
        for (auto* f : keyFields)
            f->cancelWaiting();
        for (auto* f : gridKeyFields)
            f->cancelWaiting();
    }
    
private:
    KeyboardMappingManager& manager;
    juce::OwnedArray<juce::Label> actionLabels;
    juce::OwnedArray<KeyInputField> keyFields;
    
    // グリッド用
    juce::Label gridHeader;
    juce::OwnedArray<juce::Label> gridRowLabels;
    juce::OwnedArray<juce::Label> gridColHeaders;
    juce::OwnedArray<KeyInputField> gridKeyFields;  // 5行 × 8列 = 40個
};

// =====================================================
// キーボード設定タブのコンテンツ（Viewport付き）
// =====================================================
class KeyboardTabContent : public juce::Component
{
public:
    KeyboardTabContent(KeyboardMappingManager& mgr) 
        : manager(mgr), listContent(mgr)
    {
        headerLabel.setText("Keyboard Shortcuts", juce::dontSendNotification);
        headerLabel.setFont(juce::FontOptions(18.0f, juce::Font::bold));
        headerLabel.setColour(juce::Label::textColourId, ThemeColours::Silver);
        addAndMakeVisible(headerLabel);
        
        descLabel.setText("Click to assign key. Delete/Backspace to clear.", 
                          juce::dontSendNotification);
        descLabel.setFont(juce::FontOptions(12.0f));
        descLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
        addAndMakeVisible(descLabel);
        
        viewport.setViewedComponent(&listContent, false);
        viewport.setScrollBarsShown(true, false);
        addAndMakeVisible(viewport);
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff151515));
    }
    
    void resized() override
    {
        auto area = getLocalBounds().reduced(20);
        
        headerLabel.setBounds(area.removeFromTop(30));
        descLabel.setBounds(area.removeFromTop(25));
        area.removeFromTop(10);
        
        listContent.setSize(area.getWidth() - 15, listContent.getPreferredHeight());
        viewport.setBounds(area);
    }
    
    void mouseDown(const juce::MouseEvent&) override
    {
        listContent.cancelAllWaiting();
    }
    
private:
    KeyboardMappingManager& manager;
    juce::Label headerLabel;
    juce::Label descLabel;
    KeyboardListContent listContent;
    juce::Viewport viewport;
};
