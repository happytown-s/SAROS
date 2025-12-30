/*
  ==============================================================================

    TransportPanel.h
    Created: 18 Oct 2025 7:41:06pm
    Author:  mt sh

  ==============================================================================
*/

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "ThemeColours.h"
#include "LooperAudio.h"
#include "RoundButtonLookAndFeel.h"
// =====================================================
// 🎛 TransportPanel クラス宣言
// =====================================================
class TransportPanel: public juce::Component,
					public juce::Button::Listener

{
public:
	TransportPanel(LooperAudio& looperRef);
	~TransportPanel() override ;
	enum class State {Idle, Standby, Recording, Playing, Stopped };

	std::function<void(const juce::String&)> onAction;
	std::function<void()> onSettingsRequested;


	void paint(juce::Graphics& g)override;
	void resized() override;
	void buttonClicked(juce::Button* button) override;
	void setState(State newState);
	State getState() const{return currentState;}


private:

	State currentState = State::Idle;
	LooperAudio& looper;

	// 🍕 丸型ボタン用LookAndFeel
	RoundButtonLookAndFeel roundLookAndFeel;

	//ボタン宣言
	juce::TextButton recordButton {juce::String::fromUTF8("\xE2\x8F\xBA")};   // ⏺
	juce::TextButton playButton {juce::String::fromUTF8("\xE2\x96\xB6")};     // ▶
	juce::TextButton undoButton {juce::String::fromUTF8("\xE2\x86\xB6")};     // ↶
	juce::TextButton clearButton {juce::String::fromUTF8("\xE2\x8C\xAB")};    // ⌫
	juce::TextButton settingButton {juce::String::fromUTF8("\xE2\x9A\x99")};  // ⚙

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportPanel);
};
