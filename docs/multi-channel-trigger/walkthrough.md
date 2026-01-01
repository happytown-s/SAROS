# 16チャンネル対応 & 個別リンク設定 Walkthrough (Updated)

最大16チャンネルの入力に対応し、かつ各ペアごとにステレオリンク（Stereo/Mono）を設定できる柔軟なシステムを実装しました。

---

## 主な変更点

### 1. 16チャンネルへの拡張
- `MAX_CHANNELS` を `16` に変更し、内部のレベル監視用配列を拡張しました。
- `AudioDeviceSelectorComponent` でも最大16チャンネルまで選択可能になりました。

### 2. 個別ステレオリンク / 個別有効化
- 各ペアで **LINK** がONの場合はステレオとして、OFFの場合は **個別モノラル（+3dBブースト適用）** として動作します。

### 3. Settings UI の刷新
- `juce::Viewport` を採用し、16chでも操作しやすいスクロールリスト形式にしました。
- **マスターメーターの復元**: Sensitivity（閾値）スライダーの直下に、全アクティブチャンネルの最大レベルを表示する大きなメーターを配置しました。これにより、全体の感度設定が容易になります。
- 各行（CH 1, CH 2...）ごとに、ACTIVE切替、LINK切替（偶数ch）、個別メーターを表示します。

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
