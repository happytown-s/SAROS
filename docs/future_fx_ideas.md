# Future FX Ideas & Roadmap

実装候補のFX一覧とパラメータ案です。

## 1. Bitcrusher (ビットクラッシャー)
デジタルな劣化・歪みを加えるLo-Fiエフェクト。
*   **BITS**: ビット深度の削減 (24bit -> 4bit)。バリバリとした歪み。
*   **RATE (Downsample)**: サンプリング周波数の削減。金属的なエイリアスノイズ。
*   **MIX**: 原音とエフェクト音のバランス。

## 2. Slicer / Trance Gate
音声をリズミカルにカットするゲートエフェクト。
*   **RATE**: カット周期 (1/4, 1/8, 1/16拍など)。
*   **DEPTH**: 音量減衰の深さ。
*   **SHAPE**: 波形形状 (Square=パキパキ, Sine=柔らかく, Saw=鋭く)。

## 3. Phaser
位相をずらしてシュワシュワした浮遊感を出すエフェクト。
*   **RATE**: うねりの速さ。
*   **DEPTH**: うねりの深さ。
*   **FEEDBACK**: 共鳴の強さ（エグ味）。

## 4. Pitch Correction (Auto-Tune)
ユーザー要望のピッチ補正エフェクト。
入力ピッチを検出し、指定したスケールの最も近い音程に強制補正する「ケロケロボイス」効果。
*   **SCALE**: 補正するスケール (Chromatic, Maj, Min, Pentatonic等)。
*   **SPEED (Retune Speed)**: 補正速度。0msに近いほど「ケロケロ」感（ロボットボイス感）が強くなる。
*   **AMOUNT**: 補正のかかり具合。
*   ***Implementation Note***: 基本的なピッチ検出(FFT/YIN)とピッチシフト処理が必要。難易度高。

## 5. Tape Stop
レコードやテープを急停止させた時のように、ピッチと速度が同時に下がっていくエフェクト。
*   **SPEED**: 停止（または復帰）までの時間。
*   **TRIGGER**: 開始トリガー。
*   *Note*: マスターにかけるのがいいかもしれないので、その場合はマスターFXを実装してから着手する。

## 6. Granular Cloud
音を細かな「粒(Grain)」に分解して再構築するグリッチ・幻想的エフェクト。
*   **GRAIN SIZE**: 粒の大きさ。音の質感を劇的に変える。
*   **DENSITY**: 粒の密度。
*   **PITCH JITTER**: 各粒のピッチのランダム度。

## 7. Reverse Delay
ディレイ音を逆再生して出力するサイケデリックなエフェクト。
*   **TIME**: 逆再生する単位時間。
*   **FEEDBACK**: フィードバック量。

## 8. Ring Modulator
入力音と別の波形を掛け合わせ、金属的な響きを作る無機質なエフェクト。
*   **FREQ**: 変調周波数。
*   **MIX**: ドライ/ウェット。

## 9. Distortion (ディストーション)
クラシックな歪みエフェクト。ドラムに太さを加えたり、シンセを攻撃的にするのに最適。
*   **DRIVE**: 歪みの量。上げるほど激しく歪む。
*   **TONE**: 高域/低域のバランス。歪み後の音色調整。
*   **TYPE**: 歪みの種類 (Soft Clip, Hard Clip, Tube, Fuzz等)。
*   **MIX**: 原音とのバランス。

## 10. Harmonizer (ハーモナイザー)
入力音にピッチシフトした音を重ねてハーモニーを作るエフェクト。
*   **INTERVAL**: ハーモニーの音程 (-12 ~ +12半音、または3度/5度等のプリセット)。
*   **DETUNE**: 微妙なピッチズレを加えてコーラス的な厚みを出す。
*   **MIX**: 原音とハーモニー音のバランス。
*   ***Implementation Note***: ピッチシフト処理が必要。Auto-Tuneと技術的に共通する部分が多い。

## 11. Pitch Shifter (ピッチシフター)
音程を上下にシフトするシンプルなエフェクト。オクターブ上/下やデチューン効果に。
*   **PITCH**: シフト量 (-24 ~ +24半音)。
*   **FINE**: 細かいピッチ調整 (セント単位)。
*   **MIX**: 原音とのバランス。
*   ***Implementation Note***: Harmonizer/Auto-Tuneと技術共有可能。

---
# 既存ロジック活用系

## 12. Tremolo (トレモロ)
音量を周期的に揺らすシンプルなエフェクト。
*   **RATE**: 揺れの速さ。
*   **DEPTH**: 揺れの深さ。
*   **SHAPE**: 波形 (Sine, Square, Triangle)。
*   ***Reuse***: LFO + ゲインのみ。新規実装だが非常にシンプル。

## 13. Auto-Pan (オートパン)
音像を左右に周期的に振るエフェクト。
*   **RATE**: パンの速さ。
*   **WIDTH**: 振り幅。
*   ***Reuse***: Tremoloと同じLFOロジック、ゲインをL/R別に適用。

## 14. Chorus (コーラス) ✅ Implemented
Flangerより遅い変調で厚みを出すエフェクト。
*   **RATE**: 変調速度 (Flangerより遅め 0.1-1Hz)。
*   **DEPTH**: 変調の深さ。
*   **MIX**: 原音とのバランス。
*   ***Reuse***: Flangerと同じ `juce::dsp::Chorus`。Centre Delayを長く(7-20ms)設定するだけ。

## 15. 3-Band EQ
低/中/高の3バンドイコライザー。
*   **LOW**: 低域ゲイン。
*   **MID**: 中域ゲイン。
*   **HIGH**: 高域ゲイン。
*   ***Reuse***: 既存の `juce::dsp::StateVariableTPTFilter` を3つ並列で使用。

## 16. Noise Gate (ノイズゲート)
一定音量以下をカットするエフェクト。
*   **THRESHOLD**: カットする閾値。
*   **ATTACK**: 開く速さ。
*   **RELEASE**: 閉じる速さ。
*   ***Reuse***: Compressorのサイドチェーン検出ロジックを応用。

## 17. Ping Pong Delay
左右交互にディレイ音が跳ねるステレオディレイ。
*   **TIME**: ディレイタイム。
*   **FEEDBACK**: フィードバック量。
*   **MIX**: Dry/Wet。
*   ***Reuse***: 既存のDelayを改造、L/R交互にpush/pop。

## 18. Freeze (フリーズ)
現在の音を無限ループで保持するエフェクト。アンビエント向け。
*   **TRIGGER**: フリーズ開始/解除。
*   **LENGTH**: 保持する長さ (ms)。
*   ***Reuse***: Beat Repeatのロジックを応用、無限リピート化。




