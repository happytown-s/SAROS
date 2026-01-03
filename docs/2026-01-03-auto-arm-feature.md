# Auto-Arm (Chain Record) 機能 (2026-01-03)

## 概要

録音終了時に自動で次の空きトラックを「録音待機（Arm）」状態にする機能を実装しました。
これにより、ユーザーはトラック選択の操作を挟まずに、シームレスに複数トラックへの連続録音が可能になります。

## 機能の特徴

### 1. ワンクリック連続録音

- **Auto-Armボタン**: ヘッダー部（右上）に配置されたトグルボタンでON/OFF切り替え
- **自動トラック遷移**: 録音終了後、自動で次の空きトラックが待機状態に
- **途中解除**: 録音中でもボタンをOFFにすれば即座に連鎖を停止

### 2. スマートスキップ機能

- 既に録音済みのトラックは自動でスキップ
- 空きトラックのみを対象として順次選択
- 全トラックが埋まっている場合はAuto-Armを自動終了

### 3. 状態管理の改善

- **Auto-ArmをOFF**: 選択トラックとStandby状態を自動クリア
- **STOPボタン**: Auto-Arm状態を完全にリセット
- **STOP_RECボタン**: Auto-Arm状態をリセットし、待機状態をクリア
- **CLEARボタン**: すべての状態をリセット（既存機能に統合）

## 使い方

### 基本フロー

1. **Auto-ArmボタンをON**にする
2. トラック1を選択して録音開始
3. 録音終了後、**自動的にトラック2が待機状態**になる
4. トリガー入力で録音開始（トラック選択不要）
5. この流れがトラック8まで、または空きトラックがなくなるまで継続

### 途中で停止したい場合

- 録音中に**Auto-ArmボタンをOFF**にする
- 現在の録音は継続され、終了後に連鎖が停止
- 選択状態と待機状態が自動でクリアされる

### すべて停止したい場合

- **STOPボタン**を押す
- 再生中のすべてのトラックが停止
- Auto-Arm状態も自動でリセット

## 実装詳細

### 変更ファイル

#### MainComponent.h

- `juce::ToggleButton autoArmButton`: Auto-Armトグルボタン
- `bool isAutoArmEnabled`: Auto-Arm状態フラグ
- `int nextTargetTrackId`: 次にArm状態になるトラックID（将来の拡張用）
- `int findNextEmptyTrack(int fromTrackId) const`: 次の空きトラックを検索
- `void updateNextTargetPreview()`: 予告表示の更新（現在は内部処理のみ）

#### MainComponent.cpp

- **コンストラクタ**: Auto-Armボタンの初期化とイベントハンドラ設定
- **onRecordingStopped**: 録音終了時の連鎖ロジック実装
- **findNextEmptyTrack**: 空きトラック検索アルゴリズム
- **updateNextTargetPreview**: 次ターゲットの管理
- **transportPanel.onAction("STOP")**: STOP時のAuto-Armリセット
- **transportPanel.onAction("STOP_REC")**: STOP_REC時のAuto-Armリセット
- **transportPanel.onAction("CLEAR")**: CLEAR時のAuto-Armリセット
- **resized()**: Auto-Armボタンのレイアウト（ヘッダー右上に配置）

### アルゴリズム詳細

#### スマートスキップ (findNextEmptyTrack)

```cpp
int MainComponent::findNextEmptyTrack(int fromTrackId) const
{
    const auto& tracks = looper.getTracks();
    int maxTracks = 8;
    
    for (int i = fromTrackId + 1; i <= maxTracks; i++)
    {
        if (auto it = tracks.find(i); it == tracks.end() || it->second.recordLength == 0)
        {
            return i; // 空きトラック発見
        }
    }
    
    return -1; // 空きトラックなし
}
```

#### 連鎖ロジック (onRecordingStopped内)

1. Auto-ArmがONか確認
2. 次の空きトラックを検索（`findNextEmptyTrack`）
3. 空きトラックが見つかった場合：
   - トラックを選択状態に
   - 待機状態（Standby）に設定
   - さらに次のトラックを予告（将来のUI拡張用）
4. 空きトラックがない場合：
   - Auto-Armを自動終了
   - ボタンをOFFに

## テスト結果

### ✅ 基本フロー確認

- トラック1→2→3→4と連続録音が正常に動作
- 各トラックの録音終了後、次のトラックが自動で待機状態になることを確認

### ✅ 途中解除確認

- トラック3録音中にAuto-ArmをOFF
- トラック3終了後、トラック4が待機状態にならないことを確認
- 選択状態が自動でクリアされることを確認

### ✅ スマートスキップ確認

- トラック1, 3を録音済み、トラック2を空にして検証
- トラック1から再録音時、トラック2が正しく選択されることを確認

### ✅ STOP/STOP_REC動作確認

- Auto-Arm中にSTOPボタンを押下
- すべてのトラックが停止し、Auto-Arm状態がリセットされることを確認
- STOP_RECボタンでも同様の動作を確認

### ✅ 全トラック埋まっている場合

- 全8トラックを録音済みにして検証
- トラック8録音終了後、Auto-Armが自動でOFFになることを確認

## 今後の拡張案（オプション）

### 次ターゲット予告表示

現在、`nextTargetTrackId`は内部で管理されていますが、UIに表示されていません。
将来的には以下の機能を追加可能：

- 次にArm状態になるトラックの外枠を薄く明滅させる
- `LooperTrackUi`に`isNextTarget`フラグを追加
- `paint()`で予告表示のエフェクトを実装

## ビルド環境

- **Mac**: ビルド成功、動作確認済み
- **Windows CI**: GitHub Actionsで自動ビルド（JUCE 8.0.10対応済み）

## 関連コミット

- `5362f08`: Auto-Arm機能の初期実装
- `a70e071`: Auto-ArmのOFF時の選択解除、STOP/STOP_REC時の状態管理を修正
- `f96cc98`: feature/auto-armをmainにマージ
