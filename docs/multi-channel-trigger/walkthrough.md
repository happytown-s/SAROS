# 16チャンネル対応 & 個別リンク設定 Walkthrough (Updated)

最大16チャンネルの入力に対応し、かつ各ペアごとにステレオリンク（Stereo/Mono）を設定できる柔軟なシステムを実装しました。

---

## 主な変更点

### 1. 16チャンネルへの拡張
- `MAX_CHANNELS` を `16` に変更し、内部のレベル監視用配列を拡張しました。
- `AudioDeviceSelectorComponent` でも最大16チャンネルまで選択可能になりました。

### 2. 個別ステレオリンク / 個別有効化
- 各ペアで **LINK** がONの場合はステレオとして、OFFの場合は **個別モノラル（+3dBブースト適用）** として動作します。

### 3. Settings UI刷新 (New!)

16チャンネルを効率的に管理するため、設定画面を大幅にリニューアルしました。

#### タブ構成
設定画面を `juce::TabbedComponent` を用いて2つのタブに分割しました。

1. **Device タブ**: オーディオデバイスの選択、入出力チャンネルの有効化設定。
2. **Trigger タブ**: トリガー感度（Threshold）、キャリブレーション、および詳細なチャンネル設定。

#### チャンネル詳細グリッド (Channel Pair Grid)
2チャンネルを1つのペアとして扱う「ペアカード」UIを採用しました。

- **レイアウト**: 1行に4ペア（8チャンネル）を表示し、16チャンネル時は2行に折り返されます。
- **ペアカード機能**:
    - **デバイス名表示**: 各チャンネルの上に `Input 1` などのデバイス入力名を表示。
    - **縦長メーター**: 視認性の高い縦型レベルメーター。
    - **ACTIVEボタン**: 各チャンネル個別にON/OFF可能。
    - **LINKボタン**: ペア（例: 1-2）ごとにステレオリンクを一括設定。

#### 実装クラス
- `SettingsComponent`: タブ管理
- `DeviceTabContent`: デバイス選択UI
- `TriggerTabContent`: トリガー設定UI
- `ChannelPairCard`: ペアごとのUIコンポーネント
- `ChannelPairGridContainer`: カードのグリッド配置管理

---

## 変更ファイル

| ファイル | 概要 |
|---------|------|
| [ChannelTriggerSettings.h](file:///Users/mtsh/書類/code/JUCE/ORAS/Source/ChannelTriggerSettings.h) | `MAX_CHANNELS` の定義と制限緩和 |
| [InputManager.h](file:///Users/mtsh/書類/code/JUCE/ORAS/Source/InputManager.h) | `MAX_CHANNELS` 拡張と配列サイズ変更 |
| [InputManager.cpp](file:///Users/mtsh/書類/code/JUCE/ORAS/Source/InputManager.cpp) | ゲインブースト判定の個別化 |
| [SettingsComponent.h](file:///Users/mtsh/書類/code/JUCE/ORAS/Source/SettingsComponent.h) | UI刷新、マスターメーター復元 |

---

## 検証結果

- ✅ 16チャンネルまでのビルドと、各チャンネルの状態管理を確認しました。
- ✅ マスターメーターにより、閾値を設定する際の全チャンネル最大レベルの視認性が向上しました。
