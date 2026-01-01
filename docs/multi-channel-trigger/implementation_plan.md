# 16チャンネル対応と個別ステレオリンク設定の実装計画

16チャンネルまでの入力に対応し、かつ各チャンネルペアごとに独立してステレオリンクを設定できるように拡張します。

## User Review Required

> [!IMPORTANT]
> - 現在の「全チャンネル一括ステレオリンク」から「ペアごとのステレオリンク」に変更されます。
> - UIが大幅に変更され、チャンネル数が多い場合にスクロールが必要になる可能性があります。

## Proposed Changes

### [ChannelTriggerSettings.h](file:///Users/mtsh/書類/code/JUCE/ORAS/Source/ChannelTriggerSettings.h)
- `MultiChannelTriggerManager::setNumChannels` の `jassert` 制限を 16 に。
- 偶数番目のチャンネル（0, 2, 4...）にステレオリンク設定を持たせ、対応する奇数チャンネル（1, 3, 5...）の動作を制御するように変更。

### [InputManager](file:///Users/mtsh/書類/code/JUCE/ORAS/Source/InputManager.h)
- `MAX_CHANNELS` を 16 に変更。
- `channelLevels` 配列のサイズを 16 に変更。
- `detectMultiChannelTrigger` 内で、各チャンネルの `isStereoLinked` フラグを個別にチェックするようにループを修正。

### [SettingsComponent.h](file:///Users/mtsh/書類/code/JUCE/ORAS/Source/SettingsComponent.h)
- グローバルな「Stereo Linked」ボタンを廃止、または各チャンネルペアのデフォルト設定とする。
- 入力チャンネル数に応じた「Channel Settings ScrollView」を追加。
- 各チャンネルにつき：
    - 小型レベルメーター（縦型または細い横型）
    - Stereo/Mono 切替スイッチ（ペアに対して1つ）
    - 有効/無効スイッチ（各ch）

## Verification Plan

### Automated Tests
- `cmake --build build` でビルドが通ることを確認。
- 16チャンネル入力デバイスを選択した際の挙動。

### Manual Verification
- ステレオリンクON/OFFの切り替えが正しく「+3dBゲインブースト」に反映されるか確認。
- キャリブレーションが全チャンネル（16ch）に対して同時に正しく動作するか確認。
- UIが16ch表示時に崩れないか確認。
