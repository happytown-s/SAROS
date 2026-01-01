# マルチチャンネル入力トリガーシステム刷新

複数の入力デバイス（マイクと仮想オーディオ等）が混在する環境に最適化されたトリガー検知システムを実装します。

## 概要

現在のシステムは全チャンネルを同一閾値で処理していますが、新システムでは：
- チャンネルごとに個別の設定（閾値、ステレオリンク、有効/無効、キャリブレーション）を管理
- モノラル入力に自動ゲインブースト適用
- いずれか1チャンネルでも閾値を超えたらトリガー発火（One-shot）
- キャリブレーション機能でノイズフロアに基づく閾値自動設定

---

## Proposed Changes

### Component 1: 新規データ構造

#### [NEW] [ChannelTriggerSettings.h](file:///Users/mtsh/書類/code/JUCE/ORAS/Source/ChannelTriggerSettings.h)

チャンネルごとの設定を管理する構造体：

```cpp
struct ChannelTriggerSettings {
    float threshold = 0.005f;       // トリガー閾値
    bool isStereoLinked = true;     // ステレオリンク設定
    bool isActive = true;           // チャンネル有効/無効
    bool isCalibrationEnabled = true; // キャリブレーション使用フラグ
    float calibratedNoiseFloor = 0.0f; // キャリブレーション結果
};
```

---

### Component 2: InputManager の改修

#### [MODIFY] [InputManager.h](file:///Users/mtsh/書類/code/JUCE/ORAS/Source/InputManager.h)

- `std::vector<ChannelTriggerSettings>` でチャンネル設定を動的管理
- ステレオリンクOFF時のゲインブースト（+3dB）処理
- チャンネルごとの閾値判定ロジック
- キャリブレーション実行メソッド

主な変更点：
```cpp
class InputManager {
public:
    // チャンネル設定管理
    void setNumChannels(int numChannels);
    ChannelTriggerSettings& getChannelSettings(int channel);
    void calibrateChannel(int channel);  // ノイズフロア測定
    
    // モノラルモード時のゲインブースト
    static constexpr float MONO_GAIN_BOOST_DB = 3.0f;
    
    // 改良版トリガー検出
    bool analyzeMultiChannel(const juce::AudioBuffer<float>& input);
    
private:
    std::vector<ChannelTriggerSettings> channelSettings;
};
```

---

### Component 3: SettingsComponent UI改修

#### [MODIFY] [SettingsComponent.h](file:///Users/mtsh/書類/code/JUCE/ORAS/Source/SettingsComponent.h)

設定画面に以下を追加：
1. **チャンネル情報表示** - 現在のデバイスのチャンネル数を表示
2. **モノ/ステレオ切替** - 各チャンネルペアのリンク設定
3. **キャリブレーションボタン** - ノイズフロア測定実行
4. **キャリブレーションON/OFF** - 使用しない場合は閾値を最低値に設定

UIレイアウト案：
```
┌─────────────────────────────────────────────┐
│ [Audio Device Selector]                     │
├─────────────────────────────────────────────┤
│ Input Channels: 2                           │
│ ○ Stereo Linked  ● Mono (L/R separate)      │
├─────────────────────────────────────────────┤
│ □ Use Calibration  [Calibrate] button       │
│ Threshold: ────●───────── 0.050             │
│ [Level Meter ▓▓▓▓▓░░░░░░░░░░░░░░░]          │
└─────────────────────────────────────────────┘
```

---

### Component 4: AudioInputBuffer 拡張

#### [MODIFY] [AudioInputBuffer.h](file:///Users/mtsh/書類/code/JUCE/ORAS/Source/AudioInputBuffer.h)

- マルチチャンネル対応のリングバッファ
- チャンネルごとの閾値を使用したトランジェント検出

---

### Component 5: 設定の永続化

#### [MODIFY] [MainComponent.cpp](file:///Users/mtsh/書類/code/JUCE/ORAS/Source/MainComponent.cpp)

- チャンネル設定のJSON保存/読み込み
- デバイス変更時の自動保存

---

## Verification Plan

### Manual Verification

> [!IMPORTANT]
> このプロジェクトはオーディオアプリケーションのため、自動テストではなく手動での動作確認が必要です。

1. **ビルド確認**
   - `cmake --build build --target ORAS` でビルドエラーがないことを確認

2. **設定画面の確認**
   - Settings画面を開き、チャンネル情報が表示されることを確認
   - Mono/Stereo切替ボタンが動作することを確認

3. **キャリブレーション機能**
   - Calibrateボタンを押して閾値が自動設定されることを確認
   - キャリブレーションOFF時に閾値が最低値になることを確認

4. **トリガー動作**
   - マイク入力で音を出してトリガーが発火することを確認
   - 仮想オーディオ（BlackHole等）からの入力でもトリガーが発火することを確認

5. **設定の保存/復元**
   - アプリを再起動して設定が保持されていることを確認

---

## User Review Required

> [!IMPORTANT]
> 以下の点について確認・ご意見をお聞かせください：
>
> 1. **ゲインブースト量**: モノラルモード時の+3dBで適切でしょうか？
> 2. **キャリブレーション時間**: ノイズフロア測定は何秒程度が適切でしょうか？（提案: 2秒）
> 3. **UI配置**: 設定画面のレイアウト案で問題ないでしょうか？
> 4. **チャンネル数上限**: 最大何チャンネルまで対応する必要がありますか？（提案: 8ch）
