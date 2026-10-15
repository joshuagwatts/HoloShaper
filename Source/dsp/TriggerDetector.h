#pragma once
// HoloShaper DSP — JUCE-independent plain C++17.
// Audio trigger: mono sum -> gain -> one-pole LP/HP/off -> RMS envelope ->
// threshold w/ hysteresis -> rising edge -> retrigger holdoff.

#include <algorithm>
#include <cmath>

namespace holoshaper {

enum class FilterType { Off = 0, Lowpass = 1, Highpass = 2 };

class TriggerDetector
{
public:
    void prepare(double sampleRate)
    {
        sampleRate_ = (sampleRate > 0.0) ? sampleRate : 44100.0;
        reset();
    }

    void reset()
    {
        lp_ = 0.0f;
        env_ = 0.0f;
        level_ = 0.0f;
        above_ = false;
        holdoffLeft_ = 0;
    }

    void setParams(float gain, FilterType type, float freqHz,
                   float threshold01, float rmsTimeMs, float holdoffMs)
    {
        gain_ = gain;
        type_ = type;
        threshold_ = std::min(std::max(threshold01, 0.0f), 1.0f);

        float f = std::max(freqHz, 1.0f);
        lpA_ = 1.0f - std::exp(-2.0f * 3.14159265f * f / (float) sampleRate_);

        float tau = std::max(rmsTimeMs, 0.5f) / 1000.0f;
        rmsA_ = 1.0f - std::exp(-1.0f / (tau * (float) sampleRate_));

        holdoffSamples_ = (int) (std::max(holdoffMs, 0.0f) / 1000.0f * sampleRate_);
    }

    // Returns true on the exact sample the trigger fires.
    bool processSample(float monoSample)
    {
        float x = monoSample * gain_;

        if (type_ == FilterType::Lowpass)
        {
            lp_ += lpA_ * (x - lp_);
            x = lp_;
        }
        else if (type_ == FilterType::Highpass)
        {
            lp_ += lpA_ * (x - lp_);
            x = x - lp_;
        }

        env_ += rmsA_ * (x * x - env_);
        level_ = std::sqrt(std::max(env_, 0.0f));
        if (level_ > 2.0f) level_ = 2.0f;

        bool fired = false;
        if (!above_ && level_ > threshold_)
        {
            above_ = true;
            if (holdoffLeft_ <= 0)
            {
                fired = true;
                holdoffLeft_ = holdoffSamples_;
            }
        }
        else if (above_ && level_ < threshold_ * 0.6f)
        {
            above_ = false;
        }

        if (holdoffLeft_ > 0)
            --holdoffLeft_;

        return fired;
    }

    float getLevel() const { return level_; }

private:
    double sampleRate_ = 44100.0;
    float gain_ = 1.0f;
    FilterType type_ = FilterType::Off;
    float threshold_ = 0.5f;
    float lpA_ = 0.0f;
    float rmsA_ = 0.0f;
    int holdoffSamples_ = 0;

    float lp_ = 0.0f;
    float env_ = 0.0f;
    float level_ = 0.0f;
    bool above_ = false;
    int holdoffLeft_ = 0;
};

} // namespace holoshaper
