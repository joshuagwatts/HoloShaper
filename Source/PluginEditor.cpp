#include "PluginEditor.h"
#include "PluginProcessor.h"

HoloShaperEditor::HoloShaperEditor(HoloShaperProcessor& p)
    : AudioProcessorEditor(&p)
{
    setSize(640, 420);
}

void HoloShaperEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff141416));
    g.setColour(juce::Colour(0xffc8f04a));
    g.setFont(28.0f);
    g.drawText("HoloShaper", getLocalBounds().reduced(20),
               juce::Justification::centredTop, true);
    g.setColour(juce::Colour(0xff888888));
    g.setFont(14.0f);
    g.drawText("curve editor + trigger + MIDI CC out — coming next",
               getLocalBounds().reduced(20),
               juce::Justification::centred, true);
}
