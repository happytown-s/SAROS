# 波形アラインメント根本修正

## 概要
マルチプライヤー(x2等)導入後に発生していた「波形開始位置のズレ」問題を根本解決。

### Waveform Alignment & Visualization Fixes (2026-01-10)

### 概要
波形の描画開始位置の不整合、x2倍速録音時の位相ズレ、およびアプリアイコンの設定を行いました。
さらに、目視確認用のテスト波形生成機能を強化し、UIとの連動を実装しました。

### 変更点

1.  **録音開始位置の表示修正**
    *   `LooperAudio.cpp`: `recordStartSample` の計算で、以前は `masterStartSample` からの差分を反転させていましたが、`writePosition`（ループ内の絶対位置）をそのまま使用するように修正しました。これにより、録音を開始した時点の角度（例：ループの50%時点で録音開始したら6時の位置）から正しく波形が描画されるようになりました。

2. 
- **Correction of Test Data logic**:
    - Initially, `recordStartSample` for tracks 7 and 8 was incorrectly set to the 4th beat of the *first* bar (Beat 3) instead of the second bar (Beat 7).
    - Fixed to `samplesPerBeat * 7` to correctly represent "Start @ Bar 2, Beat 4".

- **Fixing `maxMultiplier` Propagation**:
    - Discovered that `CircularVisualizer` was adding waveforms using a stale `maxMultiplier` (default 1) during test generation, causing incorrect angle calculations even if the formula was correct.
    - Updated `MainComponent::onRecordingStopped` to explicitly call `visualizer.setMaxMultiplier(looper.getMaxLoopMultiplier())` *before* adding the waveform.
    - Added `getMaxLoopMultiplier()` to `LooperAudio`.

- **Restoring Calculation Logic**:
    - Reverted the `startAngleRatio` calculation in `CircularVisualizer.h` to the "subtraction method" (`recordPhase - bufferAngleSpan`).
    - With the correct `maxMultiplier` (2.0) supplied, this formula correctly calculates angles relative to the entire extended loop circle.
    - **Result**:
        - Track 4 (x1, Start@Beat2): Calculated as `0.125` (45 degrees, or 1.5 o'clock) -> Correct for x2 scale.
        - Track 7 (x2, Start@Beat7): Calculated as `0.875` (315 degrees, or 10.5 o'clock) -> Correct.

## Verification Results
- **Track 1-3**: Full length waveforms displayed correctly.
- **Track 4-6 (Punch-in @ Beat 2)**:
    - Displayed at approx **1.5 o'clock (45°)**.
    - This is correct because with an x2 track present, the circle represents 8 beats (2 bars). Beat 2 is 1/4 of a standard bar, but 1/8 of the x2 circle. 1/8 of 360° is 45°.
- **Track 7-8 (Punch-in @ Bar 2, Beat 4)**:
    - Displayed at approx **10.5 o'clock (315°)**.
    - Start is at Beat 7. 7/8 of the x2 circle is 315°.
 これにより、等倍だけでなくx2倍速や/2半速トラックでも、録音された音がマスターループと完全に同期した角度に表示されるようになりました。
    *   スレーブトラックのオフセット（+60度など）を廃止し、全トラックをマスターと同じ12時基準（-90度オフセット）に統一しました。

3.  **テスト機能の強化**
    *   `LooperAudio.cpp`: `generateTestWaveformsForVisualTest()` を実装。
        *   トラック1-3: 基本パターン（x1, x2, /2）
        *   トラック4-6: 途中録音（パンチイン）シミュレーション。2拍目から録音を開始したデータを生成し、波形が正しい位相（3時方向）から描画されるかを検証可能に。
    *   `LooperTrackUi.cpp`: `setLoopMultiplier(float)` メソッドを追加し、外部からUIボタン（x2, /2）のトグル状態を制御可能にしました。
    *   `MainComponent.cpp`: キーボードの「T」キーまたはUIの「TEST」ボタンで、テスト波形生成と同時に各トラックの倍率表示（x2, /2）が自動設定されるようにしました。

4.  **アプリアイコン設定**
    *   四隅を透過処理したアイコン画像を `Source/Assets/icon.png` に配置。
    *   `CMakeLists.txt` に `ICON_BIG` / `ICON_SMALL` 設定を追加し、アプリビルド時にアイコンが適用されるようにしました。

### 検証結果
*   **波形表示**: x2トラック、/2トラック共に、マスターループのリピート回数や開始位置に応じた正しい角度で波形が描画されることを確認。
*   **途中録音（パンチイン）**: 2拍目から録音開始したトラック（x1, x2, /2）が、正しく3時の方向（90度）から波形表示されることを確認。これにより `addWaveform` の位相計算ロジックの正当性を証明。
*   **テスト機能**: Tキー押下で一発でテスト環境が構築され、UI表示も矛盾なく更新されることを確認。
*   **停止・再生**: 停止後の再生再開時に、全トラックが同期して再生されることを確認（手動再生時は全トラック頭出し）。
