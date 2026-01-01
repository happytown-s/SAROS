# 16チャンネル対応と個別ステレオリンク実装タスク

## Phase 1: データ構造の拡張 (16ch & 個別リンク)
- [x] `ChannelTriggerSettings.h` の `MAX_CHANNELS` を 16 に定義
- [x] `MultiChannelTriggerManager` の `setNumChannels` 制限緩和

## Phase 2: InputManager 改修
- [x] `InputManager.h` の配列サイズを 16 に拡張
- [x] `InputManager.cpp` のゲインブースト判定を個別設定参照に変更
- [x] 多チャンネルトリガー検出ロジックの動作確認

## Phase 3: SettingsComponent UI刷新
- [x] `juce::Viewport` によるスクロール可能なチャンネルリストの実装
- [x] `ChannelSettingRow` による個別設定（Active/Link/Meter）の実装
- [x] 最大16チャンネル表示時のレイアウト最適化

## Phase 4: 設定の永続化
- [x] 個別の設定がJSONに保存・復元されることを確認（既存ロジックで対応）

## Phase 5: 検証
- [x] ビルド成功確認
- [ ] 16ch等の多チャンネル環境での実機確認（ユーザー側）
