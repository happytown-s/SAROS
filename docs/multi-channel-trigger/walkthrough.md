# マルチチャンネル入力トリガーシステム Walkthrough

複数の入力デバイス（マイクと仮想オーディオ等）が混在する環境に最適化されたトリガー検知システムを実装しました。

---

## 新規ファイル

| ファイル | 説明 |
|---------|------|
| [ChannelTriggerSettings.h](file:///Users/mtsh/書類/code/JUCE/ORAS/Source/ChannelTriggerSettings.h) | チャンネルごとのトリガー設定と管理クラス |

---

## 変更ファイル

### [InputManager.h](file:///Users/mtsh/書類/code/JUCE/ORAS/Source/InputManager.h)

render_diffs(file:///Users/mtsh/書類/code/JUCE/ORAS/Source/InputManager.h)

### [InputManager.cpp](file:///Users/mtsh/書類/code/JUCE/ORAS/Source/InputManager.cpp)

render_diffs(file:///Users/mtsh/書類/code/JUCE/ORAS/Source/InputManager.cpp)

### [SettingsComponent.h](file:///Users/mtsh/書類/code/JUCE/ORAS/Source/SettingsComponent.h)

render_diffs(file:///Users/mtsh/書類/code/JUCE/ORAS/Source/SettingsComponent.h)

### [MainComponent.cpp](file:///Users/mtsh/書類/code/JUCE/ORAS/Source/MainComponent.cpp)

render_diffs(file:///Users/mtsh/書類/code/JUCE/ORAS/Source/MainComponent.cpp)

---

## 新機能

### 1. チャンネル情報表示
Settings画面に現在の入力チャンネル数を表示

### 2. ステレオ/モノ切替
- **Stereo Linked**: L/Rを1ペアとして処理
- **Mono**: 各チャンネルを個別処理、**+3dBゲインブースト**適用

### 3. キャリブレーション機能
- 「Calibrate」ボタンで2秒間のノイズフロア測定
- 測定結果に基づいて閾値を自動設定
- 「Use Calibration」OFF時は閾値を最低値に設定（ゲート開放）

### 4. 設定の永続化
- ステレオリンク、キャリブレーション有効、チャンネル設定をJSON形式で保存
- アプリ再起動後も設定が復元される

---

## 検証

- ✅ ビルド成功
- 動作確認はユーザー側で実施
