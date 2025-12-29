/*
  ==============================================================================

    RoundButtonLookAndFeel.h
    Created: 16 Dec 2025
    Author:  mt sh

  ==============================================================================
*/

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// 🍕 丸型ボタン専用LookAndFeel
class RoundButtonLookAndFeel : public juce::LookAndFeel_V4
{
public:
	RoundButtonLookAndFeel() = default;
	
	void drawButtonBackground(juce::Graphics& g,
							juce::Button& button,
							const juce::Colour& backgroundColour,
							bool isMouseOverButton,
							bool isButtonDown) override
	{
		auto bounds = button.getLocalBounds().toFloat();
		
		// 幅を基準に円のサイズを決定（縦長ボタン前提）
		float diameter = bounds.getWidth();
		
		// 円を上部に配置（少しマージンを空ける）
		float x = bounds.getCentreX() - diameter * 0.5f;
		float y = bounds.getY() + 2.0f;
		
		juce::Rectangle<float> circleBounds(x, y, diameter, diameter);
		
		// マウスオーバー・押下時の明度調整
		auto fillColour = backgroundColour;
		if (isButtonDown)
			fillColour = fillColour.darker(0.3f);
		else if (isMouseOverButton)
			fillColour = fillColour.brighter(0.1f);
		
		// 円を描画
		g.setColour(fillColour);
		g.fillEllipse(circleBounds.reduced(2.0f));  // 少し小さくして余白を作る
		
		// 縁取り
		g.setColour(juce::Colours::black.withAlpha(0.3f));
		g.drawEllipse(circleBounds.reduced(2.0f), 1.5f);
		
		// ボタンの下にラベルテキストを表示
		auto& textButton = dynamic_cast<juce::TextButton&>(button);
		auto buttonText = textButton.getButtonText();
		
		juce::String labelText = "";
		
		// Unicodeシンボルからラベルテキストを決定
		if (buttonText == juce::String::fromUTF8("\xE2\x8F\xBA"))  // ⏺
			labelText = "REC";
		else if (buttonText == juce::String::fromUTF8("\xE2\x96\xA0"))  // ■
			labelText = "STOP";
		else if (buttonText == juce::String::fromUTF8("\xE2\x96\xB6"))  // ▶
			labelText = "PLAY";
		else if (buttonText == juce::String::fromUTF8("\xE2\x86\xB6"))  // ↶
			labelText = "UNDO";
		else if (buttonText == juce::String::fromUTF8("\xE2\x8C\xAB"))  // ⌫
			labelText = "CLEAR";
		else if (buttonText == juce::String::fromUTF8("\xE2\x9A\x99"))  // ⚙
			labelText = "SETUP";
		
		if (labelText.isNotEmpty())
		{
			g.setColour(juce::Colour::fromRGB(80, 60, 45));  // DeepOvenBrown
			g.setFont(10.0f);
			
			// ボタンの下に配置（円の下）
			juce::Rectangle<float> labelBounds(
				bounds.getX(),
				circleBounds.getBottom() + 2.0f,
				bounds.getWidth(),
				12.0f
			);
			
			g.drawText(labelText, labelBounds, juce::Justification::centred, true);
		}
	}
	
	void drawButtonText(juce::Graphics& g,
					   juce::TextButton& button,
					   bool isMouseOverButton,
					   bool isButtonDown) override
	{
		auto bounds = button.getLocalBounds().toFloat();
		// 円のサイズと位置を再計算
		float diameter = bounds.getWidth();
		float circleY = bounds.getY() + 2.0f;
		
		auto iconSize = diameter * 0.6f;
		auto centerX = bounds.getCentreX();
		auto centerY = circleY + diameter * 0.5f; // 円の中心
		
		auto text = button.getButtonText();
		
		// すべてのアイコンをCreamDough色（ベージュ）に統一
		g.setColour(PizzaColours::CreamDough);
		
		// UndoボタンのデザインをSVG描画に変更 (Drawableを使用して透過とスケールを完璧にする)
		if (text == juce::String::fromUTF8("\xE2\x86\xB6"))  // ↶
		{
			// ユーザーから提供されたSVG (strokeを明示的に黒に設定)
			juce::String svgText = 
				"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"24\" height=\"24\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"#000000\" stroke-width=\"2.5\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
				"  <path d=\"M20 5 A 8 8 0 0 1 4 5\" />"
				"  <polyline points=\"1 8 4 5 7 8\" />"
				"</svg>";

			if (auto svgElem = juce::XmlDocument::parse(svgText))
			{
				if (auto drawable = juce::Drawable::createFromSVG(*svgElem))
				{
					// 黒色をCreamDough（ベージュ）に置き換え
					drawable->replaceColour(juce::Colours::black, PizzaColours::CreamDough);
					
					// アイコンの描画サイズを調整 (少し大きめに)
					float iconDrawSize = iconSize * 0.95f;
					juce::Rectangle<float> iconArea(centerX - iconDrawSize * 0.5f,
												  centerY - iconDrawSize * 0.5f,
												  iconDrawSize, iconDrawSize);
					
					// SVGのデザインが中心より少し上にあるため、手動で垂直位置を微調整
					iconArea.translate(0, iconDrawSize * 0.15f);
					
					drawable->drawWithin(g, iconArea, juce::RectanglePlacement::centred, 1.0f);
				}
			}
		}
		else if (text == juce::String::fromUTF8("\xE2\x8F\xBA"))  // ⏺ RECボタン
		{
			// 小さな塗りつぶした円を描画
			float circleRadius = iconSize / 3;
			g.fillEllipse(centerX - circleRadius, centerY - circleRadius, 
						  circleRadius * 2, circleRadius * 2);
		}
		else if (text == juce::String::fromUTF8("\xE2\x96\xA0"))  // ■ STOPボタン
		{
			// 角丸四角を描画
			float squareSize = iconSize * 0.8f;
			juce::Rectangle<float> squareArea(
				centerX - squareSize/2,
				centerY - squareSize/2,
				squareSize,
				squareSize
			);
			g.fillRoundedRectangle(squareArea, squareSize * 0.15f); // 角を少し丸く
		}
		else
		{
			// その他のテキスト描画（三角形、X、歯車など）
			juce::Rectangle<float> iconArea(
				centerX - iconSize/2,
				centerY - iconSize/2,
				iconSize,
				iconSize
			);
			
			g.setFont(iconSize); 
			g.drawText(text, iconArea, juce::Justification::centred, false);
		}
	}
};
