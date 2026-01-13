# Video Export Mode (Seamless Loop Capture) 実装レポート (2026-01-13)

## 概要
SNS等への投稿用として、継ぎ目のない「エンドレス動画」を画面収録するための専用モードを実装しました。
このモードでは、UIの自動非表示、マスターループに基づいた正確な再生停止、およびループの繋ぎ目を視覚的に美しく見せるための「呼吸アニメーション」を統合しています。

## 技術的仕様

### 1. モード管理 (MainComponent)
`isVideoMode` フラグにより、アプリケーションの状態を「通常モード」から「撮影モード」へ移行させます。

- **startVideoMode()**:
    - 全ての操作UI（`TransportPanel`, `FXPanel`, `TrackUIs`, `Header Buttons`）を `setVisible(false)` に設定し、ビジュアライザーのみを全画面に近い形でクリーンに表示します。
    - `LooperAudio` の全トラックを一度停止・同期し、最初から再生をリスタートします。
    - 開始時点の絶対サンプル位置 (`getCurrentSamplePosition()`) を `videoModeStartSample` として記録します。
- **stopVideoMode()**:
    - 再生を停止し、全てのUIコンポーネントの可視性を復元します。
    - `resized()` を呼び出してレイアウトを再計算します。

### 2. ループ同期とオートメーション (MainComponent::timerCallback)
撮影モード中は、タイマー（60Hz）により毎フレーム以下のロジックが実行されます。

```cpp
// 経過サンプル数の計算
juce::int64 elapsed = currentSample - videoModeStartSample;
juce::int64 totalDuration = (juce::int64)masterLen * 2; // 正確に2ループ分

if (elapsed >= totalDuration) {
    stopVideoMode(); // 自動終了
} else {
    float progress = (float)elapsed / (float)totalDuration;
    visualizer.setVideoAnimationProgress(progress);
}
```

これにより、手動操作によるタイミングのズレを排除し、動画編集時に前後をカットするだけで完璧なループ素材として扱えるようになります。

### 3. ビジュアライザー呼吸アニメーション (CircularVisualizer)
ループの循環を強調するため、ズームスケールをサインカーブに基づいて変動させています。

- **Progress (0% ～ 100%)**: 動画全体の進行度。
- **Animation Curve**: `sin(progress * PI)`
    - 0%（開始）: scale 1.0 (最小)
    - 50%（1ループ終了時点）: scale 1.8 (最大)
    - 100%（2ループ終了時点）: scale 1.0 (最小)

この設計により、動画の開始フレームと終了フレームが「最小サイズ」で完全に一致するため、視覚的なジャンプ（カクつき）が発生しません。

## 関連トピック
- [Autotune (Pitch Correction) 実装レポート (2026-01-12)](https://www.notion.so/Autotune-Pitch-Correction-2026-01-12-1793757bb3a88090a93cf7811776ceb1)
- [Visualizer Roadmap](https://www.notion.so/Visualizer-Roadmap-1793757bb3a88095b6c3e6203cf310be)

## 今後の展望
- **Native Rendering**: 現在は画面収録を前提としていますが、JUCEの `VideoComponent` や外部ライブラリを用いて、直接動画ファイルとして書き出す機能への拡張を検討中です。
