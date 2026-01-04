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
        g.setFont(14.0f);
        
        juce::String text = isWaiting ? "Press a key..." : displayText;
        if (text.isEmpty()) text = "Not Set";
        
        g.drawText(text, bounds.reduced(8, 0), juce::Justification::centred);
    }
    
    void mouseDown(const juce::MouseEvent&) override
    {
        isWaiting = true;
        repaint();
        grabKeyboardFocus();
    }
    
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (!isWaiting) return false;
        
        // ESCキーでキャンセル
        if (key.getKeyCode() == juce::KeyPress::escapeKey)
        {
            isWaiting = false;
            repaint();
            return true;
        }
        
        // Deleteキーでクリア
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
        
        // マッピングを設定
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
    
    std::function<void()> onMappingChanged;
    
private:
    KeyboardMappingManager& manager;
    juce::String action;
    juce::String displayText;
    bool isWaiting = false;
};

// =====================================================
// キーボード設定タブのコンテンツ
// =====================================================
class KeyboardTabContent : public juce::Component
{
public:
    KeyboardTabContent(KeyboardMappingManager& mgr) : manager(mgr)
    {
        // Header
        headerLabel.setText("Keyboard Shortcuts", juce::dontSendNotification);
        headerLabel.setFont(juce::FontOptions(18.0f, juce::Font::bold));
        headerLabel.setColour(juce::Label::textColourId, ThemeColours::Silver);
        addAndMakeVisible(headerLabel);
        
        // Description
        descLabel.setText("Click to assign key. Delete/Backspace to clear.", 
                          juce::dontSendNotification);
        descLabel.setFont(juce::FontOptions(12.0f));
        descLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
        addAndMakeVisible(descLabel);
        
        // アクション一覧を作成
        auto actions = KeyboardMappingManager::getAllActions();
        for (const auto& action : actions)
        {
            auto label = std::make_unique<juce::Label>();
            label->setText(action.displayName, juce::dontSendNotification);
            label->setColour(juce::Label::textColourId, juce::Colours::white);
            label->setFont(juce::FontOptions(14.0f));
            addAndMakeVisible(*label);
            actionLabels.add(std::move(label));
            
            auto field = std::make_unique<KeyInputField>(manager, action.id);
            field->onMappingChanged = [this]() {
                // 他のフィールドの表示も更新（キーが奪われた場合）
                for (auto* f : keyFields)
                    f->updateDisplay();
                repaint();
            };
            addAndMakeVisible(*field);
            keyFields.add(field.release());
        }
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
        
        int rowHeight = 36;
        int labelWidth = 180;
        int fieldWidth = 160;
        
        for (int i = 0; i < actionLabels.size(); ++i)
        {
            auto row = area.removeFromTop(rowHeight);
            actionLabels[i]->setBounds(row.removeFromLeft(labelWidth));
            row.removeFromLeft(20); // spacing
            keyFields[i]->setBounds(row.removeFromLeft(fieldWidth).reduced(0, 4));
        }
    }
    
private:
    KeyboardMappingManager& manager;
    juce::Label headerLabel;
    juce::Label descLabel;
    juce::OwnedArray<juce::Label> actionLabels;
    juce::OwnedArray<KeyInputField> keyFields;
};
