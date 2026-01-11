# Flanger Effect Implementation - 2026-01-11

## 概要
FXパネルに**Flanger**エフェクトを追加しました。
`juce::dsp::Chorus` を応用し、短い遅延時間（1.5ms）とフィードバック設定により、特徴的なフランジングサウンドを実現しています。

## 実装内容

### 1. DSP (LooperAudio)
*   `juce::dsp::Chorus<float>` を使用。
*   **初期設定**:
    *   Centre Delay: 1.5ms (通常コーラスより短い)
    *   Mix: 50% (固定) - 最もキャンセレーション効果が高い値
*   **パラメータ**:
    *   Rate: 0.1Hz - 5.0Hz
    *   Depth: 0.0 - 1.0
    *   Feedback: 0.0 - 0.95 (UIで制御可能)

### 2. UI (FXPanel)
*   **スロット**: メニューに「Flanger」を追加。
*   **コントロール**: 
    *   **RATE**: うねりの速さ (0.1Hz - 5.0Hz)
    *   **DEPTH**: うねりの深さ (0% - 100%)
    *   **F.BACK**: フィードバック量 (0% - 95%)
*   **レイアウト**: 3つのノブを横並びに配置。MIDI Learnにも対応 (`fx_flanger_rate`, `fx_flanger_depth`, `fx_flanger_feedback`)。

## 配置順序
Filter (EQ) -> **Flanger** -> Delay -> Reverb
※ 一般的なモジュレーションエフェクトの位置に配置しました。

## 確認方法
1.  トラックを選択し、FXラックで空きスロットをクリック。
2.  「Flanger」を選択。
3.  RATE, DEPTH, F.BACK を操作して音色の変化を確認。
