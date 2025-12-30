/*
  ==============================================================================

    LooperTrackUi.cpp
    Created: 21 Sep 2025 5:33:29pm
    Author:  mt sh

  ==============================================================================
*/

#include "LooperTrackUi.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include "ThemeColours.h"

//==============================================================================
void LooperTrackUi::paint(juce::Graphics& g)
{
	auto bounds = getLocalBounds().toFloat();
	
	// レイアウト定義
	float width = bounds.getWidth();
	float buttonSize = width; // 正方形
	
	juce::Rectangle<float> buttonArea(0, 0, buttonSize, buttonSize);
	// 下部に15pxの隙間を追加し、さらにボタンとの間にも10pxの隙間(gap)を作る
	float gap = 10.0f;
	juce::Rectangle<float> bottomArea(0, buttonSize + gap, width, bounds.getHeight() - buttonSize - 15.0f - gap); 

	// --- 1. Top Selection Button ---
	// Glowing backdrop
    if (isSelected) {
        g.setColour(ThemeColours::NeonCyan.withAlpha(0.15f));
        g.fillRoundedRectangle(buttonArea.expanded(2.0f), 10.0f);
    }

	if(isSelected){
		g.setColour(ThemeColours::ElectricBlue);
	}else if(isMouseOver && buttonArea.contains(getMouseXYRelative().toFloat())){
		g.setColour(ThemeColours::MetalGray.brighter(0.2f));
	}else{
		g.setColour(ThemeColours::MetalGray);
	}
	g.fillRoundedRectangle(buttonArea, 8.0f);
    
    // Border
	g.setColour(isSelected ? ThemeColours::NeonCyan : ThemeColours::Silver.withAlpha(0.4f));
	g.drawRoundedRectangle(buttonArea, 8.0f, 1.5f);

	// State-specific overlays with synchronized pulsing
	float time = (float)juce::Time::getMillisecondCounterHiRes() * 0.001f;
	float pulse = 0.5f + 0.5f * std::sin(time * juce::MathConstants<float>::pi * 0.5f);
	
	if(state == TrackState::Recording){
		float alpha = 0.2f + 0.3f * pulse; // 0.2 to 0.5
		g.setColour(ThemeColours::RecordingRed.withAlpha(alpha));
		g.fillRoundedRectangle(buttonArea, 8.0f);
		drawGlowingBorder(g, ThemeColours::RecordingRed, buttonArea);
	}else if(state == TrackState::Playing){
		float alpha = 0.1f + 0.15f * pulse; // 0.1 to 0.25
		g.setColour(ThemeColours::PlayingGreen.withAlpha(alpha));
		g.fillRoundedRectangle(buttonArea, 8.0f);
		drawGlowingBorder(g, ThemeColours::PlayingGreen, buttonArea);
	} else if (state == TrackState::Standby) {
		float alpha = 0.15f + 0.2f * pulse; // 0.15 to 0.35
        g.setColour(ThemeColours::ElectricBlue.withAlpha(alpha));
        g.fillRoundedRectangle(buttonArea, 8.0f);
        drawGlowingBorder(g, ThemeColours::ElectricBlue, buttonArea);
    }

	// Track Number
	g.setColour(ThemeColours::Silver);
	g.setFont(juce::Font("Inter", 20.0f, juce::Font::bold));
	g.drawText(juce::String(trackId), buttonArea, juce::Justification::centred, true);


	// --- 2. Level Meter (Bottom Left) ---
	// メーターエリア定義 (スライダーの左側)
	juce::Rectangle<float> meterArea = bottomArea.removeFromLeft(width * 0.4f).reduced(4.0f, 0.0f); // 左右のみreduce
	
	// 背景
	g.setColour(juce::Colours::black.withAlpha(0.2f));
	g.fillRoundedRectangle(meterArea, 3.0f);
	
	// レベルバー
	if (currentRmsLevel > 0.0f)
	{
		float levelHeight = meterArea.getHeight() * juce::jmin(currentRmsLevel * 4.0f, 1.0f); // ゲイン少し上げて表示
		juce::Rectangle<float> levelRect(meterArea.getX(), 
										 meterArea.getBottom() - levelHeight, 
										 meterArea.getWidth(), 
										 levelHeight);
		
		g.setColour(ThemeColours::PlayingGreen);
		if (currentRmsLevel > 0.8f) g.setColour(ThemeColours::RecordingRed);
		
		g.fillRoundedRectangle(levelRect, 3.0f);
	}
}

void LooperTrackUi::drawGlowingBorder(juce::Graphics& g, juce::Colour glowColour, juce::Rectangle<float> area)
{
	// Use global time for consistent animation across all tracks
	float time = (float)juce::Time::getMillisecondCounterHiRes() * 0.001f;
	
	// Soft pulsing glow effect (sine wave, 0.5 Hz = gentle pulse)
	float pulse = 0.5f + 0.5f * std::sin(time * juce::MathConstants<float>::pi * 0.5f);
	float glowAlpha = 0.3f + 0.4f * pulse; // Range: 0.3 to 0.7
	float glowThickness = 2.0f + 2.0f * pulse; // Range: 2 to 4
	
	// Draw soft outer glow
	for (int i = 3; i >= 0; --i)
	{
		float expand = (float)i * 2.0f;
		float alpha = glowAlpha * (1.0f - (float)i / 4.0f);
		g.setColour(glowColour.withAlpha(juce::jlimit(0.0f, 1.0f, alpha * 0.5f)));
		g.drawRoundedRectangle(area.expanded(expand), 8.0f + expand * 0.5f, glowThickness);
	}
	
	// Inner bright border
	g.setColour(glowColour.withAlpha(juce::jlimit(0.0f, 1.0f, glowAlpha)));
	g.drawRoundedRectangle(area, 8.0f, glowThickness);
}

//==============================================================================
void LooperTrackUi::FaderLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                                       float sliderPos, float minSliderPos, float maxSliderPos,
                                                       const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    // Tick marks
    g.setColour(ThemeColours::Silver.withAlpha(0.4f));
    int numTicks = 5; 
    float trackSpecWidth = 2.0f;
    float centerX = x + width * 0.5f;
    
    for (int i = 0; i < numTicks; ++i)
    {
        float ratio = (float)i / (float)(numTicks - 1);
        float tickY = y + height - (height * ratio);
        
        float tickSize = (i == 0 || i == numTicks - 1 || i == 2) ? 6.0f : 3.0f;
        
        g.drawLine(centerX - trackSpecWidth - tickSize, tickY, centerX - trackSpecWidth, tickY, 1.0f);
        g.drawLine(centerX + trackSpecWidth, tickY, centerX + trackSpecWidth + tickSize, tickY, 1.0f);
    }

    // Track
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    juce::Rectangle<float> track(centerX - trackSpecWidth * 0.5f, 
                                 (float)y, 
                                 trackSpecWidth, 
                                 (float)height);
    g.fillRoundedRectangle(track, 1.0f);

    // Thumb
    float thumbWidth = (float)width * 0.7f;
    float thumbHeight = 8.0f;
    
    juce::Rectangle<float> thumb(0, 0, thumbWidth, thumbHeight);
    thumb.setCentre(centerX, sliderPos);
    
    g.setColour(ThemeColours::NeonCyan);
    g.fillRoundedRectangle(thumb, 2.0f);
    
    // Thumb line
    g.setColour(juce::Colours::white);
    g.drawLine(thumb.getX() + 2.0f, thumb.getCentreY(), thumb.getRight() - 2.0f, thumb.getCentreY(), 1.0f);
}

//==============================================================================
LooperTrackUi::LooperTrackUi(int id, TrackState initState)
	: trackId(id), state(initState)
{
	// スライダー設定
	gainSlider.setSliderStyle(juce::Slider::LinearVertical);
	gainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
	gainSlider.setRange(0.0, 1.0); // 0倍〜1倍
	gainSlider.setValue(0.75);     // デフォルトを75%（メモリ位置）に設定
    
    // カスタムLookAndFeelを適用
    gainSlider.setLookAndFeel(&faderLookAndFeel);

	gainSlider.onValueChange = [this]()
	{
		if(onGainChange)
			onGainChange((float)gainSlider.getValue());
	};

	addAndMakeVisible(gainSlider);
}

void LooperTrackUi::resized()
{
	auto bounds = getLocalBounds();
	float width = bounds.getWidth();
	float buttonSize = width;
	
	// フェーダーエリア (メーターの右側)
	// 下部に15pxの隙間、上部(ボタン下)に10pxの隙間を作る
	int gap = 10;
	juce::Rectangle<int> bottomArea(0, (int)buttonSize + gap, width, bounds.getHeight() - (int)buttonSize - 15 - gap);
	
	// 左40%はメーター用に空けて、右60%にスライダーを配置
	gainSlider.setBounds(bottomArea.removeFromRight((int)(width * 0.6f)).reduced(0, 0)); // reducedは不要になるかもだが一応0
}

//==============================================================================
void LooperTrackUi::mouseDown(const juce::MouseEvent& e)
{
	// 上部のボタンエリアのみクリック反応
	if (e.y < getWidth()) // 幅＝高さの正方形エリア
	{
		if(listener != nullptr)
			listener->trackClicked(this);
	}
}

void LooperTrackUi::mouseEnter(const juce::MouseEvent&){
	isMouseOver = true;
	repaint();
}
void LooperTrackUi::mouseExit(const juce::MouseEvent&){
	isMouseOver = false;
	repaint();
}

void LooperTrackUi::setSelected(bool shouldBeSelected){
	isSelected = shouldBeSelected;
	repaint();
}
bool LooperTrackUi::getIsSelected() const {return isSelected;}

void LooperTrackUi::setListener(Listener* listener) { this->listener = listener;}

void LooperTrackUi::setState(TrackState newState)
{
	if (state != newState)
    {
        state = newState;
        if (state == TrackState::Recording || state == TrackState::Playing || state == TrackState::Standby)
        {
            if (!isTimerRunning()) {
                flashProgress = 0.0f;
                startTimerHz(60);
            }
        }
        else
        {
            stopTimer(); // アニメーション用タイマー停止
        }
        repaint();
    }
}

juce::String LooperTrackUi::getStateString()const {
	switch (state) {
		case TrackState::Recording: return "Recording";
		case TrackState::Playing: return "Playing";
		case TrackState::Stopped: return "Stopped";
		case TrackState::Idle: return "Idle";
		case TrackState::Standby: return "Standby";
	}
	return "Unknown";
}

void LooperTrackUi::startRecording(){
	setState(TrackState::Recording);
}
void LooperTrackUi::stopRecording(){
	setState(TrackState::Playing);
}

void LooperTrackUi::startFlash(){
	isFlashing = true;
	flashProgress = 0.0f;
}

void LooperTrackUi::timerCallback(){
	// アニメーション更新 (repaint only, global time handles sync)
	if(state == TrackState::Recording || state == TrackState::Playing || state == TrackState::Standby){
		repaint();
	}
}

void LooperTrackUi::setLevel(float rms)
{
	currentRmsLevel = rms;
    // メーター部分の再描画だけでも良いが、アニメーションと同期しているので一括repaintでOK
	// 負荷が気になる場合は repaint(メーター領域) に限定も可能
}

void LooperTrackUi::drawGlowingBorder(juce::Graphics& g, juce::Colour glowColour)
{
    // 旧メソッド（互換性のため残すか、削除して新しい方を使う）
    drawGlowingBorder(g, glowColour, getLocalBounds().toFloat()); 
}

