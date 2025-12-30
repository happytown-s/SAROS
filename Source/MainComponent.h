#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "LooperTrackUi.h"
#include "LooperAudio.h"
#include "InputTap.h"
#include "Util.h"
#include "ThemeColours.h"
#include "TransportPanel.h"
#include "CircularVisualizer.h"

//==============================================================================
// ルーパーアプリ本体
//==============================================================================
class MainComponent :
public juce::AudioAppComponent,
public LooperTrackUi::Listener,
public LooperAudio::Listener,
public juce::Timer
{
	public:
	MainComponent();
	void onRecordingStarted(int trackID) override;
	void onRecordingStopped(int trackID) override;



	~MainComponent() override;

	// AudioAppComponent
	void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
	void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
	void releaseResources() override;

	// JUCE Component
	void paint(juce::Graphics&) override;
	void resized() override;

	// UIイベント
	void trackClicked(LooperTrackUi* trackClicked) override;
	void showDeviceSettings();
	void updateStateVisual();
	int getSelectedTrackId() const {return selectedTrackId;}


	private:
	// ===== オーディオ関連 =====
	InputTap inputTap;
	juce::TriggerEvent& sharedTrigger;
	LooperAudio looper ;//10秒バッファ

	void timerCallback()override;



	// ===== デバイス管理 =====
	juce::AudioDeviceManager deviceManager;
	
	// ===== 設定管理 =====
	std::unique_ptr<juce::PropertiesFile> appProperties;
	void saveAudioDeviceSettings();
	void loadAudioDeviceSettings();

	// ===== UI =====
	CircularVisualizer visualizer;
	TransportPanel transportPanel;
	int selectedTrackId = 0;
	std::atomic<bool> isStandbyMode { false };
    std::atomic<bool> forceRecordRequest { false };


	std::vector<std::unique_ptr<LooperTrackUi>> trackUIs;
	LooperTrackUi* selectedTrack = nullptr;

	const int headerVisualArea = 280;
	const int topHeight = 40;
	const int trackWidth = 80;
	const int trackHeight = 350;
	const int spacing = 10;
	const int tracksPerRow = 8;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
