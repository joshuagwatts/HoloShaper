#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <vector>

#include "dsp/CurveEvaluator.h"
#include "dsp/TriggerDetector.h"
#include "dsp/PhaseClock.h"

// HoloShaper v1.1 — volume shaper + MIDI CC out.
// Single lane: audio trigger -> tempo-synced phase -> bezier curve ->
// gain shaping + MIDI CC. Dry passthrough is always preserved.
class HoloShaperProcessor : public juce::AudioProcessor
{
public:
    HoloShaperProcessor();
    ~HoloShaperProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "HoloShaper"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // --- Editor interface (message thread) ---
    void setCurvePins(const std::vector<holoshaper::CurvePin>& pins);
    std::vector<holoshaper::CurvePin> getCurvePins() const;
    float evaluateCurve(float phase) const;

    float getUiPhase() const  { return uiPhase.load(std::memory_order_relaxed); }
    float getUiLevel() const  { return uiLevel.load(std::memory_order_relaxed); }
    float getUiBpm() const    { return uiBpm.load(std::memory_order_relaxed); }
    juce::int64 getTriggerCount() const
    {
        return triggerCount.load(std::memory_order_relaxed);
    }

    static constexpr double kBeatsPerCycle[4] = { 1.0, 2.0, 4.0, 8.0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void renderCurveTable(); // message thread: render pins -> inactive table, swap

    // DSP (audio thread)
    holoshaper::TriggerDetector detector_;
    holoshaper::PhaseClock clock_;

    // Curve lookup double buffer: message thread renders the inactive one,
    // then publishes via activeTable_. Audio thread only reads activeTable_.
    float curveTableA_[holoshaper::CurveEvaluator::kTableSize];
    float curveTableB_[holoshaper::CurveEvaluator::kTableSize];
    std::atomic<float*> activeTable_;

    // Pins + evaluator live on the message thread; guarded for editor access.
    mutable juce::CriticalSection pinLock_;
    holoshaper::CurveEvaluator curve_;

    // UI readouts
    std::atomic<float> uiPhase_{ 0.0f };
    std::atomic<float> uiLevel_{ 0.0f };
    std::atomic<float> uiBpm_{ 120.0f };
    std::atomic<juce::int64> triggerCount_{ 0 };

    int lastCcValue_ = -1; // audio thread only

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HoloShaperProcessor)
};
