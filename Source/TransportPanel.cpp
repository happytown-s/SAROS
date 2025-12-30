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
	setupRoundButton(clearButton,  PizzaColours::CheeseYellow);
	setupRoundButton(settingButton,PizzaColours::CreamDough);

    // テキストIDを設定 (LookAndFeelで判別するため)
    undoButton.setButtonText("UNDO");
    clearButton.setButtonText("CLEAR");
    settingButton.setButtonText("SETUP");
    recordButton.setButtonText("REC");
    playButton.setButtonText("PLAY");

	// 🍕 カスタムLookAndFeelを適用
	for (auto* btn : {&recordButton, &playButton, &undoButton, &clearButton, &settingButton})
	{
		addAndMakeVisible(btn);
		btn->addListener(this);
		btn->setLookAndFeel(&roundLookAndFeel);  // カスタムLookAndFeelを適用
	}
}

TransportPanel::~TransportPanel()
{
	// LookAndFeelを解除
	for (auto* btn : {&recordButton, &playButton, &undoButton, &clearButton, &settingButton})
	{
		btn->setLookAndFeel(nullptr);
		btn->removeListener(this);
	}
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
	const int buttonWidth = 50;  // 幅を50pxに固定（大きくなりすぎないように）
	const int buttonHeight = buttonWidth + 20;      // 高さ（ラベル用スペース追加）
	
	std::vector<juce::TextButton*> buttons =
	{
		&recordButton,
		&playButton,
		&undoButton,
		&clearButton,
		&settingButton
	};
	
	int totalWidth = buttonWidth * buttons.size() + spacing * (buttons.size() - 1);
	int startX = area.getX() + (area.getWidth() - totalWidth) / 2;
	int y = area.getY();
	
	for (auto* btn : buttons)
	{
		btn->setBounds(startX, y, buttonWidth, buttonHeight);  // 縦長
		startX += buttonWidth + spacing;
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
	if(button == &recordButton)
	{
		// 録音中のみ「停止」を送る。スタンバイ中はもう一度「REC」を送って強制開始させる。
		if (currentState == State::Recording)
			onAction("STOP_REC");
		else
			onAction("REC");
	}
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
			playButton.setColour(juce::TextButton::buttonColourId, PizzaColours::MushroomGray);

			undoButton.setColour(juce::TextButton::buttonColourId, PizzaColours::MushroomGray.withAlpha(0.5f));

			break;

		case State::Standby:
			// 🟭 待機中
			recordButton.setButtonText("REC");
			recordButton.setColour(juce::TextButton::buttonColourId, PizzaColours::CheeseYellow);
			
			playButton.setButtonText("PLAY");
			playButton.setColour(juce::TextButton::buttonColourId, PizzaColours::MushroomGray);
			
			undoButton.setColour(juce::TextButton::buttonColourId, PizzaColours::MushroomGray.withAlpha(0.5f));
			break;

		case State::Recording:
			// 🔴 録音中
			recordButton.setButtonText("STOP_REC");
			recordButton.setColour(juce::TextButton::buttonColourId, PizzaColours::TomatoRed.darker(0.3f));
			
			playButton.setButtonText("PLAY");
			playButton.setColour(juce::TextButton::buttonColourId, PizzaColours::MushroomGray);
			
			undoButton.setColour(juce::TextButton::buttonColourId, PizzaColours::MushroomGray.withAlpha(0.5f));

			break;

		case State::Playing:
			// ▶️ 再生中
			recordButton.setButtonText("REC");
			recordButton.setColour(juce::TextButton::buttonColourId, PizzaColours::TomatoRed);

			playButton.setButtonText("STOP"); 
			playButton.setColour(juce::TextButton::buttonColourId, PizzaColours::CheeseYellow);
			
			break;

		case State::Stopped:
			// ⏹ 停止中 (再生可能なトラックあり)
			recordButton.setButtonText("REC");
			recordButton.setColour(juce::TextButton::buttonColourId, PizzaColours::TomatoRed);
			
			playButton.setButtonText("PLAY"); 
			playButton.setColour(juce::TextButton::buttonColourId, PizzaColours::BasilGreen);
			
			break;
	}
    
	repaint();
}