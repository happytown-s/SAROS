# マルチチャンネル入力トリガーシステム実装タスク

## Phase 1: データ構造

- [x] `ChannelTriggerSettings.h` 新規作成
- [x] `SmartRecConfig` に新しい設定項目を追加

## Phase 2: InputManager 改修

- [x] `InputManager.h` にチャンネル設定管理を追加
- [x] マルチチャンネル対応の `analyze` メソッド実装
- [x] モノラルモード時のゲインブースト処理
- [x] キャリブレーション機能実装

## Phase 3: AudioInputBuffer 拡張

- [x] マルチチャンネル対応のリングバッファ
- [x] チャンネルごとの閾値によるトランジェント検出

## Phase 4: SettingsComponent UI改修

- [x] チャンネル情報表示
- [x] モノ/ステレオ切替UI
- [x] キャリブレーションボタン
- [x] キャリブレーションON/OFF切替

## Phase 5: 設定の永続化

- [x] チャンネル設定のJSON形式定義
- [x] MainComponent での保存/読み込み実装

## Phase 6: 検証

- [x] ビルド確認
- [ ] 動作確認（ユーザー側）
