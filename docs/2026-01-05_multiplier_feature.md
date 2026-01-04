# x2/÷2トラック倍率機能 実装完了

**日付**: 2026-01-05

## 概要
トラック2-8に対して、マスターループの2倍(x2)または半分(÷2)の長さで録音できる倍率切り替え機能を実装。

## 実装した機能

### 1. 録音長の自動調整
- **x2選択時**: マスターの2倍の長さ（8カウント）まで録音して自動停止
- **÷2選択時**: マスターの半分の長さ（2カウント）で自動停止
- バッファサイズも倍率に応じて動的に確保

### 2. 波形ビジュアライザの連動
- **x2押下時**: 1周の円内に2回分の波形パターンが凝縮表示される
- マスター波形も同様に2回分の波形として表示
- ÷2の場合はマスターはそのまま、録音波形が半分で表示

### 3. UI制御
- 録音完了後はx2/÷2ボタンが無効化（視覚的に薄く表示）
- ALLCLEARでボタンが再有効化＆リセット

## 変更ファイル

### `LooperAudio.cpp`
- `startRecording()`: バッファサイズを`masterLoopLength * loopMultiplier`で確保
- `recordIntoTracks()`: 録音可能量と自動停止条件に`loopMultiplier`を反映
- `stopRecording()`: アラインメント処理で`effectiveLength`を計算

### `CircularVisualizer.h`
- `WaveformPath`構造体に`originalBuffer`等のフィールドを追加
- `setTrackMultiplier(trackId, multiplier)`メソッドを追加
- `regenerateWaveformPath()`で1周内にloopRatio回分の波形を凝縮表示

### `MainComponent.cpp`
- `onLoopMultiplierChange`コールバックでビジュアライザを更新
- x2時はマスター波形も2倍表示に連動

### `LooperTrackUi.cpp`
- `setState()`で録音完了後にボタン無効化、ALLCLEARで再有効化
