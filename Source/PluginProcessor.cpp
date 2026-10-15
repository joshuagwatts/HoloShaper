#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
float lookupMod(const float* table, float phase)
{
    constexpr int N = holoshaper::CurveEvaluator::kTableSize;
    float pos = phase * (float) (N - 1);
    int i0 = (int) pos;
    if (i0 < 0) i0 = 0;
    if (i0 >= N - 1) i0 = N - 2;
    float frac = pos - (float) i0;
    return table[i0] + frac * (table[i0 + 1] - table[i0]);
}
} // namespace

HoloShaperProcessor::HoloShaperProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "HoloShaperState", createParameterLayout()),
      activeTable_(curveTableA_)
{
    renderCurveTable(); // render default duck curve into table A
}

HoloShaperProcessor::~HoloShaperProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout
HoloShaperProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "trigGain", "Trigger Gain",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.01f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "filterType", "Filter",
        juce::StringArray{ "Off", "Lowpass", "Highpass" }, 0));
    juce::NormalisableRange<float> freqRange(40.0f, 8000.0f);
    freqRange.setSkewForCentre(800.0f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "filterFreq", "Filter Freq", freqRange, 800.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "threshold", "Threshold",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "rmsTime", "RMS Time",
        juce::NormalisableRange<float>(1.0f, 200.0f, 0.5f), 10.0f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "mode", "Mode",
        juce::StringArray{ "Free", "Retrigger" }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "rate", "Rate",
        juce::StringArray{ "1/4", "1/2", "1/1", "2/1" }, 1));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "holdoff", "Holdoff",
        juce::NormalisableRange<float>(10.0f, 500.0f, 1.0f), 100.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "mix", "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "amount", "Amount",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        "ccEnable", "MIDI CC Enable", false));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        "ccNum", "CC Number", 0, 127, 1));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        "ccChan", "CC Channel", 1, 16, 1));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        "ccMin", "CC Min", 0, 127, 0));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        "ccMax", "CC Max", 0, 127, 127));

    return layout;
}

void HoloShaperProcessor::prepareToPlay(double sampleRate, int)
{
    detector_.prepare(sampleRate);
    lastCcValue_ = -1;
}

void HoloShaperProcessor::releaseResources() {}

void HoloShaperProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    const double sampleRate = getSampleRate();

    // Snapshot params once per block.
    const float trigGain  = apvts.getRawParameterValue("trigGain")->load();
    const int   filterIdx = (int) apvts.getRawParameterValue("filterType")->load();
    const float filterFreq = apvts.getRawParameterValue("filterFreq")->load();
    const float threshold = apvts.getRawParameterValue("threshold")->load();
    const float rmsTime   = apvts.getRawParameterValue("rmsTime")->load();
    const int   modeIdx   = (int) apvts.getRawParameterValue("mode")->load();
    const int   rateIdx   = juce::jlimit(0, 3, (int) apvts.getRawParameterValue("rate")->load());
    const float holdoff   = apvts.getRawParameterValue("holdoff")->load();
    const float mix       = apvts.getRawParameterValue("mix")->load();
    const float amount    = apvts.getRawParameterValue("amount")->load();
    const bool  ccEnable  = apvts.getRawParameterValue("ccEnable")->load() > 0.5f;
    const int   ccNum     = (int) apvts.getRawParameterValue("ccNum")->load();
    const int   ccChan    = (int) apvts.getRawParameterValue("ccChan")->load();
    const int   ccMin     = (int) apvts.getRawParameterValue("ccMin")->load();
    const int   ccMax     = (int) apvts.getRawParameterValue("ccMax")->load();

    detector_.setParams(trigGain,
                        (holoshaper::FilterType) juce::jlimit(0, 2, filterIdx),
                        filterFreq, threshold, rmsTime, holdoff);
    clock_.setBeatsPerCycle(kBeatsPerCycle[rateIdx]);
    const bool retrigMode = (modeIdx == 1);

    // Host transport.
    double ppq = -1.0;
    double bpm = 120.0;
    bool playing = false;
    if (auto* hostPlayHead = getPlayHead())
    {
        if (auto pos = hostPlayHead->getPosition())
        {
            playing = pos->getIsPlaying();
            ppq = pos->getPpqPosition().orFallback(-1.0);
            bpm = pos->getBpm().orFallback(120.0);
        }
    }
    clock_.setBpm(bpm);
    uiBpm_.store((float) bpm, std::memory_order_relaxed);

    const float* table = activeTable_.load(std::memory_order_acquire);
    const float* inL = buffer.getReadPointer(0);
    const float* inR = numChannels > 1 ? buffer.getReadPointer(1) : inL;
    float* outL = buffer.getWritePointer(0);
    float* outR = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    const double beatsPerSample = (bpm / 60.0) / sampleRate;
    float lastMod = 1.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float mono = 0.5f * (inL[i] + inR[i]);
        if (detector_.processSample(mono))
        {
            if (retrigMode)
                clock_.retrigger();
            triggerCount_.fetch_add(1, std::memory_order_relaxed);
        }

        if (playing && ppq >= 0.0)
            clock_.updateFromHost(ppq + i * beatsPerSample);
        else
            clock_.advanceFree(1.0, sampleRate);

        const float mod = lookupMod(table, clock_.getPhase());
        lastMod = mod;
        const float m = 1.0f - amount * (1.0f - mod);

        const float dryL = inL[i];
        outL[i] = dryL * (1.0f - mix) + (dryL * m) * mix;
        if (outR != nullptr)
        {
            const float dryR = inR[i];
            outR[i] = dryR * (1.0f - mix) + (dryR * m) * mix;
        }
    }

    uiPhase_.store(clock_.getPhase(), std::memory_order_relaxed);
    uiLevel_.store(detector_.getLevel(), std::memory_order_relaxed);

    if (ccEnable)
    {
        const int lo = juce::jmin(ccMin, ccMax);
        const int hi = juce::jmax(ccMin, ccMax);
        const int val = lo + (int) std::round(lastMod * (float) (hi - lo));
        if (val != lastCcValue_)
        {
            midiMessages.addEvent(juce::MidiMessage::controllerEvent(ccChan, ccNum, val), 0);
            lastCcValue_ = val;
        }
    }
}

void HoloShaperProcessor::setCurvePins(const std::vector<holoshaper::CurvePin>& pins)
{
    juce::ScopedLock sl(pinLock_);
    curve_.setPins(pins);
    renderCurveTable();
}

std::vector<holoshaper::CurvePin> HoloShaperProcessor::getCurvePins() const
{
    juce::ScopedLock sl(pinLock_);
    return curve_.getPins();
}

float HoloShaperProcessor::evaluateCurve(float phase) const
{
    juce::ScopedLock sl(pinLock_);
    return curve_.evaluate(phase);
}

void HoloShaperProcessor::renderCurveTable()
{
    // Ping-pong: render into the inactive buffer, then publish.
    float* inactive = (activeTable_.load(std::memory_order_relaxed) == curveTableA_)
                          ? curveTableB_
                          : curveTableA_;
    curve_.renderTable(inactive, holoshaper::CurveEvaluator::kTableSize);
    activeTable_.store(inactive, std::memory_order_release);
}

void HoloShaperProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    juce::XmlElement curveXml("HOLOSHAPER_CURVE");
    {
        juce::ScopedLock sl(pinLock_);
        for (const auto& p : curve_.getPins())
        {
            auto* e = curveXml.createNewChildElement("PIN");
            e->setAttribute("x", p.x);
            e->setAttribute("y", p.y);
            e->setAttribute("hox", p.handleOutX);
            e->setAttribute("hoy", p.handleOutY);
            e->setAttribute("hix", p.handleInX);
            e->setAttribute("hiy", p.handleInY);
        }
    }
    state.setProperty("curveXml", curveXml.toString(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void HoloShaperProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr)
        return;
    auto state = juce::ValueTree::fromXml(*xml);
    if (!state.isValid())
        return;

    apvts.replaceState(state);

    juce::String curveXmlStr = state.getProperty("curveXml", juce::String());
    if (curveXmlStr.isNotEmpty())
    {
        if (auto curveXml = juce::XmlDocument::parse(curveXmlStr))
        {
            std::vector<holoshaper::CurvePin> pins;
            for (auto* e : curveXml->getChildIterator())
            {
                if (!e->hasTagName("PIN"))
                    continue;
                holoshaper::CurvePin p;
                p.x = (float) e->getDoubleAttribute("x");
                p.y = (float) e->getDoubleAttribute("y");
                p.handleOutX = (float) e->getDoubleAttribute("hox");
                p.handleOutY = (float) e->getDoubleAttribute("hoy");
                p.handleInX  = (float) e->getDoubleAttribute("hix");
                p.handleInY  = (float) e->getDoubleAttribute("hiy");
                pins.push_back(p);
            }
            if (pins.size() >= 2)
                setCurvePins(pins);
        }
    }
}

juce::AudioProcessorEditor* HoloShaperProcessor::createEditor()
{
    return new HoloShaperEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HoloShaperProcessor();
}
