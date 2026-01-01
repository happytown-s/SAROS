# 16チャンネル対応 & 個別リンク設定 Walkthrough

最大16チャンネルの入力に対応し、かつ各ペアごとにステレオリンク（Stereo/Mono）を設定できる柔軟なシステムを実装しました。

---

## 主な変更点

### 1. 16チャンネルへの拡張
- `MAX_CHANNELS` を `16` に変更し、内部のレベル監視用配列を拡張しました。
- `AudioDeviceSelectorComponent` でも最大16チャンネルまで選択可能になりました。

### 2. 個別ステレオリンク / 個別有効化
- これまで一括だった「Stereo Linked」設定を、チャンネルごと（リンクはペアごと）に設定できるようにしました。
- 各ペアで **LINK** がONの場合はステレオとして、OFFの場合は **個別モノラル（+3dBブースト適用）** として動作します。

### 3. Settings UI の刷新
- 多数のチャンネルを管理するため、`juce::Viewport` を採用したスクロール可能なリスト形式に変更しました。
- 各行（CH 1, CH 2...）で以下の操作が可能です：
    - **ACTIVE**: チャンネルの有効/無効切り替え
    - **LINK**: ステレオリンクの切り替え（偶数チャンネルのみ）
    - **LEVEL METER**: 各チャンネルごとの信号入力レベル確認

---

## 変更ファイル

| ファイル | 概要 |
|---------|------|
| [ChannelTriggerSettings.h](file:///Users/mtsh/書類/code/JUCE/ORAS/Source/ChannelTriggerSettings.h) | `MAX_CHANNELS` の定義と制限緩和 |
| [InputManager.h](file:///Users/mtsh/書類/code/JUCE/ORAS/Source/InputManager.h) | `MAX_CHANNELS` 拡張と配列サイズ変更 |
| [InputManager.cpp](file:///Users/mtsh/書類/code/JUCE/ORAS/Source/InputManager.cpp) | ゲインブースト判定の個別化 |
| [SettingsComponent.h](file:///Users/mtsh/書類/code/JUCE/ORAS/Source/SettingsComponent.h) | **UI大幅刷新**（スクロールリスト形式） |

---

## 検証結果

- ✅ 16チャンネルまでのビルドと、各チャンネルの状態管理ロジックを確認しました。
- ✅ ステレオリンク切り替えによる +3dB ブーストの ON/OFF が正しく反映されることを確認しました。
- ✅ UIがコンパクトになり、チャンネル数が増えても崩れずに操作可能なことを確認しました。
