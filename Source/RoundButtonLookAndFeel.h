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

// Glassmorphism Circular Button LookAndFeel
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
		
		// --- Glassmorphism Background ---
		
		// Outer glow (subtle)
		auto text = button.getButtonText();
		juce::Colour glowColor = ThemeColours::NeonCyan;
		if (text == "REC" || text == "STOP_REC")
			glowColor = ThemeColours::RecordingRed;
		else if (text == "PLAY" || text == "STOP")
			glowColor = ThemeColours::PlayingGreen;
		
		// Draw outer glow
		for (int i = 3; i >= 1; --i)
		{
			float alpha = isMouseOverButton ? 0.15f : 0.08f;
			g.setColour(glowColor.withAlpha(alpha / (float)i));
			g.fillEllipse(circleBounds.expanded(i * 2.0f));
		}
		
		// Frosted glass background - darker gradient
		juce::ColourGradient glassGradient(
			juce::Colour(40, 45, 55).withAlpha(0.85f), 
			circleBounds.getCentreX(), circleBounds.getY(),
			juce::Colour(25, 30, 40).withAlpha(0.95f), 
			circleBounds.getCentreX(), circleBounds.getBottom(),
			false
		);
		g.setGradientFill(glassGradient);
		g.fillEllipse(circleBounds.reduced(2.0f));
		
		// Top highlight (glass reflection effect)
		juce::Path highlight;
		highlight.addArc(circleBounds.getX() + 8, circleBounds.getY() + 4, 
						 circleBounds.getWidth() - 16, circleBounds.getHeight() * 0.5f,
						 juce::MathConstants<float>::pi * 1.1f, 
						 juce::MathConstants<float>::pi * 1.9f, true);
		g.setColour(juce::Colours::white.withAlpha(0.12f));
		g.strokePath(highlight, juce::PathStrokeType(2.0f));
		
		// Inner shadow/depth
		if (isButtonDown)
		{
			g.setColour(juce::Colours::black.withAlpha(0.3f));
			g.fillEllipse(circleBounds.reduced(3.0f));
		}
		
		// Border - subtle glass edge
		float borderAlpha = isMouseOverButton ? 0.5f : 0.25f;
		g.setColour(juce::Colours::white.withAlpha(borderAlpha));
		g.drawEllipse(circleBounds.reduced(2.0f), 1.0f);
		
		// Inner accent border
		g.setColour(glowColor.withAlpha(isMouseOverButton ? 0.4f : 0.2f));
		g.drawEllipse(circleBounds.reduced(3.5f), 0.5f);
		
		// Label below button
		juce::String labelText = "";
		
		if      (text == "REC" || text == "STOP_REC") labelText = "REC";
		else if (text == "PLAY" || text == "STOP")    labelText = "PLAY";
		else if (text == "UNDO")  labelText = "UNDO";
		else if (text == "CLEAR") labelText = "CLEAR";
		else if (text == "SETUP") labelText = "SETUP";
		
		// Label below button (removed for compact UI)
		// if (labelText.isNotEmpty())
		// {
		// 	g.setColour(ThemeColours::Silver.withAlpha(0.7f));
		// 	g.setFont(juce::Font(juce::FontOptions("Inter", 9.0f, juce::Font::plain)));
		// 	juce::Rectangle<float> labelBounds(bounds.getX(), circleBounds.getBottom() + 3.0f, bounds.getWidth(), 12.0f);
		// 	g.drawText(labelText, labelBounds, juce::Justification::centred, true);
		// }
	}
	
	void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool isMouseOver, bool isButtonDown) override
	{
		auto bounds = button.getLocalBounds().toFloat();
		float diameter = bounds.getWidth();
		float circleY = bounds.getY() + 2.0f;
		
		auto iconSize = diameter * 0.5f;
		auto centerX = bounds.getCentreX();
		auto centerY = circleY + diameter * 0.5f;
		
		auto text = button.getButtonText();
        juce::String svgText;

        // --- SVG Icons (Modern, Clean Style) ---
		if (text == "UNDO")
		{
			// Curved arrow pointing left (undo symbol)
			svgText = 
				"<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"#FFFFFF\" stroke-width=\"2.5\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
				"  <path d=\"M20 5 A 8 8 0 0 1 4 5\" />"
				"  <polyline points=\"1 8 4 5 7 8\" />"
				"</svg>";
		}
		else if (text == "CLEAR")
		{
			svgText = 
                "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"#FFFFFF\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
                "  <polyline points=\"3 6 5 6 21 6\" />"
                "  <path d=\"M19 6L19 21a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2\" />"
                "  <line x1=\"10\" y1=\"11\" x2=\"10\" y2=\"17\" />"
                "  <line x1=\"14\" y1=\"11\" x2=\"14\" y2=\"17\" />"
                "</svg>";
		}
        else if (text == "SETUP")
        {
            svgText =
                "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"#FFFFFF\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
                "  <circle cx=\"12\" cy=\"12\" r=\"3\" />"
                "  <path d=\"M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z\" />"
                "</svg>";
        }
        else if (text == "REC")
        {
             svgText =
                "<svg viewBox=\"0 0 24 24\" fill=\"#FFFFFF\" stroke=\"none\">"
                "  <circle cx=\"12\" cy=\"12\" r=\"7\" />"
                "</svg>";
        }
        else if (text == "STOP" || text == "STOP_REC")
        {
             svgText =
                "<svg viewBox=\"0 0 24 24\" fill=\"#FFFFFF\" stroke=\"none\">"
                "  <rect x=\"6\" y=\"6\" width=\"12\" height=\"12\" rx=\"2\" />"
                "</svg>";
        }
        else if (text == "PLAY")
        {
             svgText =
                "<svg viewBox=\"0 0 24 24\" fill=\"#FFFFFF\" stroke=\"none\">"
                "  <polygon points=\"7 4 19 12 7 20 7 4\" />"
                "</svg>";
        }

        if (svgText.isNotEmpty())
        {
            if (auto svgElem = juce::XmlDocument::parse(svgText))
            {
                if (auto drawable = juce::Drawable::createFromSVG(*svgElem))
                {
                    // Icon color based on button type
                    juce::Colour iconColor = juce::Colours::white.withAlpha(0.9f);
                    
                    if (text == "REC") 
                        iconColor = ThemeColours::RecordingRed;
                    else if (text == "STOP_REC")
                        iconColor = ThemeColours::RecordingRed.brighter(0.2f);
                    else if (text == "PLAY")
                        iconColor = ThemeColours::PlayingGreen;
                    else if (text == "STOP")
                        iconColor = ThemeColours::PlayingGreen.brighter(0.2f);

                    drawable->replaceColour(juce::Colours::white, iconColor);
                    
                    float iconDrawSize = iconSize;
                    if(text == "SETUP" || text == "CLEAR" || text == "UNDO") 
                        iconDrawSize *= 0.85f;

                    juce::Rectangle<float> iconArea(centerX - iconDrawSize * 0.5f,
                                                  centerY - iconDrawSize * 0.5f,
                                                  iconDrawSize, iconDrawSize);
                    
                    if (text == "PLAY") iconArea.translate(iconDrawSize * 0.08f, 0);

                    // Add subtle shadow under icon
                    if (auto shadowDrawable = juce::Drawable::createFromSVG(*svgElem))
                    {
                        shadowDrawable->replaceColour(juce::Colours::white, juce::Colours::black.withAlpha(0.3f));
                        shadowDrawable->drawWithin(g, iconArea.translated(1, 1), juce::RectanglePlacement::centred, 1.0f);
                    }

                    drawable->drawWithin(g, iconArea, juce::RectanglePlacement::centred, 1.0f);
                }
            }
        }
	}
};
