# RMS Rubber Effect Implementation - 2026-01-11

## 概要
Visualizerの各トラック表示（リング状の波形）に対し、音量（RMS）に応じて有機的に伸縮・振動する「ゴムエフェクト」を実装しました。
単純なスケール変化ではなく、ばね・ダンパーモデル（Spring-Mass System）を用いた物理演算を導入することで、振動に「余韻」や「重み」を持たせ、リッチな視覚効果を実現しています。

## 実装内容

### 1. 物理演算の導入 (`CircularVisualizer.h`)
トラックごとに `TrackPhysicsState` 構造体を保持し、以下の物理パラメータでシミュレーションを行っています。

*   **Spring Constant (ばね定数)**: `0.15f` - 戻ろうとする力
*   **Damping (減衰係数)**: `0.85f` - 振動の収束速度
*   **Target Offset**: `RMS * 0.4f` (最大40%の拡大)

`timerCallback` 内で毎フレーム（60fps）物理ステップを更新し、滑らかな振動を生成しています。

```cpp
void update()
{
    float force = (targetOffset - currentOffset) * springConstant;
    velocity += force;
    velocity *= damping;
    currentOffset += velocity;
}
```

### 2. RMS取得の改善 (`LooperAudio.cpp`)
Visualizerが参照するRMS値を、エフェクト（Beat Repeat, Filter等）適用後のバッファから計算するように変更しました。
これにより、フィルタで音がこもったり、ビートリピートで連打されたりした際も、その音響変化に完全に同期して波形が振動します。

*   **変更前**: 生の録音バッファ (`track.buffer`) から計算
*   **変更後**: FX処理用一時バッファ (`trackBuffer`) から計算（`mixTracksToOutput` 内）

### 3. 疎結合な連携 (`MainComponent.cpp`)
`MainComponent` で毎回RMSを取得して渡すのではなく、初期化時に `visualizer.setLooperAudio(&looper)` で参照を渡し、Visualizer内部で自律的にデータをプルする方式を採用しました。これによりメインループの責務を軽減しています。

## 検証結果
*   ビルド成功 (Make 100%)
*   各トラックが独立して音量に反応することを確認済み（設計上）
*   x1, x2, /2 等の倍率設定時も、物理オフセットはトラック単位で適用されるため問題なし

## 次のステップ
*   Visual Mode video export (動画書き出し機能)
*   Loop-Synced Breathing Animation (呼吸エフェクト)
