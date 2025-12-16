/*
  ==============================================================================

    TransportPanel.cpp
    Created: 18 Oct 2025 7:55:35pm
    Author:  mt sh

  ==============================================================================
*/

#include "TransportPanel.h"


TransportPanel::TransportPanel(LooperAudio& looperRef)
: looper(looperRef)

{
	setupRoundButton(recordButton, PizzaColours::TomatoRed);
	setupRoundButton(playButton,   PizzaColours::BasilGreen);
	setupRoundButton(undoButton,   PizzaColours::GrapePurple);
	setupRoundButton(clearButton,  PizzaColours::CheeseYellow);
	setupRoundButton(settingButton,PizzaColours::CreamDough);

	for (auto* btn : {&recordButton, &playButton, &undoButton,&clearButton,&settingButton})
	{
		addAndMakeVisible(btn);
		btn->addListener(this);
	}
}

TransportPanel::~TransportPanel()
{
	for (auto* btn : { &recordButton, &playButton, &undoButton, &clearButton,&settingButton })
		btn->removeListener(this);
}

void TransportPanel::paint(juce::Graphics& g)
{
	g.fillAll(PizzaColours::CreamDough);
	g.setColour(PizzaColours::DeepOvenBrown);
	g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2), 12.0f, 2.0f);
}
//================================
//レイアウト
//================================
void TransportPanel::resized()
{
	auto area = getLocalBounds().reduced(10);
	const int spacing = 10;

	std::vector<std::pair<juce::TextButton*, int>> buttons =
	{
		{&recordButton, 80},
		{&playButton, 80},
		{&undoButton, 80},
		{&clearButton, 80},
		{&settingButton, 100}

	};

	int totalWidth = 0;
	for (auto& [_, w] : buttons) totalWidth += w + spacing;


	int startX = area.getX() + (area.getWidth() - totalWidth + spacing) / 2;
	int y = area.getY();

	for (auto& [btn, width] : buttons)
	{
		btn->setBounds({ startX, y + 5, width, area.getHeight() - 10});
		startX += width + spacing;
	}
}


//===========================================================
//オーディオ部分の状態に合わせたUI更新
//===========================================================
void TransportPanel::buttonClicked(juce::Button* button)
{
	if (!onAction) return;

	if(button == &recordButton)
	{
		if (currentState == State::Recording || currentState == State::Standby)
			onAction("STOP_REC");
		else
			onAction("REC");
	}
	else if (button == &playButton)
	{
		if (currentState == State::Playing)
			onAction("STOP");
		else
			onAction("PLAY");
	}
	else if (button == &undoButton)
	{
		onAction("UNDO");
	}
	else if (button == &clearButton)
	{
		onAction("CLEAR");
	}
	else if (button == &settingButton)
	{
		DBG("⚙️ Device settings open requested");
		onSettingsRequested();
	}
}


// TransportPanel.cpp

void TransportPanel::setState(State newState)
{
	if (currentState == newState) return;
	currentState = newState;
    
    // 汎用ボタンの色のリセット（Idle/Stopped時の色）
    undoButton.setColour(juce::TextButton::buttonColourId, PizzaColours::GrapePurple);
    clearButton.setColour(juce::TextButton::buttonColourId, PizzaColours::CheeseYellow);
    settingButton.setColour(juce::TextButton::buttonColourId, PizzaColours::CreamDough);


	switch (newState)
	{
		case State::Idle:
			// 録音済みトラックがない、初期状態
			recordButton.setButtonText("REC");
			recordButton.setColour(juce::TextButton::buttonColourId, PizzaColours::TomatoRed);
			
			playButton.setButtonText("PLAY");
			playButton.setColour(juce::TextButton::buttonColourId, PizzaColours::MushroomGray); // 押せないようにグレーアウト

			undoButton.setColour(juce::TextButton::buttonColourId, PizzaColours::MushroomGray.withAlpha(0.5f)); // 押せない

			break;

		case State::Standby:
			// 🟡 待機中
			recordButton.setButtonText("WAIT...");
			recordButton.setColour(juce::TextButton::buttonColourId, PizzaColours::CheeseYellow);
			
			playButton.setButtonText("PLAY");
			playButton.setColour(juce::TextButton::buttonColourId, PizzaColours::MushroomGray);
			
			undoButton.setColour(juce::TextButton::buttonColourId, PizzaColours::MushroomGray.withAlpha(0.5f));
			break;

		case State::Recording:
			// 🔴 録音中
			recordButton.setButtonText("STOP");
			recordButton.setColour(juce::TextButton::buttonColourId, PizzaColours::TomatoRed.darker(0.3f)); // 点滅風に濃くする
			
			playButton.setButtonText("PLAY");
			playButton.setColour(juce::TextButton::buttonColourId, PizzaColours::MushroomGray); // 録音中は再生ボタンを無効化
			
			undoButton.setColour(juce::TextButton::buttonColourId, PizzaColours::MushroomGray.withAlpha(0.5f)); // 録音中はUNDO無効

			break;

		case State::Playing:
			// ▶️ 再生中
			recordButton.setButtonText("REC");
			recordButton.setColour(juce::TextButton::buttonColourId, PizzaColours::TomatoRed); // 次の録音待機

			playButton.setButtonText("STOP");
			playButton.setColour(juce::TextButton::buttonColourId, PizzaColours::CheeseYellow);
			
			break;

		case State::Stopped:
			// ⏹ 停止中 (再生可能なトラックあり)
			recordButton.setButtonText("REC");
			recordButton.setColour(juce::TextButton::buttonColourId, PizzaColours::TomatoRed);
			
			playButton.setButtonText("PLAY");
			playButton.setColour(juce::TextButton::buttonColourId, PizzaColours::BasilGreen); // 再生可能
			
			break;
	}
    
	repaint();
}