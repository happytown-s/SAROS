
void LooperTrackUi::setLoopMultiplier(float multiplier)
{
    loopMultiplier = multiplier;
    
    // UIボタンのトグル状態を更新
    // dontSendNotificationを使うことで、コールバックのループを防ぐ
    if (std::abs(multiplier - 2.0f) < 0.001f)
    {
        mult2xButton.setToggleState(true, juce::dontSendNotification);
        multHalfButton.setToggleState(false, juce::dontSendNotification);
    }
    else if (std::abs(multiplier - 0.5f) < 0.001f)
    {
        mult2xButton.setToggleState(false, juce::dontSendNotification);
        multHalfButton.setToggleState(true, juce::dontSendNotification);
    }
    else
    {
        mult2xButton.setToggleState(false, juce::dontSendNotification);
        multHalfButton.setToggleState(false, juce::dontSendNotification);
    }
    
    // 変更通知（必要であれば）
    if (onLoopMultiplierChange)
        onLoopMultiplierChange(loopMultiplier);
        
    repaint();
}
