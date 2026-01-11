# Playhead x2 Fix Implementation - 2026-01-11

## 概要
倍率（x2等）が設定されたトラックが存在する場合、マスターの1ループではなく、最大倍率（MaxMultiplier）を考慮した「拡張されたループ区間」としてプレイヘッド位置を計算・表示するように変更しました。

## 実装内容

### 1. LooperAudio (`LooperAudio.h`, `LooperAudio.cpp`)
*   **追加メソッド**: `getEffectiveNormalizedPosition(float maxMultiplier)`
    *   **計算式**: `(currentSamplePosition - masterStartSample) / (masterLoopLength * maxMultiplier)`
    *   **役割**: グローバルなサンプル位置から、最大倍率を考慮した0.0〜1.0の正規化位置を計算して返します。
    *   これにより、x2トラックがある場合、マスターループ2周分でプレイヘッドが1周する挙動となります。

### 2. CircularVisualizer (`CircularVisualizer.h`)
*   **変更**: `setPlayHeadPosition(float effectiveNormalizedPos)`
    *   Visualizer側での `loopCount` による周回管理ロジックを削除。
    *   外部から渡された正規化位置（0.0〜1.0）をそのまま表示に反映するようにシンプル化しました。

### 3. MainComponent (`MainComponent.cpp`)
*   **変更**: `timerCallback` 内の更新処理
    *   `looper.getMaxLoopMultiplier()` を取得。
    *   `looper.getEffectiveNormalizedPosition(maxMult)` を呼び出してVisualizerに設定。

## 目的
以前の実装ではVisualizer側でループ回数をカウントしていましたが、シークや変則的な再生位置変更時にズレが生じる可能性がありました。
LooperAudio側で絶対的なサンプル位置に基づいて計算することで、常に正しい現在位置を表示できるようになります。
