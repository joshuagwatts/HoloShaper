#include "PluginProcessor.h"
#include "PluginEditor.h"

HoloShaperProcessor::HoloShaperProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

HoloShaperProcessor::~HoloShaperProcessor() {}

void HoloShaperProcessor::prepareToPlay(double, int) {}
void HoloShaperProcessor::releaseResources() {}

void HoloShaperProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // v1 scaffold: dry passthrough. Trigger/LFO/curve DSP lands here next,
    // along with MIDI CC output via midiMessages.addEvent().
    juce::ignoreUnused(midiMessages);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples()); // placeholder: mute until DSP lands
}

juce::AudioProcessorEditor* HoloShaperProcessor::createEditor()
{
    return new HoloShaperEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HoloShaperProcessor();
}
