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
	enum class TrackState{Idle,Standby,Recording,Playing,Stopped};

	LooperTrackUi(int id, TrackState initState = TrackState::Idle);

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
	void resized() override; // 🆕 Added declaration

	void setSelected(bool shouldBeSelected);
	bool getIsSelected() const;
	void setListener(Listener* listener);

	//録音処理
	void startRecording();
	void stopRecording();
	//枠の周囲の光るアニメーション用
	void startFlash();
	void timerCallback() override;
	void drawGlowingBorder(juce::Graphics& g, juce::Colour glowColour);
	void drawGlowingBorder(juce::Graphics& g, juce::Colour glowColour, juce::Rectangle<float> area); // 🆕 Added overload

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

	// Volume Fader
    class FaderLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height,
                              float sliderPos, float minSliderPos, float maxSliderPos,
                              const juce::Slider::SliderStyle, juce::Slider&) override;
    };

    FaderLookAndFeel faderLookAndFeel;
    
    // Custom Slider for Right-Click Reset
    class ResetSlider : public juce::Slider
    {
    public:
        void mouseDown(const juce::MouseEvent& e) override
        {
            if (e.mods.isRightButtonDown())
            {
                setValue(0.75); // Reset to Default
                return;
            }
            juce::Slider::mouseDown(e);
        }
    };
    
	ResetSlider gainSlider;
	float currentRmsLevel = 0.0f;

public:
	std::function<void(float)> onGainChange;
	std::function<void(float)> onLoopMultiplierChange; // 🆕 Added callback

    // Multiplier Buttons
    juce::TextButton mult2xButton { "x2" };
    juce::TextButton multHalfButton { "/2" };

	void setLevel(float rms);
	float getGain() const { return (float)gainSlider.getValue(); }
};

