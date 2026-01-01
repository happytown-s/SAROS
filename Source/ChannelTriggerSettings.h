/*
  ==============================================================================

    ChannelTriggerSettings.h
    Created: 1 Jan 2026
    Author:  mt sh

    複数入力デバイス対応のチャンネルごとのトリガー設定

  ==============================================================================
*/

#pragma once
#include <juce_core/juce_core.h>

#define MAX_CHANNELS 16

// =====================================================
// チャンネルごとのトリガー設定
// =====================================================
struct ChannelTriggerSettings
{
    float threshold = 0.005f;           // トリガー閾値（ノイズフロア基準）
    bool isStereoLinked = true;         // ステレオリンク設定
    bool isActive = true;               // チャンネルの有効/無効
    bool isCalibrationEnabled = true;   // キャリブレーションを使用するか
    float calibratedNoiseFloor = 0.0f;  // キャリブレーションで測定されたノイズフロア
    
    // モノラルモード時のゲインブースト（dB）
    static constexpr float MONO_GAIN_BOOST_DB = 3.0f;
    
    // キャリブレーションOFF時の最低閾値（事実上ゲート開放）
    static constexpr float MIN_THRESHOLD = 0.0001f;  // -80dB相当
    
    // ゲインブースト値をリニアスケールで取得
    static float getMonoGainBoostLinear()
    {
        return std::pow(10.0f, MONO_GAIN_BOOST_DB / 20.0f);  // 約1.41x
    }
    
    // 有効な閾値を取得（キャリブレーションOFFなら最低値）
    float getEffectiveThreshold() const
    {
        if (!isCalibrationEnabled)
            return MIN_THRESHOLD;
        
        // キャリブレーション済みなら、ノイズフロア + ユーザー設定の閾値
        if (calibratedNoiseFloor > 0.0f)
            return calibratedNoiseFloor + threshold;
        
        return threshold;
    }
    
    // JSON形式への変換
    juce::var toVar() const
    {
        auto obj = new juce::DynamicObject();
        obj->setProperty("threshold", threshold);
        obj->setProperty("isStereoLinked", isStereoLinked);
        obj->setProperty("isActive", isActive);
        obj->setProperty("isCalibrationEnabled", isCalibrationEnabled);
        obj->setProperty("calibratedNoiseFloor", calibratedNoiseFloor);
        return juce::var(obj);
    }
    
    // JSONから復元
    static ChannelTriggerSettings fromVar(const juce::var& v)
    {
        ChannelTriggerSettings settings;
        if (auto* obj = v.getDynamicObject())
        {
            if (obj->hasProperty("threshold"))
                settings.threshold = (float)obj->getProperty("threshold");
            if (obj->hasProperty("isStereoLinked"))
                settings.isStereoLinked = (bool)obj->getProperty("isStereoLinked");
            if (obj->hasProperty("isActive"))
                settings.isActive = (bool)obj->getProperty("isActive");
            if (obj->hasProperty("isCalibrationEnabled"))
                settings.isCalibrationEnabled = (bool)obj->getProperty("isCalibrationEnabled");
            if (obj->hasProperty("calibratedNoiseFloor"))
                settings.calibratedNoiseFloor = (float)obj->getProperty("calibratedNoiseFloor");
        }
        return settings;
    }
};

// =====================================================
// マルチチャンネル設定マネージャー
// =====================================================
class MultiChannelTriggerManager
{
public:
    MultiChannelTriggerManager() = default;
    
    // チャンネル数を設定（デバイス変更時に呼び出す）
    void setNumChannels(int numChannels)
    {
        jassert(numChannels >= 0 && numChannels <= MAX_CHANNELS);
        channelSettings.resize(static_cast<size_t>(numChannels));
        this->numChannels = numChannels;
    }
    
    int getNumChannels() const { return numChannels; }
    
    // チャンネルごとの設定アクセス
    ChannelTriggerSettings& getSettings(int channel)
    {
        jassert(channel >= 0 && channel < numChannels);
        return channelSettings[static_cast<size_t>(channel)];
    }
    
    const ChannelTriggerSettings& getSettings(int channel) const
    {
        jassert(channel >= 0 && channel < numChannels);
        return channelSettings[static_cast<size_t>(channel)];
    }
    
    // 全チャンネルにステレオリンク設定を適用
    void setStereoLinked(bool linked)
    {
        for (auto& settings : channelSettings)
            settings.isStereoLinked = linked;
    }
    
    bool isStereoLinked() const
    {
        if (channelSettings.empty()) return true;
        return channelSettings[0].isStereoLinked;
    }
    
    // 全チャンネルにキャリブレーション有効設定を適用
    void setCalibrationEnabled(bool enabled)
    {
        for (auto& settings : channelSettings)
            settings.isCalibrationEnabled = enabled;
    }
    
    bool isCalibrationEnabled() const
    {
        if (channelSettings.empty()) return true;
        return channelSettings[0].isCalibrationEnabled;
    }
    
    // 全チャンネルの閾値を設定
    void setGlobalThreshold(float thresh)
    {
        for (auto& settings : channelSettings)
            settings.threshold = thresh;
    }
    
    // JSON形式への変換（全チャンネル）
    juce::var toVar() const
    {
        juce::Array<juce::var> arr;
        for (const auto& settings : channelSettings)
            arr.add(settings.toVar());
        return juce::var(arr);
    }
    
    // JSONから復元
    void fromVar(const juce::var& v)
    {
        if (auto* arr = v.getArray())
        {
            channelSettings.clear();
            for (const auto& item : *arr)
                channelSettings.push_back(ChannelTriggerSettings::fromVar(item));
            numChannels = static_cast<int>(channelSettings.size());
        }
    }
    
private:
    std::vector<ChannelTriggerSettings> channelSettings;
    int numChannels = 0;
};
