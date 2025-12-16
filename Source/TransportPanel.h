/*
  ==============================================================================

    TransportPanel.h
    Created: 18 Oct 2025 7:41:06pm
    Author:  mt sh

  ==============================================================================
*/

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PizzaColours.h"
#include "LooperAudio.h"
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



	//ボタン宣言
	juce::TextButton recordButton {"REC"};
	juce::TextButton playButton {"PLAY"};
	juce::TextButton undoButton {"UNDO"};
	juce::TextButton clearButton {"CLEAR"};
	juce::TextButton settingButton {"SETUP"};

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportPanel);
};
