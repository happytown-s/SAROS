# 2026-01-11 FX Synchronization Implementation

## 目的
Flanger, Chorus, Tremolo の各エフェクトをマスターループの周期および録音済みトラック数に同期させ、音楽的なモジュレーションを実現する。

## 実装内容

### 1. オーディオ・ロジック ([LooperAudio.cpp](file:///Users/mtsh/書類/code/JUCE/SAROS/Source/LooperAudio.cpp))
- **同期レートの算出**: `Rate (Hz) = 録音済みトラック数 / (ループの長さ in 秒)`。トラックが増えるほど、またはループが短いほどモジュレーションが速くなる。
- **位相のリセット**: ループの開始点 (`readPosition == 0`) で各エフェクトの LFO 位相を 0 にリセットし、タイミングのズレを防止。
- **手動設定の保持**: SYNC OFF 時には、ユーザーが設定した元のレートに戻るように値を別途保持。

### 2. UI・操作系 ([FXPanel.cpp](file:///Users/mtsh/書類/code/JUCE/SAROS/Source/FXPanel.cpp))
- **SYNC ボタン**: Flanger, Chorus, Tremolo の各セクションに "SYNC" 切り替えボタンを追加。
- **UI 連動**: SYNC ON 時は Rate スライダーを無効化し、自動計算されていることを明示。
- **レイアウト更新**: `resized()` を更新し、ノブの右側に SYNC ボタンを配置。

### 3. MIDI Learn 統合 ([FXPanel.cpp](file:///Users/mtsh/書類/code/JUCE/SAROS/Source/FXPanel.cpp))
- **コンポーネント選択**: `getControlIdForComponent` を実装し、SYNC ボタンを MIDI Learn の対象として選択可能に。
- **オーバーレイ表示**: MIDI Learn モード時に SYNC ボタンも黄色い枠線でハイライトされるよう修正。
- **トグル制御**: MIDI CC 入力により SYNC の ON/OFF を切り替え可能。

## 検証方法
1. **同期の確認**: 複数トラック録音時に Flanger/Chorus の周期がループの 1 周分（またはその整数倍/分）と一致しているか聴感および波形で確認。
2. **UI 操作**: SYNC ボタンをクリックして Rate スライダーがグレーアウトし、実際にレートが切り替わることを確認。
3. **MIDI 連携**: MIDI Learn モードで SYNC ボタンを選択し、外部コントローラーからトグル操作ができることを確認。
