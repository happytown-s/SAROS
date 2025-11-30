#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "LooperAudio.h"
/*
  ==============================================================================

    LooperTrackUi.h
    Created: 21 Sep 2025 5:33:19pm
    Author:  mt sh
  ==============================================================================
*/

class LooperTrackUi : public juce::Component,juce::Timer

{
	public :

	//トラックの状態
	enum class TrackState{Idle,Recording,Playing,Stopped};

	LooperTrackUi(int id, TrackState initState = TrackState::Idle)
	: trackId(id),state(initState){}

	~LooperTrackUi() override = default;
	//リスナー関数のオーバーライド。クリックされたトラックの把握に必要
	class Listener
	{
		public:
		virtual ~Listener() = default;
		virtual void trackClicked(LooperTrackUi* track) = 0;
	};

	void setTrackId(int id){trackId = id;}
	int getTrackId() const{return trackId;}

	void setState(TrackState newState);
	TrackState getState() const {return state;}
	juce::String getStateString() const;



	void mouseDown(const juce::MouseEvent&) override;
	void mouseEnter(const juce::MouseEvent&) override;
	void mouseExit(const juce::MouseEvent&)override;
	void setSelected(bool shouldBeSelected);
	bool getIsSelected() const;
	void setListener(Listener* listener);

	//録音処理
	void startRecording();
	void stopRecording();
	//枠の周囲の光るアニメーション用
	void startFlash();
	void timerCallback() override;
	void drawGlowingBorder(juce::Graphics& g,juce::Colour glowColour);

	protected:
	void paint(juce::Graphics& g) override;



	private:

	//マウスの状態関連
	bool isMouseOver = false;
	bool isSelected = false;
	Listener* listener = nullptr;
	//トラックの状態＆トラックNo初期化
	int trackId ;
	TrackState state = TrackState::Idle;


	float flashProgress = 0.0f;
	bool isFlashing = false;
};

