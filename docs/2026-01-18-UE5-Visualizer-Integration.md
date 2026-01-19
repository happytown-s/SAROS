# UE5 Visualizer Integration Walkthrough (2026-01-18)

## 概要
Unreal Engine 5 (UE5) でビジュアライザを作成するための、SAROS側（JUCE）のOSC送信機能を実装しました。
既存のアーキテクチャに影響を与えないよう、独立した `OscDataSender` クラスとして実装しています。

## 変更点

### 1. 新規クラス `OscDataSender`
- `LooperAudio` の状態（再生位置、各トラックの音量）を監視し、OSCメッセージとして送信します。
- **送信レート**: 60fps (16ms間隔)
- **ポート**: `127.0.0.1:8000` (localhost)

### 2. OSCメッセージ仕様
| Address                   | Type   | Description                                       |
| ------------------------- | ------ | ------------------------------------------------- |
| `/saros/master/pos`       | float  | マスターの再生位置 (0.0 - 1.0)                    |
| `/saros/track/{id}/level` | float  | トラック {id} のRMS音量 (0.0 - 1.0)               |
| `/saros/event`            | string | トリガーイベント (Kick, Snareなど - *Future Use*) |

### 3. Build Configuration (`CMakeLists.txt`)
- `juce_osc` モジュールを追加。
- 新規ソースファイル (`OscDataSender.cpp`, `OscDataSender.h`) をビルド対象に追加。

## UE5側の受信設定 (参考)
1. **OSC Plugin** を有効化。
2. **OSC Server** をポート `8000` で作成。
3. `On OSC Message Received` ノードで `/saros/...` をパースして Niagara Parameter 等に流し込みます。

## 検証手順
1. ビルドして実行 (`cmake --build . --target run`)。
2. ターミナルで `nc -u -l 8000` (netcat) 等を実行し、データが流れてくるか確認。
   - または、簡単な OSC Monitor ツールを使用。
3. UE5 エディタを開き、OSC受信を確認。
