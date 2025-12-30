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
	setupFuturisticButton(recordButton,  ThemeColours::RecordingRed);
	setupFuturisticButton(playButton,    ThemeColours::PlayingGreen);
	setupFuturisticButton(undoButton,    ThemeColours::MetalGray);
	setupFuturisticButton(clearButton,   ThemeColours::MetalGray);
	setupFuturisticButton(settingButton, ThemeColours::MetalGray);

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
	g.fillAll(ThemeColours::Background.withAlpha(0.0f)); // Transparent background
	g.setColour(ThemeColours::NeonCyan.withAlpha(0.2f));
	g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2), 12.0f, 1.5f);
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
    
	switch (newState)
	{
		case State::Idle:
			recordButton.setButtonText("REC"); 
			recordButton.setColour(juce::TextButton::buttonColourId, ThemeColours::MetalGray);
			playButton.setButtonText("PLAY");
			playButton.setColour(juce::TextButton::buttonColourId, ThemeColours::MetalGray);
			break;

		case State::Standby:
			recordButton.setButtonText("REC");
			recordButton.setColour(juce::TextButton::buttonColourId, ThemeColours::ElectricBlue);
			playButton.setButtonText("PLAY");
			playButton.setColour(juce::TextButton::buttonColourId, ThemeColours::MetalGray);
			break;

		case State::Recording:
			recordButton.setButtonText("STOP_REC");
			recordButton.setColour(juce::TextButton::buttonColourId, ThemeColours::RecordingRed);
			playButton.setButtonText("PLAY");
			playButton.setColour(juce::TextButton::buttonColourId, ThemeColours::MetalGray);
			break;

		case State::Playing:
			recordButton.setButtonText("REC");
			recordButton.setColour(juce::TextButton::buttonColourId, ThemeColours::MetalGray);
			playButton.setButtonText("STOP"); 
			playButton.setColour(juce::TextButton::buttonColourId, ThemeColours::ElectricBlue);
			break;

		case State::Stopped:
			recordButton.setButtonText("REC");
			recordButton.setColour(juce::TextButton::buttonColourId, ThemeColours::MetalGray);
			playButton.setButtonText("PLAY"); 
			playButton.setColour(juce::TextButton::buttonColourId, ThemeColours::PlayingGreen);
			break;
	}
    
	repaint();
}