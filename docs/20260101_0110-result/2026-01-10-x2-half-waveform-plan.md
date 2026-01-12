# x2・/2 波形表示の実装計画

## 現状の問題点

### 1. 現在の計算式の問題
現在の `startAngleRatio` 計算:
```cpp
phase = (recordStartGlobal % masterLengthSamples) / masterLengthSamples
bufferOffset = recordStartGlobal / (masterLengthSamples * maxMultiplier)
startAngleRatio = phase - bufferOffset
```

**問題点**:
- `recordStartGlobal` は `writePosition`（バッファ内の絶対インデックス）
- x2の場合、`writePosition` は `[0, 2*masterLength)` の範囲
- /2の場合、`writePosition` は `[0, masterLength/2)` の範囲
- 現在の計算式は等倍(x1)トラック向けに調整されており、x2や/2では正しく動作しない

### 2. ビジュアル表示の要件

| 倍率             | トラック長 | 円周の占有率 | 繰り返し回数 |
| ---------------- | ---------- | ------------ | ------------ |
| x2 (maxMult=2時) | 2*master   | 100%         | 1回          |
| x1 (maxMult=2時) | master     | 50%          | 2回          |
| /2 (maxMult=2時) | master/2   | 25%          | 4回          |
| x1 (maxMult=1時) | master     | 100%         | 1回          |
| /2 (maxMult=1時) | master/2   | 50%          | 2回          |

---

## 設計方針

### 基本原則
1. **プレイヘッドが3時（0度）から開始**
2. **マスター波形は12時（-90度）から開始**（変更禁止）
3. **スレーブ波形はマスターと同期した位相で表示**
4. **録音開始位置が視覚的に正しい角度に表示される**

### アルゴリズム設計

#### ステップ1: 「録音開始フェーズ」の正規化
```
recordPhase = (writePosition % masterLength) / masterLength
```
これは「マスターループ内のどの位置で録音が始まったか」を0-1の範囲で表す。

#### ステップ2: 「表示開始角度」の計算
波形の描画は `buffer[0]` から始まる。しかし、`buffer[0]` には「録音開始位置」の音声データは入っていない。
実際の音声データは `buffer[writePosition]` にある。

**目標**: `buffer[writePosition]` が `recordPhase` の角度に表示されること。

現在の描画式:
```
angleRatio(i) = startAngleRatio + (i / trackLength) * (loopRatio / maxMultiplier)
```

`buffer[writePosition]` が `recordPhase` に来るためには:
```
recordPhase = startAngleRatio + (writePosition / trackLength) * (loopRatio / maxMultiplier)
```

解くと:
```
startAngleRatio = recordPhase - (writePosition / trackLength) * (loopRatio / maxMultiplier)
```

#### ステップ3: 各倍率での検証

**x1 (loopRatio=1, maxMultiplier=1)**:
- `startAngleRatio = recordPhase - (writePos / master) * 1`
- `= recordPhase - writePos/master`
- `= phase - phase = 0` ✓ (現在の等倍動作と一致)

**x2 (loopRatio=2, maxMultiplier=2)**:
- `startAngleRatio = recordPhase - (writePos / (2*master)) * (2/2)`
- `= recordPhase - writePos / (2*master)`
- `= (writePos % master) / master - writePos / (2*master)`

例: writePos = 1.5*master の場合
- recordPhase = 0.5 (50%位置)
- bufferOffset = 0.75
- startAngleRatio = 0.5 - 0.75 = -0.25 → 正規化して 0.75

これで `buffer[writePos=1.5*master]` は角度比率 `0.75 + (0.75 * 1) = 1.5 → 0.5` に表示される。
つまり50%位置（6時方向）に表示される。recordPhaseが0.5なので正しい！ ✓

**/2 (loopRatio=0.5, maxMultiplier=1)**:
- `startAngleRatio = recordPhase - (writePos / (master/2)) * (0.5/1)`
- `= recordPhase - writePos / master`
- `= (writePos % master) / master - writePos / master`

writePos < master/2 なので writePos % master = writePos
- `= writePos/master - writePos/master = 0` ✓

---

## 実装計画

### Phase 1: 計算式の修正
1. `addWaveform` の `startAngleRatio` 計算を上記の設計に合わせる
2. `regenerateWaveformPath` も同様に修正
3. スレーブ用の60度オフセットは維持（現在の動作が正しければ）

### Phase 2: x2ボタンの再有効化
1. `LooperTrackUi.cpp` でボタンを再表示
2. x2トラックの録音・再生テスト
3. 波形表示の検証

### Phase 3: /2ボタンの確認
1. /2トラックの録音・再生テスト
2. 波形表示の検証
3. リピート表示の確認

### Phase 4: 統合テスト
1. マスター + x2 + /2 の同時使用テスト
2. 停止→再生時の同期確認
3. 各種エッジケースの確認

---

## 変更対象ファイル

### [MODIFY] CircularVisualizer.h
- `addWaveform()`: startAngleRatio計算式の修正
- `regenerateWaveformPath()`: 同様の修正

### [MODIFY] LooperTrackUi.cpp
- x2/半分ボタンを再表示（コメントアウト解除）

---

## 検証計画

### 自動テスト
- ビルド確認

### 手動検証
1. **x1のみテスト**: マスター + スレーブ(x1) で波形位置確認
2. **x2テスト**: マスター + スレーブ(x2) で波形位置確認
3. **/2テスト**: マスター + スレーブ(/2) で波形位置確認
4. **複合テスト**: x2 + x1 + /2 の同時使用
5. **停止→再生テスト**: 全トラックの再開時同期確認
