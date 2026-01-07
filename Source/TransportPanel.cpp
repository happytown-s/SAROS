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
	
	// テストボタン（シンプルなスタイル）
	addAndMakeVisible(testButton);
	testButton.addListener(this);
	testButton.setColour(juce::TextButton::buttonColourId, ThemeColours::NeonCyan.withAlpha(0.3f));
	testButton.setColour(juce::TextButton::textColourOnId, ThemeColours::Silver);
	testButton.setColour(juce::TextButton::textColourOffId, ThemeColours::Silver);
	
	// Visual Mode Button
	addAndMakeVisible(visualModeButton);
	visualModeButton.addListener(this);
	visualModeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::black.withAlpha(0.6f));
	visualModeButton.setColour(juce::TextButton::buttonOnColourId, ThemeColours::NeonCyan.withAlpha(0.3f));
	visualModeButton.setColour(juce::TextButton::textColourOffId, ThemeColours::NeonCyan);
	visualModeButton.setColour(juce::TextButton::textColourOnId, ThemeColours::NeonCyan.brighter());

	// FX Button
	addAndMakeVisible(fxButton);
	fxButton.addListener(this);
	fxButton.setColour(juce::TextButton::buttonColourId, juce::Colours::black.withAlpha(0.6f));
	fxButton.setColour(juce::TextButton::buttonOnColourId, ThemeColours::NeonMagenta.withAlpha(0.3f));
	fxButton.setColour(juce::TextButton::textColourOffId, ThemeColours::NeonMagenta);
	fxButton.setColour(juce::TextButton::textColourOnId, ThemeColours::NeonMagenta.brighter());
}

TransportPanel::~TransportPanel()
{
	// MIDIリスナー解除
	if (midiManager != nullptr)
		midiManager->removeListener(this);
	
	// LookAndFeelを解除
	for (auto* btn : {&recordButton, &playButton, &undoButton, &clearButton, &settingButton})
	{
		btn->setLookAndFeel(nullptr);
		btn->removeListener(this);
	}
	testButton.removeListener(this);
	visualModeButton.removeListener(this);
	fxButton.removeListener(this);
}

void TransportPanel::setVisualModeButtonText(const juce::String& text)
{
	visualModeButton.setButtonText(text);
}

void TransportPanel::paint(juce::Graphics& g)
{
    // Background and border removed for transparent look
}
//================================
//レイアウト
//================================
void TransportPanel::resized()
{
	auto area = getLocalBounds().reduced(5);
	const int spacing = 5;  // spacingも縮小してグロー領域を確保
	const int buttonWidth = 50;  // 幅を50pxに（グロー用余白込み）
	const int buttonHeight = buttonWidth;  // ラベルなしなので正方形
	
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
	// ボタンを垂直方向に中央揃え（グローのために余白を確保）
	int y = area.getY() + (area.getHeight() - buttonHeight) / 2;
	
	for (auto* btn : buttons)
	{
		btn->setBounds(startX, y, buttonWidth, buttonHeight);
		startX += buttonWidth + spacing;
	}
	
	// テストボタンは右端に配置（ボタンと同じ高さに揃える）
	int sideButtonH = 30;
	int sideButtonY = area.getY() + (area.getHeight() - sideButtonH) / 2;
	testButton.setBounds(area.getRight() - 50, sideButtonY, 45, sideButtonH);
	
	// Visual Mode Buttonは左端に配置
	visualModeButton.setBounds(area.getX(), sideButtonY, 120, sideButtonH);
	
	// FX Button Position (Right of Visual Mode Button)
	fxButton.setBounds(visualModeButton.getRight() + 10, sideButtonY, 50, sideButtonH);
}


//===========================================================
//オーディオ部分の状態に合わせたUI更新
//===========================================================
void TransportPanel::buttonClicked(juce::Button* button)
{
	// MIDI Learnモード時の処理
	if (midiManager != nullptr && midiManager->isLearnModeActive())
	{
		juce::String controlId;
		
		if (button == &recordButton)
			controlId = "transport_rec";
		else if (button == &playButton)
			controlId = "transport_play";
		else if (button == &undoButton)
			controlId = "transport_undo";
		else if (button == &clearButton)
			controlId = "transport_clear";
		else if (button == &settingButton)
			controlId = "transport_settings";
		
		if (controlId.isNotEmpty())
		{
			handleButtonClick(static_cast<juce::TextButton*>(button), controlId);
			return;
		}
	}
	
	// 通常のボタン処理
	if (button == &visualModeButton)
	{
		if (onToggleTracks) onToggleTracks();
		return;
	}
	if (button == &fxButton)
	{
		if (onShowFX) onShowFX();
		return;
	}
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
		// Recording、Standby、Playing状態ではSTOPを送信
		if (currentState == State::Playing || 
		    currentState == State::Recording || 
		    currentState == State::Standby)
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
	else if (button == &testButton)
	{
		DBG("🔊 Test click requested");
		if (onTestClick) onTestClick();
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
			// スタンバイ中でも停止ボタンを表示（既存の再生トラックを停止可能に）
			playButton.setButtonText("STOP");
			playButton.setColour(juce::TextButton::buttonColourId, ThemeColours::ElectricBlue);
			break;

		case State::Recording:
			recordButton.setButtonText("STOP_REC");
			recordButton.setColour(juce::TextButton::buttonColourId, ThemeColours::RecordingRed);
			// 録音中でも停止ボタンを表示（全停止可能に）
			playButton.setButtonText("STOP");
			playButton.setColour(juce::TextButton::buttonColourId, ThemeColours::ElectricBlue);
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

// =====================================================
// MIDI Learn
// =====================================================

void TransportPanel::setMidiLearnManager(MidiLearnManager* manager)
{
	if (midiManager != nullptr)
		midiManager->removeListener(this);
	
	midiManager = manager;
	
	if (midiManager != nullptr)
		midiManager->addListener(this);
}

void TransportPanel::handleButtonClick(juce::TextButton* button, const juce::String& controlId)
{
	// MIDI Learnモードの場合、学習対象として設定
	if (midiManager != nullptr && midiManager->isLearnModeActive())
	{
		midiManager->setLearnTarget(controlId);
		DBG("MIDI Learn: Waiting for input - " + controlId);
		// MIDI信号を待つ（MidiLearnManagerが自動的に処理）
		return;
	}
	
	// 通常のボタン処理は既存のbuttonClickedで処理
}

void TransportPanel::midiValueReceived(const juce::String& controlId, float value)
{
	// MIDI信号を受信した時の処理
	// Note: 値に関わらずトリガー（トグル動作のMIDIコントローラーに対応するため、0受信時も実行する）
	// if (value < 0.5f) return;
	
	// controlIdに応じてボタンをトリガー
	if (controlId == "transport_rec")
		buttonClicked(&recordButton);
	else if (controlId == "transport_play")
		buttonClicked(&playButton);
	else if (controlId == "transport_undo")
		buttonClicked(&undoButton);
	else if (controlId == "transport_clear")
		buttonClicked(&clearButton);
	else if (controlId == "transport_settings")
		buttonClicked(&settingButton);
}

void TransportPanel::midiLearnModeChanged(bool isActive)
{
	if (isActive)
		startTimer(30); // 30ms間隔で点滅アニメーション用
	else
	{
		stopTimer();
		repaint();
	}
}

void TransportPanel::timerCallback()
{
	// 点滅アニメーションのために再描画
	if (midiManager != nullptr && midiManager->isLearnModeActive())
		repaint();
}

juce::String TransportPanel::getControlIdForButton(juce::Button* button)
{
	if (button == &recordButton)       return "transport_rec";
	if (button == &playButton)         return "transport_play";
	if (button == &undoButton)         return "transport_undo";
	if (button == &clearButton)        return "transport_clear";
	if (button == &settingButton)      return "transport_settings";
	return "";
}

void TransportPanel::paintOverChildren(juce::Graphics& g)
{
	if (midiManager == nullptr || !midiManager->isLearnModeActive())
		return;

	// MIDI Learnモード時のオーバーレイ描画
	std::vector<juce::TextButton*> buttons = { &recordButton, &playButton, &undoButton, &clearButton, &settingButton };
	
	// 点滅用アルファ値 (0.3 ~ 0.7)
	float alpha = juce::jlimit(0.0f, 1.0f, 0.5f + 0.2f * std::sin(juce::Time::getMillisecondCounter() * 0.01f));
	
	for (auto* btn : buttons)
	{
		juce::String controlId = getControlIdForButton(btn);
		if (controlId.isEmpty()) continue;
		
		auto bounds = btn->getBounds().toFloat().expanded(2.0f);
		
		// 1. 学習対象として選択されている場合（黄色点滅）
		if (midiManager->getLearnTarget() == controlId)
		{
			g.setColour(juce::Colours::yellow.withAlpha(alpha));
			g.drawRoundedRectangle(bounds, 5.0f, 3.0f);
			
			g.setColour(juce::Colours::yellow.withAlpha(0.2f));
			g.fillRoundedRectangle(bounds, 5.0f);
		}
		// 2. 既にマッピングされている場合（緑枠）
		else if (midiManager->hasMapping(controlId))
		{
			g.setColour(ThemeColours::PlayingGreen.withAlpha(0.8f));
			g.drawRoundedRectangle(bounds, 5.0f, 2.0f);
		}
		// 3. マッピングされていないが対象可能な場合（薄い白枠でヒント）
		else
		{
			g.setColour(ThemeColours::Silver.withAlpha(0.2f));
			g.drawRoundedRectangle(bounds, 5.0f, 1.0f);
		}
	}
}