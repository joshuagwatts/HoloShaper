#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_graphics/juce_graphics.h>

// v1 scaffold editor: real ShaperBox-style curve editor lands next.
class HoloShaperEditor : public juce::AudioProcessorEditor
{
public:
    explicit HoloShaperEditor(HoloShaperProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override {}

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HoloShaperEditor)
};
