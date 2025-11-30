#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
	: sharedTrigger(inputTap.getTriggerEvent()),
		looper(44100, 44100 * 10),
		transportPanel(looper)
{
	setAudioChannels(2, 2);
	deviceManager.addAudioCallback(&inputTap); // 入力だけTapする

	startTimerHz(30);

	// トラック初期化
	for (int i = 0; i < 8; ++i)
	{
		int newId = static_cast<int>(trackUIs.size() + 1);
		auto track = std::make_unique<LooperTrackUi>(newId, LooperTrackUi::TrackState::Idle);
		track->setListener(this);
		addAndMakeVisible(track.get());
		trackUIs.push_back(std::move(track));
		looper.addTrack(newId);
	}

	//pizzaビジュアライザー仮置き
	addAndMakeVisible(pizzaVisualizer);
	// ボタン類設定
	addAndMakeVisible(transportPanel);

	transportPanel.onAction = [this](const juce::String& action)
	{
		if      (action == "REC")    looper.startRecording(selectedTrackId);
		else if (action == "PLAY")   looper.startPlaying(selectedTrackId);
		else if (action == "STOP")   looper.stopPlaying(selectedTrackId);
		else if (action == "UNDO")   looper.undoLastRecording();
		else if (action == "CLEAR")   looper.allClear();
		else if (action == "SETUP")   showDeviceSettings();

	};
	transportPanel.onSettingsRequested = [this]()
	{
		showDeviceSettings();
	};




	setSize(1000, 800);


	//ルーパーからのリスナーイベントを受け取る
	looper.addListener(this);
}

MainComponent::~MainComponent()
{
	shutdownAudio();
}

//==============================================================================

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
	inputTap.prepare(sampleRate, samplesPerBlockExpected);
	looper.prepareToPlay(samplesPerBlockExpected, sampleRate);
	looper.setTriggerReference(inputTap.getManager().getTriggerEvent());

	DBG("InputTap trigger address = " + juce::String((juce::uint64)(uintptr_t)&inputTap.getTriggerEvent()));
	DBG("Shared trigger address   = " + juce::String((juce::uint64)(uintptr_t)&sharedTrigger));

}

void MainComponent::releaseResources()
{
	looper.stopAllTracks();
	DBG("releaseResources called");
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
	auto& trig = sharedTrigger;
	bufferToFill.clearActiveBufferRegion();

	// 入力バッファを取得
	juce::AudioBuffer<float> input(bufferToFill.buffer->getNumChannels(),
								   bufferToFill.numSamples);
	input.clear();
	inputTap.getLatestInput(input);

	// === トリガーが立ったら ===

	if (trig.triggerd)
	{
		
		bool anyRecording = false;
		for (auto& t : trackUIs)
		{
			if (t->getIsSelected() &&
				t->getState() == LooperTrackUi::TrackState::Recording)
			{
				anyRecording = true;
				break;
			}
		}

		if (!anyRecording)
		{
			// 🟢 新規録音を開始
			for (auto& t : trackUIs)
			{
				if (t->getIsSelected())
				{
					looper.startRecording(t->getTrackId());

					juce::MessageManager::callAsync([this, &trig, &t]()
					{t->setState(LooperTrackUi::TrackState::Recording);
					});
				}
			}
		}
		else
		{
			// 鎮火！
			trig.triggerd = false;
			trig.sampleInBlock = -1;
			trig.absIndex = -1;
		}
			
	}
	// 🌀 LooperAudio の処理は常に実行
	looper.processBlock(*bufferToFill.buffer, input);
}



//==============================================================================

void MainComponent::paint(juce::Graphics& g)
{
		g.fillAll(PizzaColours::CreamDough);

	// 🍞 オーブンで焼けた外縁の影をうっすら描く
	auto bounds = getLocalBounds().toFloat();
	g.setGradientFill(juce::ColourGradient::vertical(
									PizzaColours::DeepOvenBrown.withAlpha(0.25f),
									0.0f,
									PizzaColours::CreamDough,
									(float)getHeight()));
	g.fillRect(bounds);
	// 🍅上部に赤グラデーション
	juce::Rectangle<float> topBar(0, 0, getWidth(), 60.0f);
	g.setGradientFill(juce::ColourGradient::horizontal(PizzaColours::TomatoRed.withAlpha(0.35f),0.0f,PizzaColours::CheeseYellow.withAlpha(0.15f),
		(float)getWidth()
		));
	g.fillRect(topBar);

	// 🍕 タイトルを焼印風に
	g.setColour(PizzaColours::DeepOvenBrown);
	g.setFont(juce::Font("Arial Rounded MT Bold", 28.0f, juce::Font::bold));
	g.drawText("PizzaLooper", topBar, juce::Justification::centred);

	// 🎛 セパレーターライン（ピザの切り目みたいに）
	g.setColour(PizzaColours::DeepOvenBrown.withAlpha(0.3f));
	g.drawLine(0, 65.0f, (float)getWidth(), 65.0f, 2.0f);

}

void MainComponent::resized() 
{
	auto area = getLocalBounds().reduced(15);

	// 🍕 ピザエリアを確保
	auto pizzaArea = area.removeFromTop(pizzaVisualArea);

	// 正円にするための調整
	auto pizzaSize = juce::jmin(pizzaArea.getWidth(), pizzaArea.getHeight()) * 0.8f; // 少し小さめに
	auto pizzaX = pizzaArea.getCentreX() - pizzaSize / 2.0f;
	auto pizzaY = pizzaArea.getY() + (pizzaArea.getHeight() - pizzaSize) / 2.0f;

	pizzaVisualizer.setBounds(pizzaX, pizzaY, pizzaSize, pizzaSize);

	// 🎛 トランスポートエリア
	auto transportArea = area.removeFromTop(70);
	transportPanel.setBounds(transportArea);
	// 🎚 トラック群
	int x = 0, y = 0;
	for (int i = 0; i < trackUIs.size(); i++)
	{
		int row = i / tracksPerRow;
		int col = i % tracksPerRow;
		x = col * (trackWidth + spacing);
		y = row * (trackHeight + spacing);

		trackUIs[i]->setBounds(area.getX() + x + spacing,
							   area.getY() + y + spacing,
							   trackWidth, trackHeight);
	}
}

//==============================================================================

void MainComponent::trackClicked(LooperTrackUi* clickedTrack)
{
	const bool wasSelected = clickedTrack->getIsSelected(); // 押す前の状態を記録

	// まず全トラックの選択を解除
	for (auto& t : trackUIs)
		t->setSelected(false);

	// もし前回選ばれてなかったら今回ONにする
	clickedTrack->setSelected(!wasSelected);

	// すべて再描画
	for (auto& t : trackUIs)
		t->repaint();

	if (clickedTrack->getIsSelected())
		DBG("🎯 Selected track ID: " << clickedTrack->getTrackId());
	else
		DBG("🚫 All tracks deselected");
}



void MainComponent::showDeviceSettings()
{
	auto* selector = new juce::AudioDeviceSelectorComponent(
															deviceManager,
															0, 2,   // min/max input
															0, 2,   // min/max output
															false, false,
															true, true
															);
	selector->setSize(520, 360);

	juce::DialogWindow::LaunchOptions opts;
	opts.dialogTitle = "Audio Device Settings";
	opts.content.setOwned(selector);
	opts.componentToCentreAround = this;
	opts.useNativeTitleBar = true;
	opts.escapeKeyTriggersCloseButton = true;
	opts.launchAsync();
}

void MainComponent::updateStateVisual()
{
	bool anyRecording = false;
	bool anyPlaying = false;
	bool selectedDuringPlay = false;

	for(auto& t : trackUIs)
	{
		switch (t->getState()) {
			case LooperTrackUi::TrackState::Recording:
				anyRecording = true;
				break;
			case LooperTrackUi::TrackState::Playing:
				anyPlaying = true;
				break;
			default:
				break;
		}
		if(t->getIsSelected() && anyPlaying)
			selectedDuringPlay = true;
	}
	// === 🎛 TransportPanel へ状態を通知 ===
	if (anyRecording)
	{
		transportPanel.setState(TransportPanel::State::Recording);
	}
	else if (anyPlaying && selectedDuringPlay)
	{
		transportPanel.setState(TransportPanel::State::Playing);
	}
	else if (anyPlaying)
	{
		transportPanel.setState(TransportPanel::State::Stopped);
	}
	else
	{
		transportPanel.setState(TransportPanel::State::Idle);
	}
//
//	if(anyRecording)
//	{
//		recordButton.setButtonText("Play");
//		recordButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgreen);
//	}
//	else if(anyPlaying && selectedDuringPlay)
//	{recordButton.setButtonText("Next");
//		recordButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkorange);
//	}else if(anyPlaying)
//	{recordButton.setButtonText("Playing");
//		recordButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
//	}else
//	{
//		recordButton.setButtonText("Record");
//		recordButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
//	}

	//DBG("UpdateVisual!!");
	repaint();
}


void MainComponent::timerCallback()
{
	const auto& tracks = looper.getTracks();

	bool anyRecording = anyTrackSatisfies(tracks, [](const auto& track){ return track.isRecording; });
	bool anyPlaying = anyTrackSatisfies(tracks, [](const auto& track){ return track.isPlaying; });

	//TrackUIの状態更新
	for (const auto& [id, data] : tracks)
	{
		if (id -1 >= trackUIs.size())
			continue;

		auto& trackUI = trackUIs[id - 1];
		auto newState = LooperTrackUi::TrackState::Idle;

		if (data.isRecording)
			newState = LooperTrackUi::TrackState::Recording;
		else if (data.isPlaying)
			newState = LooperTrackUi::TrackState::Playing;
		
		if (trackUI->getState() != newState)
		{
			trackUI->setState(newState);
			trackUI->repaint();
		}
	}
	

	//TransportPanelの状態更新
	bool hasRecorded = looper.hasRecordedTracks(); // 🆕 録音済みトラックがあるか確認

	// TransportPanelの状態更新
	if (anyRecording)
	{
		// 🔴 録音中
		transportPanel.setState(TransportPanel::State::Recording);
	}
	else if (anyPlaying)
	{
		// ▶️ 再生中
		transportPanel.setState(TransportPanel::State::Playing);
	}
	else if (hasRecorded)
	{
		// ⏹ 停止中だが再生可能なトラックがある
		transportPanel.setState(TransportPanel::State::Stopped);
	}
	else
	{
		// 🔘 何もない/アイドル状態
		transportPanel.setState(TransportPanel::State::Idle);
	}
}




//===========リスナーイベント=================

void MainComponent::onRecordingStarted(int trackID)
{
	//DBG("Main : Track" << trackID << "started !");

	for (auto& t : trackUIs)
	{
		util::safeUi([this, &t, trackID]{
			if (t->getTrackId() == trackID)
				t->setState(LooperTrackUi::TrackState::Recording);
		});
	}
}

void MainComponent::onRecordingStopped(int trackID)
{
	//DBG("EVENT !!! Main : Track " << trackID << " finished recording!" );

	for (auto& t : trackUIs)
	{
		util::safeUi([this, &t, trackID]{
			if (t->getTrackId() == trackID)
				t->setState(LooperTrackUi::TrackState::Playing);
		});
	}
}
