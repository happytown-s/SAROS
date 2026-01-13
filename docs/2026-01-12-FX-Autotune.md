# Autotune (Pitch Correction) Implementation Plan

## 概要 (Overview)
リアルタイムで入力音声またはトラック再生音声のピッチを検出し、指定したスケール（キー）の最も近い音程に補正する「オートチューン」エフェクトを実装します。

## 目標 (Goals)
1. **リアルタイムピッチ検出**: 低レイテンシで基本周波数 (F0) を検出する。
2. **ピッチ補正**: 検出したF0とターゲットノートとの差分を計算し、音声をシフトさせる。
3. **パラメータ制御**: Correction Amount (適用量), Speed (補正速度), Key/Scale (スケール指定) を操作可能にする。
4. **UI統合**: FXPanelに専用のコントロールを追加する。

---

## 技術アプローチ (Technical Approach)

### 1. ピッチ検出 (Pitch Detection)
JUCE標準には高度なピッチ検出クラスが含まれていないため、以下のいずれかのアプローチを採用します。

**A. 自己相関関数 (Autocorrelation) - 推奨**
*   **メリット**: 実装が比較的容易で、単音（ボーカル等）に対しては十分な精度が出る。
*   **デメリット**: 計算コストが高くなる可能性がある（ダウンサンプリングで軽減可能）。倍音誤検出（オクターブエラー）の対策が必要。
*   **実装方針**:
    *   入力信号をローパスフィルタに通す。
    *   ゼロ交差法（Zero-Crossing）と自己相関を組み合わせ、推定F0を算出する。
    *   YINアルゴリズムの簡易版を実装することを目標とする。

### 2. ピッチシフト (Pitch Shifting)
**Time-Domain Pitch Shifting (SOLA / PSOLA)**
*   オートチューン特有の「ケロケロボイス」を作るには、周波数領域（FFT）よりも時間領域での処理（Overlapped Add）の方が適している場合が多い。
*   **Delay Line Modulation**:
    *   可変長のディレイラインを使用し、読み出し速度を書き込み速度より速く/遅くすることでピッチを変える。
    *   読み出しヘッドが書き込みヘッドを追い越さないように、クロスフェード（Windowing）を行う。

### 3. スケールスナップ (Scale Snapping)
*   検出した周波数をMIDIノート番号に変換。
*   `Scale` 定義（例: C Major = {0, 2, 4, 5, 7, 9, 11}）に基づき、最も近い有効なノートを探す。
*   ターゲット周波数を算出する。

---

## クラス設計 (Class Design)

### `PitchDetector` (New Class)
*   `float detectPitch(const float* buffer, int numSamples, double sampleRate)`
*   内部でバッファリングを行い、安定したF0を返す。

### `PitchShifter` (New Class)
*   `void setPitchRatio(float ratio)`
*   `void process(const float* input, float* output, int numSamples)`
*   ディレイラインとウィンドウ関数を用いた簡易的なタイムストレッチ/ピッチシフト実装。

### `LooperAudio` / `FXChain`
*   **追加メンバ**:
    ```cpp
    bool autotuneEnabled = false;
    int  autotuneKey = 0;   // 0=C, 1=C#, ...
    int  autotuneScale = 0; // 0=Chromatic, 1=Major, 2=Minor, ...
    float autotuneAmount = 1.0f; // 0.0=No correction, 1.0=Hard tune
    float autotuneSpeed = 0.1f;  // Smooth factor
    
    PitchDetector pitchDetector;
    PitchShifter  pitchShifter;
    ```

---

## UI設計 (UI Design)

### `FXPanel`
*   **EffectType::Autotune** を追加。
*   **Controls**:
    *   **Key**: ComboBox (C, C#, D...)
    *   **Scale**: ComboBox (Chromatic, Major, Minor, Pentatonic...)
    *   **Amount**: Slider (0% - 100%) - 補正の強さ
    *   **Speed**: Slider (Fast - Slow) - グライドの速さ（0ms - 100ms程度）

---

## 実装ステップ (Implementation Steps)

1.  **基盤作成**:
    *   `PitchDetector` クラスの作成（まずはサイン波でテスト）。
    *   `PitchShifter` クラスの作成（単純なリサンプリングから開始）。
2.  **DSP統合**:
    *   `LooperAudio::processBlock` または `mixTracksToOutput` に処理を挿入。
    *   `FXChain` に統合。
3.  **UI実装**:
    *   `FXPanel` にAutotune用のスロットとUIコンポーネントを追加。
4.  **ブラッシュアップ**:
    *   ピッチ検出精度の向上（ノイズ除去、閾値調整）。
    *   クロスフェード処理の滑らかさ改善。
