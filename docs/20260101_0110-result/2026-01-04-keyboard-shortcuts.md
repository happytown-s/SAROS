# 2026-01-04 キーボードショートカット機能追加

## 概要
設定画面に「Keyboard」タブを追加し、キーボードショートカットを設定できる機能を実装。

## 追加ファイル

### Source/KeyboardMappingManager.h
キーボードマッピング管理クラス。
- アクションIDとキーコードの対応を保持
- `~/Library/Application Support/SAROS/SAROS.keymap` に設定を永続化
- 特殊キー（SPACE, RETURN, F1-F12, 矢印キー等）対応

### Source/KeyboardTabContent.h
設定画面のキーボードタブUI。
- 各アクションをリスト表示
- 右側にキー入力フィールド
- クリックでキー入力待機、Delete/Backspaceでクリア

## 変更ファイル

### SettingsComponent.h
- `KeyboardTabContent.h`をインクルード
- コンストラクタに`KeyboardMappingManager&`パラメータ追加
- 「Keyboard」タブを追加

### MainComponent.h / MainComponent.cpp
- `KeyboardMappingManager`メンバー追加
- `keyPressed()`オーバーライドで各アクション実行
- `setWantsKeyboardFocus(true)`でキー入力有効化

### CircularVisualizer.h
- `scopeData`参照箇所で`std::max(0.0f, ...)`を適用
- アサーションエラー(juce_Colour.cpp:340)を解消

## 割り当て可能アクション

| アクション         | 説明                 |
| ------------------ | -------------------- |
| REC (Record)       | 録音開始             |
| PLAY               | 再生開始             |
| UNDO               | 直前の録音取り消し   |
| Track 1-8 Select   | 各トラック選択       |
| AUTO-ARM Toggle    | オートアーム切替     |
| VISUAL MODE Toggle | ビジュアルモード切替 |
| FX MODE Toggle     | FXモード切替         |

## 使い方
1. ⚙ ボタンで設定画面を開く
2. 「Keyboard」タブを選択
3. 各項目の右側をクリックしてキーを入力
4. Delete/Backspaceでクリア、ESCでキャンセル
5. 設定は自動保存される

## 技術的詳細

### キー処理フロー
```cpp
bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    juce::String action = keyboardMappingManager.getActionForKey(key.getKeyCode());
    if (action.isEmpty()) return false;
    
    // アクション実行
    if (action == KeyboardMappingManager::ACTION_REC)
        transportPanel.onAction("REC");
    // ...
    return true;
}
```

### 設定永続化
- PropertiesFileでキーマッピングを保存
- アプリ起動時に自動読み込み
- 変更時に自動保存
