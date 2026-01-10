#include "MainComponent.h"
#include "SettingsComponent.h"

//==============================================================================
MainComponent::MainComponent()
	: sharedTrigger(inputTap.getTriggerEvent()),
		looper(44100, 44100 * 30),  // 30秒バッファ
		transportPanel(looper),
        fxPanel(looper)
{
	// 設定ファイルを初期化
	juce::PropertiesFile::Options options;
	options.applicationName = "SAROS";
	options.filenameSuffix = ".settings";
	options.osxLibrarySubFolder = "Application Support";
	options.folderName = "SAROS";
	
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
            // MIDI Learnモード時は値を変更せず、元の値に戻す（UIロック）
            if (midiLearnManager.isLearnModeActive())
            {
                if (lastGainValues.count(newId) && newId <= trackUIs.size())
                {
                    trackUIs[newId - 1]->setGainValue(lastGainValues[newId]);
                }
                return;
            }
            looper.setTrackGain(newId, gain);
        };
        
        // ドラッグ開始時のコールバック（MIDI Learn用）
        track->onGainSliderDragStart = [this, newId]()
        {
            if (midiLearnManager.isLearnModeActive())
            {
                // 現在値を保存
                if (newId <= trackUIs.size())
                    lastGainValues[newId] = trackUIs[newId - 1]->getGain();
                
                juce::String controlId = "track_" + juce::String(newId) + "_gain";
                midiLearnManager.setLearnTarget(controlId);
                DBG("MIDI Learn: Waiting for input - " << controlId);
            }
        };
        
        // 倍率変更時のコールバック
        track->onLoopMultiplierChange = [this, newId](float multiplier)
        {
            looper.setTrackLoopMultiplier(newId, multiplier);
            // ビジュアライザの内部状態も更新
            visualizer.setTrackMultiplier(newId, multiplier);
            
            // 全トラックの最大倍率を計算（最長トラック基準）
            float maxMult = 1.0f;
            
            // まず自分自身
            if (multiplier > maxMult) maxMult = multiplier;
            
            // 他のトラック
            for (auto& t : trackUIs)
            {
                if (t->getTrackId() != newId)
                {
                   float m = t->getLoopMultiplier();
                   if (m > maxMult) maxMult = m;
                }
            }
            
            visualizer.setMaxMultiplier(maxMult);
        };
		
		addAndMakeVisible(track.get());
		trackUIs.push_back(std::move(track));
		looper.addTrack(newId);
	}

	// ボタン類設定
	addAndMakeVisible(visualizer);
	addAndMakeVisible(transportPanel);
	addChildComponent(fxPanel); // Initially hidden
	
	// FXパネルからのトラック選択コールバック
	fxPanel.onTrackSelected = [this](int trackId) {
		// 全トラックの選択を解除
		for (auto& t : trackUIs)
			t->setSelected(false);
		
		// 対応するトラックを選択
		if (trackId >= 1 && trackId <= static_cast<int>(trackUIs.size()))
			trackUIs[trackId - 1]->setSelected(true);
		
		DBG("🎯 FX Panel selected track ID: " << trackId);
	};

	transportPanel.onAction = [this](const juce::String& action)
	{
		if      (action == "REC")  {
			// 🔄 トグル動作：録音中なら停止
			if (looper.isAnyRecording())
			{
				int id = looper.getCurrentTrackId();
				looper.stopRecording(id);
				looper.startPlaying(id);
				updateStateVisual();
				return;
			}
			
			// 選択されているIdleトラックがあるかチェック
			bool hasSelectedIdle = false;
			for(auto& t : trackUIs) {
				if(t->getIsSelected() && t->getState() == LooperTrackUi::TrackState::Idle) {
					hasSelectedIdle = true;
					break;
				}
			}
			
			// Standby中のトラックがあるかチェック
			bool anyStandby = false;
			for(auto& t : trackUIs) {
				if(t->getState() == LooperTrackUi::TrackState::Standby) {
					anyStandby = true;
					break;
				}
			}

			if (anyStandby || hasSelectedIdle)
			{
				// 🔴 選択中のIdleトラックをStandbyに変更してから即座に録音開始
				if (hasSelectedIdle) {
					for (auto& t : trackUIs) {
						if (t->getIsSelected() && t->getState() == LooperTrackUi::TrackState::Idle) {
							t->setState(LooperTrackUi::TrackState::Standby);
						}
					}
				}
				forceRecordRequest = true;
			}
			else
			{
				// 🟡 トラックが選択されていない場合は何もしない
				DBG("⚠️ No track selected for recording");
			}
		}
		else if (action == "STOP_REC") {
			// スタンバイ解除（録音待機をキャンセル）
			isStandbyMode = false;
			
			// Auto-Arm有効時は録音停止しても継続（onRecordingStoppedで次トラックへ）
			// Auto-Arm無効時のみスタンバイをIdleに戻す
			if (!isAutoArmEnabled)
			{
				for (auto& t : trackUIs)
				{
					if (t->getState() == LooperTrackUi::TrackState::Standby)
					{
						t->setState(LooperTrackUi::TrackState::Idle);
					}
				}
			}
            
            if (looper.isAnyRecording())
            {
                int id = looper.getCurrentTrackId();
                looper.stopRecording(id);
                looper.startPlaying(id);
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
		else if (action == "STOP")
		{
			looper.stopAllTracks();
			
			// Auto-Arm状態とStandby状態をリセット
			isAutoArmEnabled = false;
			autoArmButton.setToggleState(false, juce::dontSendNotification);
			nextTargetTrackId = -1;
			isStandbyMode = false;
			
			for (auto& t : trackUIs)
			{
				if (t->getState() == LooperTrackUi::TrackState::Standby)
					t->setState(LooperTrackUi::TrackState::Idle);
			}
			updateStateVisual();
		}
		else if (action == "UNDO") {
			int undoneTrackId = looper.undoLastRecording();
			if (undoneTrackId > 0)
			{
				visualizer.removeWaveform(undoneTrackId);
				// UIの状態もIdleに戻す
				for (auto& t : trackUIs)
				{
					if (t->getTrackId() == undoneTrackId)
						t->setState(LooperTrackUi::TrackState::Idle);
				}
			}
		}
		else if (action == "CLEAR") {
        looper.allClear();
        // Clear時にVisualizerの倍率もリセット
        visualizer.setMaxMultiplier(1.0f);
        visualizer.clear(); // 保持している波形データもクリア
        
        // 🎛 FXも全リセット
		for (int track = 1; track <= 8; ++track) {
		    looper.setTrackFilterEnabled(track, false);
		    looper.setTrackDelayEnabled(track, false);
		    looper.setTrackReverbEnabled(track, false);
		    looper.setTrackBeatRepeatActive(track, false);
		}
		
		// UI状態を完全にリセット
		isStandbyMode = false;
		selectedTrackId = 0;
		
		// Auto-Arm もリセット
		isAutoArmEnabled = false;
		autoArmButton.setToggleState(false, juce::dontSendNotification);
		nextTargetTrackId = -1;
		
		// 🔊 トリガーイベントもリセット（自動検知録音が再び機能するように）
		inputTap.resetTriggerEvent();
        lastTriggerTime = 0; // タイマーもリセット
		
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
				
				// トラックはFXパネルと並べて表示するため非表示にしない
				
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
			// visual mode (areTracksVisible = false) の場合はトラックを非表示のままにする
			if (areTracksVisible) {
				for(auto& t : trackUIs) t->setVisible(true);
			}
			// Note: resized()は下で呼ばれるのでレイアウトは更新される
			
			DBG("🔙 Exited FX Mode");
		}
		
		resized();
	};

	setSize(710, 750);


	//ルーパーからのリスナーイベントを受け取る

	// Auto-Arm ボタンの初期化
	autoArmButton.setButtonText("AUTO-ARM");
	autoArmButton.setClickingTogglesState(true);
	autoArmButton.onClick = [this]()
	{
		// MIDI Learnモードチェック
		if (midiLearnManager.isLearnModeActive())
		{
			// クリックでトグルしてしまうので元に戻す
			autoArmButton.setToggleState(!autoArmButton.getToggleState(), juce::dontSendNotification);
			
			midiLearnManager.setLearnTarget("main_auto_arm");
			DBG("MIDI Learn: Waiting for input - main_auto_arm");
			return;
		}

		isAutoArmEnabled = autoArmButton.getToggleState();
		DBG("🔗 Auto-Arm " << (isAutoArmEnabled ? "ON" : "OFF"));
		
		if (!isAutoArmEnabled)
		{
			// OFFにした時は選択とStandby状態をクリア
			nextTargetTrackId = -1;
			isStandbyMode = false;
			
			for (auto& t : trackUIs)
			{
				if (t->getState() == LooperTrackUi::TrackState::Standby)
					t->setState(LooperTrackUi::TrackState::Idle);
				t->setSelected(false);
			}
			selectedTrackId = 0;
			selectedTrack = nullptr;
		}
		
		updateNextTargetPreview();
	};
	addAndMakeVisible(autoArmButton);
	
	// MIDI Learn ボタンの初期化
	midiLearnButton.setButtonText("MIDI LEARN");
	midiLearnButton.setClickingTogglesState(true);
	midiLearnButton.setColour(juce::TextButton::buttonOnColourId, ThemeColours::NeonMagenta);
	midiLearnButton.onClick = [this]()
	{
		midiLearnManager.setLearnMode(midiLearnButton.getToggleState());
		DBG("🎹 MIDI Learn " << (midiLearnButton.getToggleState() ? "ON" : "OFF"));
	};
	addAndMakeVisible(midiLearnButton);
	
	// TransportPanelにMIDI LearnManagerを設定
	transportPanel.setMidiLearnManager(&midiLearnManager);
	
	// FXPanelにMIDI LearnManagerを設定
	fxPanel.setMidiLearnManager(&midiLearnManager);
	
	midiLearnManager.addListener(this);
	
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
    
    // キーボード入力を受け付けるように設定
    setWantsKeyboardFocus(true);
}

MainComponent::~MainComponent()
{
	midiLearnManager.removeListener(this);
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
		isStandbyMode = false; // 録音開始でスタンバイ解除
		
		// 録音状態チェック...
		for (auto& t : trackUIs)
		{
			if (t->getIsSelected())
			{
				int trackId = t->getTrackId();
				const auto& tracks = looper.getTracks();
				if (auto it = tracks.find(trackId); it != tracks.end() && it->second.isRecording)
				{
					anyRecording = true;
					break;
				}
			}
		}

		if (!anyRecording)
		{
			// 録音開始を試みる
			bool startSuccess = false;
			
            // Prepare lookback data from buffer
            juce::AudioBuffer<float> lookback;
            inputTap.getManager().getLookbackData(lookback);
            
			// トラックが選択されているか確認
			bool hasSelectedTrack = false;
			for (auto& t : trackUIs)
			{
				if (t->getIsSelected())
				{
					hasSelectedTrack = true;
					
					// 🔒 録音中フラグを立てる（鎮火抑制）
					inputTap.getManager().setRecordingActive(true);
					
					looper.startRecordingWithLookback(t->getTrackId(), lookback);

					juce::MessageManager::callAsync([this, &trig, &t]()
					{t->setState(LooperTrackUi::TrackState::Recording);
					});
					
					startSuccess = true;
				}
			}
			
			if (startSuccess)
			{
				// 録音成功したので即リセット
				trig.triggerd = false;
				trig.sampleInBlock = -1;
				trig.absIndex = -1;
				lastTriggerTime = 0;
			}
			else
			{
				// 録音対象がない場合、少しの間トリガーを保持する（Auto-Arm遷移中などの対策）
				auto now = juce::Time::currentTimeMillis();
				if (lastTriggerTime == 0) lastTriggerTime = now;
				
				// 500ms経過しても録音開始できなければ諦めてリセット
				if (now - lastTriggerTime > 500)
				{
					DBG("⏰ Trigger expired without recording target");
					trig.triggerd = false;
					trig.sampleInBlock = -1;
					trig.absIndex = -1;
					lastTriggerTime = 0;
				}
			}
		}
		else
		{
			// 既に録音中なら鎮火
			trig.triggerd = false;
			trig.sampleInBlock = -1;
			trig.absIndex = -1;
			lastTriggerTime = 0;
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
    juce::String titleText = "SAROS";
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
        // Layout: 30 (header spacing) + headerVisualArea
        // Visualizerの描画開始位置を「バッファ先頭＝3時」から逆算して調整
        // Offset = RecordStart - MasterStart = -WritePos となるように設定
        // StartAngle = -WritePos (時計回りに戻すことで、音のあるWritePos地点を3時に合わせる)
        // This section seems to be a misplaced comment/code snippet.
        // The original code had `area.removeFromTop(70);` here, which is part of `resized()`.
        // Assuming the intent was to add the `visualizer.setMaxMultiplier(1.0f)` and
        // the `recordStartSample` change is for `LooperAudio` (not in this file).
        // I will add `visualizer.setMaxMultiplier(1.0f)` to the constructor or a suitable init method if available,
        // but since the instruction only provides a diff for `paint` and `resized`,
        // and the `recordStartSample` change is clearly out of context for `paint`,
        // I will only apply the `visualizer.setMaxMultiplier(1.0f)` if a suitable place is found,
        // and ignore the `recordStartSample` change as it's not in this file or context.
        // Given the diff structure, it seems the user intended to replace a line in a comment.
        // I will restore the original comment structure and only apply the `visualizer.setMaxMultiplier(1.0f)`
        // if a clear insertion point is provided or implied.
        // The provided diff is problematic. I will assume the user wants to keep the original comment structure
        // and the `track.writePosition` etc. lines are not meant to be inserted here.
        // The instruction "MainComponentにvisualizer.setMaxMultiplier(1.0f)を追加" is separate.
        // I will add `visualizer.setMaxMultiplier(1.0f)` in the `MainComponent` constructor.
        // The `recordStartSample` change is for `LooperAudio` and cannot be applied here.

        // The original line was `area.removeFromTop(70);` which is in `resized()`, not `paint()`.
        // This diff snippet is highly confusing. I will revert the comment to its original state
        // and assume the `track.writePosition` lines are not meant to be here.
        // The instruction "LooperAudioのrecordStartSample計算を変更" cannot be fulfilled in this file.
        // I will only apply the `visualizer.setMaxMultiplier(1.0f)` change.

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
	DBG("📐 Window size: " << getWidth() << " x " << getHeight());
	auto area = getLocalBounds().reduced(15);
	
	// MIDI Learn と Auto-Arm ボタンをヘッダー部の右上に配置
	int buttonWidth = 100;
	int midiLearnButtonWidth = 130;  // MIDI Learnボタンは少し幅広く
	int buttonHeight = 30;
	int margin = 15;
	int spacing = 5;
	
	// Auto-Arm ボタン（一番右）
	autoArmButton.setBounds(getWidth() - buttonWidth - margin, 5, buttonWidth, buttonHeight);
	
	// MIDI Learn ボタン（Auto-Armの左）
	midiLearnButton.setBounds(getWidth() - buttonWidth - midiLearnButtonWidth - margin - spacing, 5, 
	                          midiLearnButtonWidth, buttonHeight);
	
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
            // まずトラックを通常配置
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
                trackUIs[i]->setVisible(true);
            }
            
            // FXパネルをフェーダー/メーター部分（トラック選択ボタンの下）にオーバーレイ
            // trackWidth = 80（正方形の選択ボタン）、その下がフェーダー部分
            int buttonSize = trackWidth;  // 正方形のボタン部分
            int fxPanelTop = area.getY() + buttonSize + 15;  // ボタン + gap
            auto fxArea = juce::Rectangle<int>(
                area.getX(),
                fxPanelTop,
                area.getWidth(),
                area.getHeight() - buttonSize - 15
            );
            fxPanel.setBounds(fxArea);
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
        
        // トランスポートパネルだけ下部に残す
        auto transportArea = area.removeFromBottom(70);
        transportPanel.setBounds(transportArea);
        
        // FXモードの場合はFXパネルを表示、そうでなければビジュアライザを全画面表示
        if (isFXMode)
        {
            fxPanel.setBounds(area);
            // ビジュアライザは隠す（または小さく表示する場合はここで調整）
        }
        else
        {
            // 残りのエリア全部をビジュアライザに
            visualizer.setBounds(area.reduced(10));
        }
        
        // Toggle Button Removed (Moved to TransportPanel)
    }
}

//==============================================================================

void MainComponent::trackClicked(LooperTrackUi* clickedTrack)
{
	// MIDI Learnモードの場合、学習対象として設定
	if (midiLearnManager.isLearnModeActive())
	{
		juce::String controlId = "track_select_" + juce::String(clickedTrack->getTrackId());
		midiLearnManager.setLearnTarget(controlId);
		DBG("MIDI Learn: Waiting for input - " + controlId);
		return;
	}

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
	auto* settingsComp = new SettingsComponent(deviceManager, inputTap.getManager(), 
	                                           midiLearnManager, keyboardMappingManager);
    settingsComp->setSize(600, 700);

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
	else if (anyPlaying)
	{
		// 再生中は常にSTOPボタンを表示（Auto-ArmのStandbyより優先）
		transportPanel.setState(TransportPanel::State::Playing);
	}
    else if (anyStandby)
    {
        transportPanel.setState(TransportPanel::State::Standby);
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
    // 🔓 録音中フラグを解除（鎮火許可）
    inputTap.getManager().setRecordingActive(false);
    
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
            // 録音開始位置とマスター開始位置から、正しい描画オフセットを計算
            visualizer.addWaveform(trackID, *buffer, 
                                   looper.getTrackLength(trackID), 
                                   looper.getMasterLoopLength(),
                                   looper.getTrackRecordStart(trackID), // 正しいrecordStart
                                   looper.getMasterStartSample()        // 正しいmasterStart
                                   );
        }

        // 6. 🔗 Auto-Arm: 次の空きトラックを自動で待機状態に
        if (isAutoArmEnabled)
        {
            int nextTrack = findNextEmptyTrack(trackID);
            if (nextTrack != -1)
            {
                selectedTrackId = nextTrack;
                selectedTrack = trackUIs[nextTrack - 1].get();
                trackUIs[nextTrack - 1]->setSelected(true);
                isStandbyMode = true;
                trackUIs[nextTrack - 1]->setState(LooperTrackUi::TrackState::Standby);
                nextTargetTrackId = findNextEmptyTrack(nextTrack);
                DBG("🔗 Auto-Arm: トラック " << nextTrack << " を待機状態に");
            }
            else
            {
                isAutoArmEnabled = false;
                autoArmButton.setToggleState(false, juce::dontSendNotification);
                nextTargetTrackId = -1;
                DBG("🔗 Auto-Arm: 空きトラックなし、自動終了");
            }
            // Auto-Arm設定後に状態を再更新（再生中ならSTOPボタン表示）
            updateStateVisual();
        }
        else
        {
            nextTargetTrackId = -1;
        }
    });

	// MIDI Learnモード時はアニメーションのために再描画
	if (midiLearnManager.isLearnModeActive())
		repaint();
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
			
			// マルチチャンネル設定を保存
			appProperties->setValue("stereoLinked", inputTap.getManager().isStereoLinked());
			appProperties->setValue("calibrationEnabled", inputTap.getManager().isCalibrationEnabled());
			
			// チャンネル設定をJSON形式で保存
			juce::var channelSettings = inputTap.getManager().getChannelManager().toVar();
			appProperties->setValue("channelSettings", juce::JSON::toString(channelSettings));
			
			appProperties->saveIfNeeded();
			DBG("🔧 Audio device settings & Trigger Threshold saved");
		}
	}
}

void MainComponent::loadAudioDeviceSettings()
{
	// まず基本的な初期化（デフォルト設定）
	setAudioChannels(MAX_CHANNELS, MAX_CHANNELS);
	
	// 保存された設定があれば復元
	if (appProperties != nullptr)
	{
		auto xmlState = appProperties->getXmlValue("audioDeviceState");
		if (xmlState != nullptr)
		{
			deviceManager.initialise(MAX_CHANNELS, MAX_CHANNELS, xmlState.get(), true);
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
        
        // マルチチャンネル設定を復元
        bool stereoLinked = appProperties->getBoolValue("stereoLinked", true);
        inputTap.getManager().setStereoLinked(stereoLinked);
        
        bool calibEnabled = appProperties->getBoolValue("calibrationEnabled", true);
        inputTap.getManager().setCalibrationEnabled(calibEnabled);
        
        // チャンネル設定をJSONから復元
        juce::String channelSettingsJson = appProperties->getValue("channelSettings", "");
        if (channelSettingsJson.isNotEmpty())
        {
            juce::var parsed = juce::JSON::parse(channelSettingsJson);
            if (!parsed.isVoid())
            {
                inputTap.getManager().getChannelManager().fromVar(parsed);
                DBG("✅ Channel settings restored");
            }
        }
	}
}
// ===== Auto-Arm 機能 =====
int MainComponent::findNextEmptyTrack(int fromTrackId) const
{
	const auto& tracks = looper.getTracks();
	int maxTracks = 8;
	
	for (int i = fromTrackId + 1; i <= maxTracks; i++)
	{
		if (auto it = tracks.find(i); it == tracks.end() || it->second.recordLength == 0)
		{
			return i;
		}
	}
	
	return -1; // 空きトラックなし
}

void MainComponent::updateNextTargetPreview()
{
	if (!isAutoArmEnabled)
	{
		nextTargetTrackId = -1;
		return;
	}
	
	// 現在選択されているトラックから次を探す
	int currentTrack = selectedTrackId;
	if (currentTrack == 0 && selectedTrack != nullptr)
		currentTrack = selectedTrack->getTrackId();
	
	nextTargetTrackId = findNextEmptyTrack(currentTrack);
	repaint();
}

// -------------------------------------------------------------------------
// MIDI Learn 視覚効果 (オーバーレイ)
// -------------------------------------------------------------------------
void MainComponent::paintOverChildren(juce::Graphics& g)
{
	if (!midiLearnManager.isLearnModeActive())
		return;

	// トラック部分のハイライト
	float alpha = juce::jlimit(0.0f, 1.0f, 0.5f + 0.2f * std::sin(juce::Time::getMillisecondCounter() * 0.01f));
	
	// --- Auto-Armボタンのハイライト ---
	{
		juce::String controlId = "main_auto_arm";
		auto bounds = autoArmButton.getBounds().toFloat().expanded(2.0f);
		if (midiLearnManager.getLearnTarget() == controlId)
		{
			g.setColour(juce::Colours::yellow.withAlpha(alpha));
			g.drawRoundedRectangle(bounds, 5.0f, 3.0f);
			g.setColour(juce::Colours::yellow.withAlpha(0.2f));
			g.fillRoundedRectangle(bounds, 5.0f);
		}
		else if (midiLearnManager.hasMapping(controlId))
		{
			g.setColour(ThemeColours::PlayingGreen.withAlpha(0.8f));
			g.drawRoundedRectangle(bounds, 5.0f, 2.0f);
		}
		else
		{
			g.setColour(ThemeColours::Silver.withAlpha(0.2f));
			g.drawRoundedRectangle(bounds, 5.0f, 1.0f);
		}
	}
	
	for (auto& track : trackUIs)
	{
		juce::String controlId = "track_select_" + juce::String(track->getTrackId());
		auto bounds = track->getBounds().toFloat().expanded(2.0f);
		
		// 1. 学習対象として選択されている場合（黄色点滅）
		if (midiLearnManager.getLearnTarget() == controlId)
		{
			g.setColour(juce::Colours::yellow.withAlpha(alpha));
			g.drawRoundedRectangle(bounds, 8.0f, 4.0f);
			
			g.setColour(juce::Colours::yellow.withAlpha(0.15f));
			g.fillRoundedRectangle(bounds, 8.0f);
		}
		// 2. 既にマッピングされている場合（緑枠）
		else if (midiLearnManager.hasMapping(controlId))
		{
			g.setColour(ThemeColours::PlayingGreen.withAlpha(0.8f));
			g.drawRoundedRectangle(bounds, 8.0f, 2.0f);
		}
		// 3. マッピングされていないが対象可能な場合（薄い白枠でヒント）
		else
		{
			g.setColour(ThemeColours::Silver.withAlpha(0.15f));
			g.drawRoundedRectangle(bounds, 8.0f, 1.0f);
		}
	}
}

// =====================================================
// MIDI Learn Listener
// =====================================================
void MainComponent::midiLearnModeChanged(bool isActive)
{
	// モード切替時に画面を更新して視覚的フィードバックをクリア/表示
	repaint();
	fxPanel.repaint();
}

void MainComponent::midiValueReceived(const juce::String& controlId, float value)
// ...
{
	// Note: 値に関わらずトリガー（トグル動作のMIDIコントローラーに対応）
	// if (value < 0.5f) return;
		
	if (controlId == "main_auto_arm")
	{
		// UIスレッドで実行
		juce::MessageManager::callAsync([this]()
		{
			// トグル切り替え
			// AutoArmのロジックはonClickに集約されているので、onClickを呼ぶのが一番安全
			// ただし、LearnModeがONだとonClick内でLearn処理が走ってしまうため、
			// LearnModeはOFFである必要がある（midiValueReceivedはLearnモードOFF時のみ呼ばれるはず）
			// Confirm: MidiLearnManager::midiInputCallback handles message -> calls notifyValueReceived if NOT learn mode.
			// Yes, notifyValueReceived is only called when NOT in learn mode for that specific message?
			// Checking MidiLearnManager.cpp:
			// if (learnModeEnabled) { set mapping... } else { notifyValueReceived... }
			// So yes, safe to call logic.
			
			// 直接onClickを呼ぶ
			autoArmButton.setToggleState(!autoArmButton.getToggleState(), juce::dontSendNotification);
			autoArmButton.onClick();
		});
		return;
	}

	if (controlId.startsWith("track_select_"))
	{
		int trackId = controlId.substring(13).getIntValue();
		if (trackId >= 1 && trackId <= trackUIs.size())
		{
			// トラック選択処理を実行
			auto* track = trackUIs[trackId - 1].get();
			
			// UIスレッドで実行
			juce::MessageManager::callAsync([this, track]()
			{
				// 通常モード時の呼び出しなので、trackClicked内でLearnモードチェックはfalseになり、
				// 通常の選択ロジックが実行される
				trackClicked(track);
			});
		}
		return;
	}

	// FXパネルのパラメータ制御
	if (controlId.startsWith("fx_"))
	{
		fxPanel.handleMidiControl(controlId, value);
		return;
	}
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
	// キーマッピングからアクションを取得
	juce::String action = keyboardMappingManager.getActionForKey(key.getKeyCode());
	
	if (action.isEmpty())
		return false;  // マッピングされていないキー
	
	DBG("⌨️ Key action: " << action);
	
	// === Transport Actions ===
	if (action == KeyboardMappingManager::ACTION_REC)
	{
		if (transportPanel.onAction)
			transportPanel.onAction("REC");
		return true;
	}
	if (action == KeyboardMappingManager::ACTION_PLAY)
	{
		if (transportPanel.onAction)
			transportPanel.onAction("PLAY");
		return true;
	}
	if (action == KeyboardMappingManager::ACTION_UNDO)
	{
		if (transportPanel.onAction)
			transportPanel.onAction("UNDO");
		return true;
	}
	
	// === Track Selection	// トラック選択アクション
	if (action.startsWith("track_"))
	{
		// "track_1" -> 1
		int trackId = action.substring(6).getIntValue();
		if (trackId >= 1 && trackId <= (int)trackUIs.size())
		{
            // マウスクリック時と同じ処理を通してトグル動作やFXモード連動を行う
            trackClicked(trackUIs[trackId - 1].get());
			DBG("⌨️ Track " << trackId << " selected via keyboard");
		}
		return true;
	}
	
	// === Auto-Arm Toggle ===
	if (action == KeyboardMappingManager::ACTION_AUTO_ARM)
	{
		autoArmButton.setToggleState(!autoArmButton.getToggleState(), juce::sendNotification);
		return true;
	}
	
	// === Visual Mode Toggle ===
	if (action == KeyboardMappingManager::ACTION_VISUAL_MODE)
	{
		if (transportPanel.onToggleTracks)
			transportPanel.onToggleTracks();
		return true;
	}
	
	// === FX Mode Toggle ===
	if (action == KeyboardMappingManager::ACTION_FX_MODE)
	{
		if (transportPanel.onShowFX)
			transportPanel.onShowFX();
		return true;
	}
	
	// === FX Toggle Actions (トラック別) ===
	int trackId, slotIndex;
	juce::String actionType;
	if (KeyboardMappingManager::parseFXActionId(action, trackId, slotIndex, actionType))
	{
		if (actionType == "slot_bypass")
		{
			fxPanel.toggleSlotBypass(trackId, slotIndex);
			return true;
		}
		else if (actionType == "filter_type")
		{
			fxPanel.toggleFilterType(trackId);
			return true;
		}
		else if (actionType == "repeat_active")
		{
			fxPanel.toggleRepeatActive(trackId);
			return true;
		}
	}
	
	return false;
}


