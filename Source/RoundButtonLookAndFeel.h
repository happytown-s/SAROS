/*
  ==============================================================================

    RoundButtonLookAndFeel.h
    Created: 16 Dec 2025
    Author:  mt sh

  ==============================================================================
*/

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "ThemeColours.h"

// Futuristic Circular Button LookAndFeel
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
			fillColour = fillColour.brighter(0.2f);
		
		// Button body
		g.setColour(fillColour);
		g.fillEllipse(circleBounds.reduced(2.0f));
        
        // Neon Glow/Brim
        auto accent = ThemeColours::NeonCyan;
        if (button.getButtonText() == "REC" || button.getButtonText() == "STOP_REC")
            accent = ThemeColours::RecordingRed;
        
        g.setColour(accent.withAlpha(isMouseOverButton ? 0.6f : 0.3f));
		g.drawEllipse(circleBounds.reduced(2.0f), 1.5f);
		
		auto text = button.getButtonText();
		juce::String labelText = "";
		
		if      (text == "REC" || text == "STOP_REC") labelText = "REC";
		else if (text == "PLAY" || text == "STOP")    labelText = "PLAY";
		else if (text == "UNDO")  labelText = "UNDO";
		else if (text == "CLEAR") labelText = "CLEAR";
		else if (text == "SETUP") labelText = "SETUP";
		
		if (labelText.isNotEmpty())
		{
			g.setColour(ThemeColours::Silver.withAlpha(0.8f));
			g.setFont(juce::Font("Inter", 10.0f, juce::Font::plain));
			juce::Rectangle<float> labelBounds(bounds.getX(), circleBounds.getBottom() + 2.0f, bounds.getWidth(), 12.0f);
			g.drawText(labelText, labelBounds, juce::Justification::centred, true);
		}
	}
	
	void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) override
	{
		auto bounds = button.getLocalBounds().toFloat();
		float diameter = bounds.getWidth();
		float circleY = bounds.getY() + 2.0f;
		
		auto iconSize = diameter * 0.55f;
		auto centerX = bounds.getCentreX();
		auto centerY = circleY + diameter * 0.5f;
		
		auto text = button.getButtonText();
        juce::String svgText;

        // --- SVG Definition (Black for replacement) ---
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
			svgText = 
                "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"#000000\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
                "  <polyline points=\"3 6 5 6 21 6\" />"
                "  <path d=\"M19 6L19 21a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2\" />"
                "  <line x1=\"10\" y1=\"11\" x2=\"10\" y2=\"17\" />"
                "  <line x1=\"14\" y1=\"11\" x2=\"14\" y2=\"17\" />"
                "</svg>";
		}
        else if (text == "SETUP")
        {
            svgText =
                "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"#000000\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
                "  <circle cx=\"12\" cy=\"12\" r=\"3\" />"
                "  <path d=\"M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z\" />"
                "</svg>";
        }
        else if (text == "REC")
        {
             svgText =
                "<svg viewBox=\"0 0 24 24\" fill=\"#000000\" stroke=\"none\">"
                "  <circle cx=\"12\" cy=\"12\" r=\"8\" />"
                "</svg>";
        }
        else if (text == "STOP" || text == "STOP_REC")
        {
             svgText =
                "<svg viewBox=\"0 0 24 24\" fill=\"#000000\" stroke=\"none\">"
                "  <rect x=\"7\" y=\"7\" width=\"10\" height=\"10\" rx=\"1\" />"
                "</svg>";
        }
        else if (text == "PLAY")
        {
             svgText =
                "<svg viewBox=\"0 0 24 24\" fill=\"#000000\" stroke=\"none\">"
                "  <polygon points=\"6 4 20 12 6 20 6 4\" />"
                "</svg>";
        }

        if (svgText.isNotEmpty())
        {
            if (auto svgElem = juce::XmlDocument::parse(svgText))
            {
                if (auto drawable = juce::Drawable::createFromSVG(*svgElem))
                {
                    auto iconColor = ThemeColours::Silver;
                    if (text == "REC" || text == "STOP_REC") 
                        iconColor = ThemeColours::RecordingRed;
                    // PLAY icon stays Silver/white for visibility on green backgrounds

                    drawable->replaceColour(juce::Colours::black, iconColor);
                    
                    float iconDrawSize = iconSize;
                    if(text == "SETUP" || text == "CLEAR") iconDrawSize *= 0.85f;

                    juce::Rectangle<float> iconArea(centerX - iconDrawSize * 0.5f,
                                                  centerY - iconDrawSize * 0.5f,
                                                  iconDrawSize, iconDrawSize);
                    
                    if (text == "PLAY") iconArea.translate(iconDrawSize * 0.05f, 0);

                    drawable->drawWithin(g, iconArea, juce::RectanglePlacement::centred, 1.0f);
                }
            }
        }
	}
};
