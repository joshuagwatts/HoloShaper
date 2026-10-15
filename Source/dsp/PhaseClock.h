#pragma once
// HoloShaper DSP — JUCE-independent plain C++17.
// Tempo-synced phase clock. From host: phase = fmod(ppq / beatsPerCycle, 1).
// Fallback: free-run at last-known BPM. retrigger() resets phase to 0.

#include <cmath>

namespace holoshaper {

class PhaseClock
{
public:
    void setBeatsPerCycle(double b) { if (b > 0.0) beatsPerCycle_ = b; }
    void setBpm(double bpm)         { if (bpm > 0.0) bpm_ = bpm; }
    double getBpm() const           { return bpm_; }
    double getBeatsPerCycle() const { return beatsPerCycle_; }

    void updateFromHost(double ppqPosition)
    {
        phase_ = std::fmod(ppqPosition / beatsPerCycle_, 1.0);
        if (phase_ < 0.0)
            phase_ += 1.0;
    }

    void advanceFree(double numSamples, double sampleRate)
    {
        if (sampleRate <= 0.0)
            return;
        phase_ += (bpm_ / 60.0) * (numSamples / sampleRate) / beatsPerCycle_;
        phase_ -= std::floor(phase_);
    }

    void retrigger() { phase_ = 0.0; }
    float getPhase() const { return (float) phase_; }

private:
    double beatsPerCycle_ = 2.0;
    double bpm_ = 120.0;
    double phase_ = 0.0;
};

} // namespace holoshaper
