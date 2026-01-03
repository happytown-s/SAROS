# FX Panel MIDI Learn 実装計画

## 概要

FXパネル（`FXPanel`）上の各エフェクトパラメータをMIDIコントローラーにマッピング可能にする。
スロット（1〜4）の位置に依存せず、**「現在選択されているトラック」** の特定のエフェクトパラメータを操作できる直感的なシステムを実装する。

## コアコンセプト

* **Selection-Based Control**: MIDI操作は常に `MainComponent` で選択されているトラック（`selectedTrackId`）に対して適用される。
* **Effect-Specific Mapping**: スロット番号ではなく、「Filter Cutoff」や「Delay Time」といった**エフェクトタイプとパラメータ**に対してマッピングを行う。
    * メリット: Filterがスロット1にあってもスロット4にあっても、同じノブでCutoffを操作できる。
    * メリット: 画面上にエフェクトが表示されていなくても、内部パラメータを操作できる（「隠し味」的な操作が可能）。
* **Visual Feedback**: MIDI Learnモード時は、現在表示されているエフェクトのスライダーにオーバーレイを表示し、何がマッピング可能かを明示する。

## コントロールID設計

以下のIDを使用し、MIDIマッピングを行う。

### Filter

* `fx_filter_cutoff`: Cutoff Frequency
* `fx_filter_res`: Resonance
* `fx_filter_type`: LPF/HPF Toggle (Switch)

### Compressor

* `fx_comp_thresh`: Threshold
* `fx_comp_ratio`: Ratio

### Delay

* `fx_delay_time`: Time (ms)
* `fx_delay_feedback`: Feedback (%)
* `fx_delay_mix`: Mix (%)

### Reverb

* `fx_reverb_mix`: Mix (%)
* `fx_reverb_decay`: Decay/Room Size (%)

### Beat Repeat

* `fx_repeat_div`: Division (1/4, 1/8 ...)
* `fx_repeat_thresh`: Threshold
* `fx_repeat_active`: Active Toggle (Switch)

## 実装手順

### 1. MainComponent の拡張

* `midiValueReceived` メソッド内で、`fx_` で始まるIDを受け取った場合、それを `FXPanel` に転送する処理を追加。
* 現在選択されているトラックIDを保持しているため、それを利用して制御対象を決定する。

```cpp
// MainComponent.cpp (イメージ)
if (controlId.startsWith("fx_"))
{
    // values are normalized 0.0 - 1.0 from MidiMapping
    fxPanel.handleMidiControl(controlId, value);
}
```

### 2. FXPanel の拡張

* `handleMidiControl(juce::String controlId, float value)` メソッドを追加。
* 受け取ったIDと値に基づき、該当する `juce::Slider` の値を更新する。
* スライダーの `onValueChange` コールバックが発火し、`looper` へのパラメータ適用と `visualizer` の更新が自動的に行われる。

```cpp
// FXPanel.cpp (イメージ)
void FXPanel::handleMidiControl(const juce::String& controlId, float value)
{
    // UIスレッドでの実行を保証
    juce::MessageManager::callAsync([this, controlId, value]() {
        if (controlId == "fx_filter_cutoff")
        {
            // 正規化された値をスライダーの範囲に変換
            auto range = filterSlider.getRange();
            double scaledValue = range.start + value * (range.end - range.start);
            // Skew factor考慮が必要なら convertFrom0to1 を使う
            filterSlider.setValue(scaledValue, juce::sendNotificationSync);
        }
        else if (controlId == "fx_filter_type")
        {
             // トグル処理
             if (value > 0.5f) filterTypeButton.setToggleState(true, ...);
        }
        // ... 他のパラメータも同様
    });
}
```

### 3. MIDI Learnモード時の視覚フィードバック

* `FXPanel::paintOverChildren` (または `MainComponent` から描画) を実装。
* 現在 **可視状態にあるスライダー** に対してのみ、学習待機枠・マッピング済み枠を表示する。
* **注意**: 表示されていないスライダー（別のエフェクトスロットのものなど）には描画しないが、マッピング自体は有効。これにより画面がごちゃつくのを防ぐ。

## ユーザーメリット

* **直感性**: 「このツマミはリバーブ用」と決めてしまえば、どのトラックを選んでも、エフェクトがどこに入っていても、すぐにリバーブをかけられる。
* **柔軟性**: 画面でパラメータを見ながら微調整することも、画面を見ずに演奏に集中することも可能。
* **シンプルさ**: 操作体系がシンプルになり、ライブパフォーマンスでの事故（間違ったスロットを操作するなど）を減らせる。

## 今後の拡張性

将来的には「Master FX」や「Send FX」のような機能がついた場合も、同様のネーミング規則（`master_filter_cutoff` など）で対応可能。
