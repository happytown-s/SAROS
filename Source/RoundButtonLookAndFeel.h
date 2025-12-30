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
		float diameter = bounds.getWidth();
		float x = bounds.getCentreX() - diameter * 0.5f;
		float y = bounds.getY() + 2.0f;
		juce::Rectangle<float> circleBounds(x, y, diameter, diameter);
		
		auto fillColour = backgroundColour;
		if (isButtonDown) 
			fillColour = fillColour.darker(0.3f);
		else if (isMouseOverButton)
		{
			// SETUPボタン（地色が明るい）はホバーで暗く、それ以外は明るく
			if (button.getButtonText() == "SETUP")
				fillColour = fillColour.darker(0.1f);
			else
				fillColour = fillColour.brighter(0.1f);
		}
		
		g.setColour(fillColour);
		g.fillEllipse(circleBounds.reduced(2.0f));
		g.setColour(juce::Colours::black.withAlpha(0.3f));
		g.drawEllipse(circleBounds.reduced(2.0f), 1.5f);
		
		auto text = button.getButtonText();
		juce::String labelText = "";
		
		// IDから表示ラベルへのマッピング
		if      (text == "REC" || text == "STOP_REC") labelText = "REC";
		else if (text == "PLAY" || text == "STOP")    labelText = "PLAY";
		else if (text == "UNDO")  labelText = "UNDO";
		else if (text == "CLEAR") labelText = "CLEAR";
		else if (text == "SETUP") labelText = "SETUP";
		
		if (labelText.isNotEmpty())
		{
			g.setColour(juce::Colour::fromRGB(80, 60, 45));
			g.setFont(10.0f);
			juce::Rectangle<float> labelBounds(bounds.getX(), circleBounds.getBottom() + 2.0f, bounds.getWidth(), 12.0f);
			g.drawText(labelText, labelBounds, juce::Justification::centred, true);
		}
	}
	
	void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) override
	{
		auto bounds = button.getLocalBounds().toFloat();
		float diameter = bounds.getWidth();
		float circleY = bounds.getY() + 2.0f;
		
		auto iconSize = diameter * 0.6f;
		auto centerX = bounds.getCentreX();
		auto centerY = circleY + diameter * 0.5f;
		
		auto text = button.getButtonText();
        juce::String svgText;

        // --- SVG Definition ---
		if (text == "UNDO")
		{
			svgText = 
				"<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"#000000\" stroke-width=\"2.5\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
				"  <path d=\"M20 5 A 8 8 0 0 1 4 5\" />"
				"  <polyline points=\"1 8 4 5 7 8\" />"
				"</svg>";
		}
		else if (text == "CLEAR")
		{
            // Trash Can
			svgText = 
                "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"#000000\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
                "  <polyline points=\"3 6 5 6 21 6\" />"
                "  <path d=\"M19 6L19 21a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2\" />"
                "  <line x1=\"10\" y1=\"11\" x2=\"10\" y2=\"17\" />" // 縦線
                "  <line x1=\"14\" y1=\"11\" x2=\"14\" y2=\"17\" />" // 縦線
                "</svg>";
		}
        else if (text == "SETUP")
        {
            // Gear
            svgText =
                "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"#000000\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
                "  <circle cx=\"12\" cy=\"12\" r=\"3\" />"
                "  <path d=\"M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z\" />"
                "</svg>";
        }
        else if (text == "REC")
        {
            // Filled Circle
             svgText =
                "<svg viewBox=\"0 0 24 24\" fill=\"currentColor\" stroke=\"none\">"
                "  <circle cx=\"12\" cy=\"12\" r=\"8\" />"
                "</svg>";
        }
        else if (text == "STOP" || text == "STOP_REC")
        {
             // Rounded Square
             svgText =
                "<svg viewBox=\"0 0 24 24\" fill=\"currentColor\" stroke=\"none\">"
                "  <rect x=\"6\" y=\"6\" width=\"12\" height=\"12\" rx=\"2\" />"
                "</svg>";
        }
        else if (text == "PLAY")
        {
             // Triangle
             svgText =
                "<svg viewBox=\"0 0 24 24\" fill=\"currentColor\" stroke=\"none\">"
                "  <polygon points=\"5 3 19 12 5 21 5 3\" />"
                "</svg>";
        }

        // --- Render SVG ---
        if (svgText.isNotEmpty())
        {
			if (auto svgElem = juce::XmlDocument::parse(svgText))
			{
				if (auto drawable = juce::Drawable::createFromSVG(*svgElem))
				{
                    // 色: 全てCreamDough
					drawable->replaceColour(juce::Colours::black, PizzaColours::CreamDough);
                    // fill="currentColor" の場合は、色置換が効かない場合があるので明示的に全パスを塗る手もあるが
                    // 単純な図形なら setColour で描画するときに反映される... いやDrawableは内部色を使う。
                    // replaceColour は特定の「色」を置き換える。
                    // SVG内で fill="currentColor" としておいて、描画時に色指定... はJUCE Drawableでは効きにくい。
                    // 確実にいくなら、fill="black" stroke="black" にして replaceColour するのが無難。
                    
                    // 修正: 上記SVG定義で currentColor を使わず black に統一する（replaceColour用）
                    // REC/STOP/PLAYは fill="black" に修正
				}
			}
        }
        
        // --- 修正版 Render Logic with fixed colors ---
        // コード簡略化のため、parse -> replace -> draw を一気に行う
        // SVG文字列内の currentColor を #000000 に置換してからパースすると確実
        svgText = svgText.replace("currentColor", "#000000");

        if (auto svgElem = juce::XmlDocument::parse(svgText))
        {
            if (auto drawable = juce::Drawable::createFromSVG(*svgElem))
            {
                // SETUPボタン（背景がCreamDough）の場合は濃い色にする、それ以外はCreamDough
                if (text == "SETUP")
                    drawable->replaceColour(juce::Colours::black, PizzaColours::DeepOvenBrown);
                else
                    drawable->replaceColour(juce::Colours::black, PizzaColours::CreamDough);
                
                float iconDrawSize = iconSize * 0.9f;
                // SETUP/CLEARは少し小さめの方がバランスが良いかも
                if(text == "SETUP" || text == "CLEAR") iconDrawSize *= 0.9f;

                juce::Rectangle<float> iconArea(centerX - iconDrawSize * 0.5f,
                                              centerY - iconDrawSize * 0.5f,
                                              iconDrawSize, iconDrawSize);
                
                // 位置微調整
                if(text != "REC" && text != "STOP" && text != "PLAY") {
                     iconArea.translate(0, iconDrawSize * 0.1f);
                }
                
                // PLAYの三角形は視覚的中心がずれるので少し右に
                if (text == "PLAY") iconArea.translate(iconDrawSize * 0.1f, 0);

                drawable->drawWithin(g, iconArea, juce::RectanglePlacement::centred, 1.0f);
            }
        }
	}
};
