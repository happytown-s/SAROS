/*
  ==============================================================================

    PizzaColours.h
    Created: 18 Oct 2025 3:08:07pm
    Author:  mt sh

  ==============================================================================
*/

#pragma once
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

// =====================================================
// 🍕 PizzaColours : ピザルーパー全体のテーマカラー
// =====================================================
namespace PizzaColours
{
	// 🍕 ピザカラーパレット
	const juce::Colour CreamDough    = juce::Colour::fromRGB(248, 232, 192);  // 背景・生地
	const juce::Colour TomatoRed     = juce::Colour::fromRGB(232, 66, 54);    // 録音
	const juce::Colour BasilGreen    = juce::Colour::fromRGB(66, 190, 91);    // 再生
	const juce::Colour CheeseYellow  = juce::Colour::fromRGB(255, 187, 60);   // ハイライト
	const juce::Colour GrapePurple   = juce::Colour::fromRGB(120, 90, 200);   // 特殊ボタンなど
	const juce::Colour MushroomGray  = juce::Colour::fromRGB(190, 185, 170);  // 停止中
	const juce::Colour DeepOvenBrown = juce::Colour::fromRGB(80, 60, 45);     // テキスト・縁
}

// =====================================================
// 🍞 共通UI：ボタンスタイルセットアップ関数
// =====================================================
inline void setupRoundButton(juce::TextButton& btn, juce::Colour colour)
{
	btn.setColour(juce::TextButton::buttonColourId, colour);
	btn.setColour(juce::TextButton::textColourOnId, PizzaColours::DeepOvenBrown);
	btn.setColour(juce::TextButton::textColourOffId, PizzaColours::DeepOvenBrown);

	btn.setButtonText(btn.getButtonText().toUpperCase());
	btn.setClickingTogglesState(false);
	btn.setConnectedEdges(0);
	btn.setWantsKeyboardFocus(false);
	btn.setMouseCursor(juce::MouseCursor::PointingHandCursor);
}
