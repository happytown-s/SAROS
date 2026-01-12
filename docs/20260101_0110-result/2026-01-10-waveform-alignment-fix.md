# 波形アラインメント根本修正

## 概要
マルチプライヤー(x2等)導入後に発生していた「波形開始位置のズレ」問題を根本解決。

## 修正内容

### 1. Phase-Alignment 導入
- `startAngleRatio`計算を `modulo masterLengthSamples` ベースに変更
- どのループ周回で録音しても同じビート位相なら同じ角度から開始

### 2. recordStartSampleバグ修正
- `LooperAudio.cpp` の `stopRecording` 内で上書きしていた行を削除
- 録音開始時に設定した正しい位置情報が保持されるように

### 3. 12時基準統一
- マスター・スレーブ両方を12時（-halfPi）からスタート
- ハイライトエフェクトも-halfPiオフセットを適用

## コミット
`9799c4b` - feature/waveform-visual-effects
