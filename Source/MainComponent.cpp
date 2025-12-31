#include "MainComponent.h"
#include "SettingsComponent.h"

//==============================================================================
MainComponent::MainComponent()
	: sharedTrigger(inputTap.getTriggerEvent()),
		looper(44100, 44100 * 10),
		transportPanel(looper),
        fxPanel(looper)
{
	// 設定ファイルを初期化
	juce::PropertiesFile::Options options;
	options.applicationName = "ORAS";
	options.filenameSuffix = ".settings";
	options.osxLibrarySubFolder = "Application Support";
	options.folderName = "ORAS";
	
	appProperties.reset(new juce::PropertiesFile(options));
	
	// 保存されたオーディオ設定を読み込み
	loadAudioDeviceSettings();
	deviceManager.addAudioCallback(&inputTap); // 入力だけTapする

	startTimerHz(30);

	// トラック初期化
	for (int i = 0; i < 8; ++i)
	{
		int newId = static_cast<int>(trackUIs.size() + 1);
		auto track = std::make_unique<LooperTrackUi>(newId, LooperTrackUi::TrackState::Idle);
		track->setListener(this);
		
		// フェーダー操作時のコールバック
		track->onGainChange = [this, newId](float gain)
		{
			looper.setTrackGain(newId, gain);
		};
		
		addAndMakeVisible(track.get());
		trackUIs.push_back(std::move(track));
		looper.addTrack(newId);
	}

	// ボタン類設定
	addAndMakeVisible(visualizer);
	addAndMakeVisible(transportPanel);
	addChildComponent(fxPanel); // Initially hidden

	transportPanel.onAction = [this](const juce::String& action)
	{
		if      (action == "REC")  {
			// Check if we are already in standby (or have tracks in standby)
			bool anyStandby = false;
			for(auto& t : trackUIs) {
				if(t->getState() == LooperTrackUi::TrackState::Standby) {
					anyStandby = true;
					break;
				}
			}

			if (anyStandby)
			{
				// 🔴 Force Start Recording (Signal to audio thread)
				forceRecordRequest = true;
			}
			else
			{
				// 🟡 Enter Standby mode
				isStandbyMode = true;
				for (auto& t : trackUIs)
				{
					if (t->getIsSelected() &&
						t->getState() == LooperTrackUi::TrackState::Idle)
					{
						t->setState(LooperTrackUi::TrackState::Standby);
					}
				}
				updateStateVisual();
			}
		}
		else if (action == "STOP_REC") {
			// スタンバイ解除
			isStandbyMode = false;
            
            if (looper.isAnyRecording())
            {
                int id = looper.getCurrentTrackId();
                looper.stopRecording(id);
                looper.startPlaying(id);
            }

			for (auto& t : trackUIs)
			{
				if (t->getState() == LooperTrackUi::TrackState::Standby)
				{
					t->setState(LooperTrackUi::TrackState::Idle);
				}
			}
            updateStateVisual();
		}
		else if (action == "PLAY")
        {
             if (looper.isAnyRecording()) {
                 looper.stopRecording(looper.getCurrentTrackId());
             }
             
             const auto& tracks = looper.getTracks();
             bool anyStarted = false;
             for (const auto& [id, data] : tracks) {
                 if (data.recordLength > 0) {
                     looper.startPlaying(id);
                     anyStarted = true;
                 }
             }
             if (!anyStarted) DBG("⚠️ No tracks to play");
        }
		else if (action == "STOP")   looper.stopAllTracks();
		else if (action == "UNDO")   looper.undoLastRecording();
		else if (action == "CLEAR") {
		looper.allClear();
        visualizer.clear(); // Reset visualizer
		
		// UI状態を完全にリセット
		isStandbyMode = false;
		selectedTrackId = 0;
		
		// 全トラックを初期状態に戻す
		for (auto& t : trackUIs) {
			t->setSelected(false);
			t->setState(LooperTrackUi::TrackState::Idle);
		}
		
		updateStateVisual();
	}
		else if (action == "SETUP")   showDeviceSettings();

	};
	transportPanel.onSettingsRequested = [this]()
	{
		showDeviceSettings();
	};
	transportPanel.onTestClick = [this]()
	{
		bool processed = false;
		// 選択されているトラックにテストクリックを生成
		for (auto& t : trackUIs)
		{
			if (t->getIsSelected())
			{
				looper.generateTestClick(t->getTrackId());
				t->setState(LooperTrackUi::TrackState::Playing);
				processed = true;
				break;
			}
		}
		
		// 選択されていない場合はトラック1に
		if (!processed)
		{
			looper.generateTestClick(1);
			if (!trackUIs.empty())
				trackUIs[0]->setState(LooperTrackUi::TrackState::Playing);
		}
		updateStateVisual();
	};




	transportPanel.onToggleTracks = [this]()
	{
		areTracksVisible = !areTracksVisible;
		
		// Button Text Update in TransportPanel
		if (areTracksVisible)
			transportPanel.setVisualModeButtonText("VISUAL MODE"); // Click to hide tracks
		else
			transportPanel.setVisualModeButtonText("SHOW TRACKS"); // Click to show tracks
			
		// Visibility Update
		if (!isFXMode) {
			for (auto& t : trackUIs)
				t->setVisible(areTracksVisible);
		} else {
			// Exit FX mode when toggling visual mode
			isFXMode = false;
			fxPanel.setVisible(false);
			for (auto& t : trackUIs)
				t->setVisible(areTracksVisible);
		}
			
		resized();
		repaint();
	};

	transportPanel.onShowFX = [this]()
	{
		// Toggle FX Mode
		bool targetState = !isFXMode;
		
		if (targetState) // Trying to enable FX Mode
		{
			// Find selected track
			int selectedId = -1;
			for(auto& t : trackUIs) {
				if(t->getIsSelected()) {
					selectedId = t->getTrackId();
					break;
				}
			}
			
			if (selectedId != -1) {
				isFXMode = true;
				fxPanel.setTargetTrackId(selectedId);
				fxPanel.setVisible(true);
				
				// Hide tracks
				for(auto& t : trackUIs) t->setVisible(false);
				
				DBG("🪄 Entered FX Mode for Track " << selectedId);
			} else {
				// No track selected: Maybe flash the button or show a warning?
				DBG("⚠️ Cannot enter FX Mode: No track selected");
				// Force disable
				isFXMode = false;
			}
		}
		else // Disable FX Mode
		{
			isFXMode = false;
			fxPanel.setVisible(false);
			
			// Restore track visibility based on areTracksVisible
			if (areTracksVisible) {
				for(auto& t : trackUIs) t->setVisible(true);
			}
			
			DBG("🔙 Exited FX Mode");
		}
		
		resized();
	};

	setSize(760, 800);


	//ルーパーからのリスナーイベントを受け取る
	looper.addListener(this);

    // Initialize Global Stars
    for (int i = 0; i < 200; ++i)
    {
        Star s;
        s.x = juce::Random::getSystemRandom().nextFloat();
        s.y = juce::Random::getSystemRandom().nextFloat();
        s.size = juce::Random::getSystemRandom().nextFloat() * 2.5f + 0.5f;
        s.brightness = juce::Random::getSystemRandom().nextFloat();
        s.speed = juce::Random::getSystemRandom().nextFloat() * 0.05f + 0.02f;
        stars.push_back(s);
    }
}

MainComponent::~MainComponent()
{
	saveAudioDeviceSettings();
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
		isStandbyMode = false; // 録音開始でスタンバイ解除)
		
		for (auto& t : trackUIs)
		{
			if (t->getIsSelected() && 
				t->getState() == LooperTrackUi::TrackState::Recording)  // ✅ Standbyを除外
			{
				anyRecording = true;
				break;
			}
		}

		if (!anyRecording)
		{
			// 🟢 新規録音を開始
            // Prepare lookback data from buffer
            juce::AudioBuffer<float> lookback;
            inputTap.getManager().getLookbackData(lookback);

			for (auto& t : trackUIs)
			{
				if (t->getIsSelected())
				{
					looper.startRecordingWithLookback(t->getTrackId(), lookback);

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
    // 🔥 Force Record Trigger (Manual)
    if (forceRecordRequest.exchange(false))
    {
        isStandbyMode = false;
        
        for (auto& t : trackUIs)
        {
            if (t->getState() == LooperTrackUi::TrackState::Standby)
            {
                looper.startRecording(t->getTrackId());
                
                juce::MessageManager::callAsync([this, &t]()
                {
                    t->setState(LooperTrackUi::TrackState::Recording);
                });
            }
        }
    }

	// 🌀 LooperAudio の処理は常に実行
	looper.processBlock(*bufferToFill.buffer, input);

	// 📊 ビジュアライザー更新 (入力と再生のミックスを渡す)
	visualizer.pushBuffer(*bufferToFill.buffer);
}



//==============================================================================

void MainComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto centre = bounds.getCentre();

    // --- Space Background (Global) ---
    // Deep space gradient
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff050510), centre.x, centre.y,
                                           juce::Colour(0xff000000), 0.0f, 0.0f, true));
    g.fillAll(); // Fill entire component
    
    // Subtle Nebula/Glow radiating from center
    g.setGradientFill(juce::ColourGradient(ThemeColours::NeonCyan.withAlpha(0.08f), centre.x, centre.y, 
                                           juce::Colours::transparentBlack, centre.x + bounds.getWidth()*0.6f, centre.y + bounds.getHeight()*0.6f, true));
    g.fillAll();

    // Draw Global Stars
    for (const auto& star : stars)
    {
        float x = star.x * bounds.getWidth();
        float y = star.y * bounds.getHeight();
        
        // 瞬き
        float alpha = juce::jlimit(0.0f, 1.0f, 0.2f + 0.8f * star.brightness); 
        g.setColour(juce::Colours::white.withAlpha(alpha));
        g.fillEllipse(x, y, star.size, star.size);
    }
    

    // Top Header with Neon Accent
    juce::Rectangle<float> topBar(0, 0, getWidth(), 40.0f);
    // Darker header background for readability
    g.setColour(juce::Colours::black.withAlpha(0.4f)); 
    g.fillRect(topBar);
    
    g.setGradientFill(juce::ColourGradient::horizontal(
        ThemeColours::NeonCyan.withAlpha(0.1f), 0.0f,
        ThemeColours::NeonMagenta.withAlpha(0.1f), (float)getWidth()));
    g.fillRect(topBar);

    // --- Title Logo Rendering ---
    juce::String titleText = "ORAS";
    float titleFontSize = 32.0f;
    
    // システムフォントを使用 (Futura または Arial)
    juce::Font titleFont(juce::FontOptions("Futura", titleFontSize, juce::Font::bold));
    if (titleFont.getTypefaceName() == "Sans-Serif") 
        titleFont = juce::Font(juce::FontOptions("Arial", titleFontSize, juce::Font::bold));
        
    titleFont.setHeight(titleFontSize);
    titleFont.setBold(true);

    juce::GlyphArrangement ga;
    ga.addLineOfText(titleFont, titleText, 0, 0);
    juce::Path titlePath;
    ga.createPath(titlePath);
    auto titlePathBounds = titlePath.getBounds();
    
    // Center the path in the top bar
    titlePath.applyTransform(juce::AffineTransform::translation(centre.x - titlePathBounds.getCentreX(), 
                                                                topBar.getCentreY() - titlePathBounds.getCentreY()));

    // 1. Neon Glow Layers (Soft Blur)
    for (int glow = 5; glow >= 1; --glow)
    {
        g.setColour(ThemeColours::NeonCyan.withAlpha(0.12f / (float)glow));
        g.strokePath(titlePath, juce::PathStrokeType((float)glow * 2.5f));
    }

    // 2. Main Title Gradient Fill
    juce::ColourGradient titleGrad(ThemeColours::NeonCyan, centre.x - 50.0f, 0.0f,
                                   ThemeColours::NeonMagenta, centre.x + 50.0f, 0.0f, false);
    g.setGradientFill(titleGrad);
    g.fillPath(titlePath);

    // 3. Bright Inner Core
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.strokePath(titlePath, juce::PathStrokeType(0.5f));

    // --- Futuristic Accents ---
    float accentW = 80.0f;
    float accentH = 24.0f;
    juce::Rectangle<float> accentRect(centre.x - accentW, topBar.getCentreY() - accentH*0.5f, accentW * 2.0f, accentH);
    
    g.setColour(ThemeColours::NeonCyan.withAlpha(0.5f));
    // Corner Brackets
    float bracketSize = 10.0f;
    // Top Left
    g.drawLine(accentRect.getX(), accentRect.getY(), accentRect.getX() + bracketSize, accentRect.getY(), 1.5f);
    g.drawLine(accentRect.getX(), accentRect.getY(), accentRect.getX(), accentRect.getY() + bracketSize, 1.5f);
    // Top Right
    g.drawLine(accentRect.getRight(), accentRect.getY(), accentRect.getRight() - bracketSize, accentRect.getY(), 1.5f);
    g.drawLine(accentRect.getRight(), accentRect.getY(), accentRect.getRight(), accentRect.getY() + bracketSize, 1.5f);
    // Bottom Left
    g.drawLine(accentRect.getX(), accentRect.getBottom(), accentRect.getX() + bracketSize, accentRect.getBottom(), 1.5f);
    g.drawLine(accentRect.getX(), accentRect.getBottom(), accentRect.getX(), accentRect.getBottom() - bracketSize, 1.5f);
    // Bottom Right
    g.drawLine(accentRect.getRight(), accentRect.getBottom(), accentRect.getRight() - bracketSize, accentRect.getBottom(), 1.5f);
    g.drawLine(accentRect.getRight(), accentRect.getBottom(), accentRect.getRight(), accentRect.getBottom() - bracketSize, 1.5f);

    // Subtle decorative scanline in header
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    for (float lx = 0; lx < getWidth(); lx += 4.0f)
        g.drawLine(lx, 0.0f, lx, 40.0f, 0.5f);

    // Top border line
    g.setColour(ThemeColours::NeonCyan.withAlpha(0.6f));
    g.drawLine(0, 40.0f, (float)getWidth(), 40.0f, 2.0f);

    // --- Track Area Background ---
    if (areTracksVisible)
    {
        // Layout: 30 (header spacing) + headerVisualArea + 70 (transport) = Start of tracks
        // Visualizerの実際の高さや隙間(spacing)も考慮
        // resized()のロジック:
        // area.removeFromTop(30);
        // area.removeFromTop(headerVisualArea);
        // area.removeFromTop(70);
        // 残りがトラック領域
        
        float trackStartY = 30.0f + (float)headerVisualArea + 70.0f;
        // マージン分(15px)もあるので、絶対座標的には +15 startY
        trackStartY += 15.0f; // Top margin used in resized()
        
        // 微調整: 背景は少し広めに描画しても良いが、ビジュアライザを隠さないように
        
        juce::Rectangle<float> trackArea(0, trackStartY, (float)getWidth(), (float)getHeight() - trackStartY);
        
        // Darken the track area significantly to make UI controls stand out
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.fillRect(trackArea);
        
        // Add a separator line
        g.setColour(ThemeColours::NeonCyan.withAlpha(0.3f));
        g.drawLine(0, trackStartY, (float)getWidth(), trackStartY, 1.0f);
    }
}

void MainComponent::resized() 
{
	auto area = getLocalBounds().reduced(15);
	
// ⬇️ Top margin for layout (skip past the 40px header bar)
	area.removeFromTop(30);

	// トラック表示/非表示によるレイアウト調整
    if (areTracksVisible)
    {
        // --- 通常モード（トラック表示） ---
        
        // Visual Area (Upper Part)
        auto visualArea = area.removeFromTop(headerVisualArea);
        visualizer.setBounds(visualArea.reduced(10));
        
        // Toggle Button Removed (Moved to TransportPanel)
        
        // 🎛 トランスポートエリア
        auto transportArea = area.removeFromTop(70);
        transportPanel.setBounds(transportArea);
        
        // 🎚 トラック群 または FXパネル
        if (isFXMode)
        {
            // Show FX Panel instead of tracks
            fxPanel.setBounds(area);
        }
        else
        {
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
    }
    else
    {
        // --- 全画面ビジュアライザモード（トラック非表示） ---
        
        // トランスポートパネルだけ下部に残す（オプション、今回はシンプルに下に配置）
        auto transportArea = area.removeFromBottom(70);
        transportPanel.setBounds(transportArea);
        
        // 残りのエリア全部をビジュアライザに
        visualizer.setBounds(area.reduced(10));
        
        // Toggle Button Removed (Moved to TransportPanel)
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
	{
		DBG("🎯 Selected track ID: " << clickedTrack->getTrackId());
		
		// If FX mode is active, switch FX panel to this new track
		if (isFXMode) {
			fxPanel.setTargetTrackId(clickedTrack->getTrackId());
		}
	}
	else
	{
		DBG("🚫 All tracks deselected");
		// If deselecting everything, maybe exit FX mode? 
		// For now, let's keep it simple: if you deselect, you might want to close FX mode manually or it stays open but "No track selected"
		// Better UX: If in FX mode and all tracks deselected -> Exit FX mode
		if (isFXMode) {
			isFXMode = false;
			fxPanel.setVisible(false);
			if (areTracksVisible) {
				for(auto& t : trackUIs) t->setVisible(true);
			}
			resized();
		}
	}
}



void MainComponent::showDeviceSettings()
{
	auto* settingsComp = new SettingsComponent(deviceManager, inputTap.getManager());
    settingsComp->setSize(550, 450);

	juce::DialogWindow::LaunchOptions opts;
	opts.dialogTitle = "Audio Device & Trigger Settings";
	opts.content.setOwned(settingsComp);
	opts.componentToCentreAround = this;
	opts.useNativeTitleBar = true;
	opts.escapeKeyTriggersCloseButton = true;
	opts.launchAsync();
}

void MainComponent::updateStateVisual()
{
	bool anyRecording = false;
	bool anyPlaying = false;
    bool anyStandby = false;
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
            case LooperTrackUi::TrackState::Standby:
                anyStandby = true;
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
    else if (anyStandby)
    {
        transportPanel.setState(TransportPanel::State::Standby);
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

    // Global Star Animation Update
    for (auto& s : stars)
    {
        s.brightness += s.speed;
        if (s.brightness > 1.0f || s.brightness < 0.0f)
        {
            s.speed = -s.speed;
        }
    }

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
		
        // 🟡 Standby状態はLooper側にはないので、UI側で維持する
        if (trackUI->getState() == LooperTrackUi::TrackState::Standby && newState == LooperTrackUi::TrackState::Idle)
        {
            // Standbyのまま
            continue;
        }

		if (trackUI->getState() != newState)
		{
			trackUI->setState(newState);
			trackUI->repaint();
		}
	}
	
    // Standby状態のトラックがあるか確認
    bool anyStandby = false;
    for (auto& t : trackUIs)
    {
        if (t->getState() == LooperTrackUi::TrackState::Standby)
        {
            anyStandby = true;
            // break; // メーター更新のためbreakしない
        }
        
        // メーター更新
        // 選択されたトラック（入力待ち状態）には入力レベルを表示
        if (t->getIsSelected() && 
            (t->getState() == LooperTrackUi::TrackState::Idle || 
             t->getState() == LooperTrackUi::TrackState::Standby))
        {
            t->setLevel(inputTap.getInputRMS());
        }
        else
        {
            // それ以外のトラックは再生中のレベルを表示
            t->setLevel(looper.getTrackRMS(t->getTrackId()));
        }
    }

	//TransportPanelの状態更新
	bool hasRecorded = looper.hasRecordedTracks(); // 🆕 録音済みトラックがあるか確認

    // 🌀 ビジュアライザ：プレイヘッド位置更新
    if (anyPlaying)
    {
        visualizer.setPlayHeadPosition(looper.getMasterNormalizedPosition());
    }
    else
    {
        //visualizer.setPlayHeadPosition(-1.0f); // 停止中は非表示、または最後の位置で止めるか
    }

	// TransportPanelの状態更新
	if (anyRecording)
	{
		// 🔴 録音中
		transportPanel.setState(TransportPanel::State::Recording);
	}
    else if (anyStandby)
    {
        // 🟡 待機中
        transportPanel.setState(TransportPanel::State::Standby);
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
    // UIスレッドで安全に一括更新
    util::safeUi([this, trackID]()
    {
        for (auto& t : trackUIs)
        {
            // 1. 録音が終わったトラックを再生状態にする
            if (t->getTrackId() == trackID)
                t->setState(LooperTrackUi::TrackState::Playing);

            // 2. 全トラックの選択を解除！
            t->setSelected(false);
        }

        // 3. 選択IDの記憶もリセット
        selectedTrackId = 0; 
        
        // 4. トランスポートパネルなどの見た目を更新
        updateStateVisual();
        
        // 5. 🌊 ビジュアライザに波形を送る
        if (auto* buffer = looper.getTrackBuffer(trackID))
        {
            visualizer.addWaveform(trackID, *buffer, 
                                   looper.getTrackLength(trackID), 
                                   looper.getMasterLoopLength(),
                                   looper.getTrackRecordStart(trackID),
                                   looper.getMasterStartSample());
        }
    });
}

//==============================================================================
// 設定保存・読み込み
//==============================================================================

void MainComponent::saveAudioDeviceSettings()
{
	if (appProperties != nullptr)
	{
		auto xml = deviceManager.createStateXml();
		if (xml != nullptr)
		{
			appProperties->setValue("audioDeviceState", xml.get());
			appProperties->setValue("triggerThreshold", inputTap.getManager().getConfig().userThreshold);
			appProperties->saveIfNeeded();
			DBG("🔧 Audio device settings & Trigger Threshold saved");
		}
	}
}

void MainComponent::loadAudioDeviceSettings()
{
	// まず基本的な初期化（デフォルト設定）
	setAudioChannels(2, 2);
	
	// 保存された設定があれば復元
	if (appProperties != nullptr)
	{
		auto xmlState = appProperties->getXmlValue("audioDeviceState");
		if (xmlState != nullptr)
		{
			deviceManager.initialise(2, 2, xmlState.get(), true);
			DBG("✅ Audio device settings restored from file");
		}
		else
		{
			DBG("ℹ️ No saved audio settings found, using defaults");
		}

        // Restore Trigger Threshold
        double savedThresh = appProperties->getDoubleValue("triggerThreshold", 0.1);
        auto conf = inputTap.getManager().getConfig();
        conf.userThreshold = (float)savedThresh;
        inputTap.getManager().setConfig(conf);
        DBG("✅ Trigger Threshold restored: " << savedThresh);
	}
}