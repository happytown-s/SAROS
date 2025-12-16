/*
  ==============================================================================

    LooperTrackUi.cpp
    Created: 21 Sep 2025 5:33:29pm
    Author:  mt sh

  ==============================================================================
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "PizzaColours.h"
#include "LooperTrackUi.h"

void LooperTrackUi::paint(juce::Graphics& g)
{

	auto bounds = getLocalBounds().toFloat();

	if(isSelected){
		g.setColour(juce::Colours::darkorange);
	}else if(isMouseOver){
		g.setColour(juce::Colours::skyblue);
	}else{
		g.setColour(juce::Colours::darkgrey);
	}
	g.drawRoundedRectangle(bounds, 10.0f, 4.0f);

	if(state == TrackState::Recording){
		g.setColour(juce::Colours::darkred);
		g.drawRoundedRectangle(bounds, 10.0f, 4.0f);
		drawGlowingBorder(g, juce::Colours::red);

	}else if(state == TrackState::Playing){
		g.setColour(juce::Colours::darkgreen);
		g.drawRoundedRectangle(bounds, 10.0f, 4.0f);
		drawGlowingBorder(g, juce::Colours::green);
	}
	else if (state == TrackState::Standby) {
        // 🟡 待機中：チーズ色で点滅！
        g.setColour(PizzaColours::CheeseYellow);
        g.drawRoundedRectangle(bounds, 10.0f, 4.0f);
        drawGlowingBorder(g, PizzaColours::CheeseYellow);
    }

}

void LooperTrackUi::drawGlowingBorder(juce::Graphics& g,juce::Colour glowColour){
	auto bounds = getLocalBounds().toFloat();

	float totalPerimeter = bounds.getWidth() * 2 + bounds.getHeight()* 2;
	float drawLength = flashProgress * totalPerimeter;

	juce::Line<float> lines[4] = {
		{bounds.getTopLeft(),bounds.getTopRight()},
		{bounds.getTopRight(),bounds.getBottomRight()},
		{bounds.getBottomRight(),bounds.getBottomLeft()},
		{bounds.getBottomLeft(),bounds.getTopLeft()}
	};
	g.setColour(glowColour);

	float remaining = drawLength;

	for(int i = 0; i < 4; ++i){
		auto lineLength = lines[i].getLength();
		if(remaining <= 0) break;

		if(remaining < lineLength){

			//線の一部だけ描く
			juce::Point<float> end = lines[i].getStart() + (lines[i].getEnd() - lines[i].getStart()) * (remaining / lineLength);
			g.drawLine(lines[i].getStart().x,lines[i].getStart().y,end.x,end.y,3.0f);
			break;
		}
		else{
			//線全体を描く
			g.drawLine(lines[i], 3.0f);
			remaining -= lineLength;
		}
	}
}


void LooperTrackUi::mouseDown(const juce::MouseEvent&)
{
	if(listener != nullptr)
		listener->trackClicked(this);
}
void LooperTrackUi::mouseEnter(const juce::MouseEvent&){
	isMouseOver = true;
	//DBG("MouseOver");
	repaint();
}
void LooperTrackUi::mouseExit(const juce::MouseEvent&){
	isMouseOver = false;
	//juce::Logger::writeToLog("MouseExit");
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
        
        // 🆕 動的な状態ならアニメーションタイマーを回す
        if (state == TrackState::Recording || state == TrackState::Playing || state == TrackState::Standby)
        {
            if (!isTimerRunning()) {
                flashProgress = 0.0f;
                startTimerHz(60);
            }
        }
        else
        {
            stopTimer();
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
	flashProgress = 0.0f;
	startTimerHz(60); //60FPS
}
void LooperTrackUi::stopRecording(){
	setState(TrackState::Playing);
	flashProgress = 0.0f;
	startTimerHz(60);
}

void LooperTrackUi::startFlash(){
	isFlashing = true;
	flashProgress = 0.0f;
}



void LooperTrackUi::timerCallback(){
	if(state == TrackState::Recording || state == TrackState::Playing
	   || state == TrackState::Standby){
		flashProgress += 0.02f; //1周で50フレーム
		if(flashProgress >= 1.0f){
			flashProgress = 0.0f;
		}
		repaint();
	}else{
		stopTimer();
	}
}

