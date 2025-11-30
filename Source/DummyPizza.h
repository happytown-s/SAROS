/*
  ==============================================================================

    DummyPizza.h
    Created: 18 Oct 2025 3:40:32pm
    Author:  mt sh

  ==============================================================================
*/

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PizzaColours.h" // カラーパレット使えるように

class DummyPizza : public juce::Component
{
	public:
	void paint(juce::Graphics& g) override
	{
		auto bounds = getLocalBounds().toFloat();

		// 🍞 生地
		g.setColour(PizzaColours::CreamDough);
		g.fillEllipse(bounds.reduced(10));

		// 🍅 トマトソース
		g.setColour(PizzaColours::TomatoRed.withAlpha(0.7f));
		g.fillEllipse(bounds.reduced(30));

		// 🧀 チーズ
		g.setColour(PizzaColours::CheeseYellow.withAlpha(0.6f));
		g.fillEllipse(bounds.reduced(45));

		// 🍀 トッピング（バジル風）
		g.setColour(PizzaColours::BasilGreen.withAlpha(0.8f));
		for (int i = 0; i < 6; ++i)
		{
			float angle = juce::MathConstants<float>::twoPi * i / 6.0f;
			float r = bounds.getWidth() / 3.2f;
			float x = bounds.getCentreX() + std::cos(angle) * r;
			float y = bounds.getCentreY() + std::sin(angle) * r;
			g.fillEllipse(x - 10, y - 10, 20, 20);
		}

		// 🍄 マッシュルームっぽい点
		g.setColour(PizzaColours::MushroomGray.withAlpha(0.7f));
		for (int i = 0; i < 5; ++i)
		{
			float angle = juce::MathConstants<float>::twoPi * (i * 0.2f + 0.1f);
			float r = bounds.getWidth() / 2.8f;
			float x = bounds.getCentreX() + std::cos(angle) * r;
			float y = bounds.getCentreY() + std::sin(angle) * r;
			g.fillEllipse(x - 6, y - 6, 12, 12);
		}

		// 🍕 外周の焦げ目
		g.setColour(PizzaColours::DeepOvenBrown);
		g.drawEllipse(bounds.reduced(10), 4.0f);
	}
};
