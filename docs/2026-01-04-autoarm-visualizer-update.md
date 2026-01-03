# 2026-01-04 Auto-Arm修正 & ビジュアライザ改善

## 概要
Auto-Arm機能のTransportPanel表示修正と、ビジュアライザのブラックホールエフェクト改善を実施。

## 変更内容

### Auto-Arm / TransportPanel修正

#### MainComponent.cpp
- `STOP_REC` アクションでAuto-Armが無効にならないように修正
- `updateStateVisual()` の状態優先順位を変更: `Recording` > `Playing` > `Standby` > `Idle`
- Auto-Arm処理後に `updateStateVisual()` を呼び出すように追加

#### TransportPanel.cpp
- `State::Recording` でもplayButtonを `STOP` に変更
- `State::Standby` でもplayButtonを `STOP` に変更
- これにより、Idle以外のすべての状態でSTOPボタンが表示される

#### LooperTrackUi.cpp
- `withAlpha` に渡すalpha値をすべて `juce::jlimit` でクランプ
- アサーションエラー（juce_Colour.cpp:340）の防止

---

### ビジュアライザ改善

#### CircularVisualizer.h

**ブラックホール日食風エフェクト:**
- パーティクルをブラックホールの前（後ろレイヤー）に描画
- 3層の炎/プラズマ風グローリング追加
  - Layer 0: 白〜クリーム
  - Layer 1: オレンジ
  - Layer 2: 赤オレンジ
- 時間ベースのアルファ値アニメーション（ゆらめき効果）
- 中高音レベルに応じて明るさが変化
- 完全な円形のブラックホール本体

**バグ修正:**
- `scopeData` の負の値を `std::max(0.0f, ...)` でクランプ
- `time` 変数の重複定義を修正

## 技術的詳細

### 状態優先順位ロジック
```cpp
if (anyRecording)
    transportPanel.setState(State::Recording);
else if (anyPlaying)
    transportPanel.setState(State::Playing);
else if (anyStandby)
    transportPanel.setState(State::Standby);
else
    transportPanel.setState(State::Idle);
```

### 炎グローエフェクト
```cpp
for (int layer = 0; layer < 3; ++layer)
{
    float flicker = 0.5f + 0.5f * std::sin(time * 3.0f + layer * 1.5f);
    float layerAlpha = baseAlpha * (0.7f + flicker * 0.3f);
    g.drawEllipse(...);
}
```

## 変更ファイル
- `Source/MainComponent.cpp`
- `Source/TransportPanel.cpp`
- `Source/LooperTrackUi.cpp`
- `Source/CircularVisualizer.h`
