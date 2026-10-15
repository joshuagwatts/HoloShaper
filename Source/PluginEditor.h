#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_graphics/juce_graphics.h>

#include <functional>
#include <memory>

#include "dsp/CurveEvaluator.h"

class HoloShaperProcessor;

// Bezier curve editor: drag pins, drag segments to bend, double-click to
// add/delete pins. Playhead follows the processor phase at 30 fps.
class CurveEditorComponent : public juce::Component,
                             private juce::Timer
{
public:
    explicit CurveEditorComponent(HoloShaperProcessor& p);
    ~CurveEditorComponent() override = default;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    void timerCallback() override { repaint(); }
    void editPins(const std::function<void(holoshaper::CurveEvaluator&)>& fn);
    juce::Point<float> toNorm(juce::Point<float> pos) const;
    juce::Point<float> toScreen(float nx, float ny) const;
    juce::Rectangle<float> plotArea() const;

    HoloShaperProcessor& processor_;

    enum class DragMode { none, pin, bend };
    DragMode dragMode_ = DragMode::none;
    int dragIndex_ = -1;      // pin index (pin mode) or segment index (bend mode)
    bool bendLeft_ = true;
    juce::Point<float> dragStart_;
    float origHandleX_ = 0.0f, origHandleY_ = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CurveEditorComponent)
};

// Small live trigger level meter + fire LED.
class TriggerMeterComponent : public juce::Component,
                              private juce::Timer
{
public:
    explicit TriggerMeterComponent(HoloShaperProcessor& p);
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    HoloShaperProcessor& processor_;
    juce::int64 lastCount_ = 0;
    juce::uint32 lastFireMs_ = 0;
    float shownLevel_ = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TriggerMeterComponent)
};

class HoloShaperEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit HoloShaperEditor(HoloShaperProcessor&);
    ~HoloShaperEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    juce::Slider& addRotary(const juce::String& paramId, const juce::String& label,
                            juce::Component& parent, const juce::String& suffix = {});
    juce::ComboBox& addCombo(const juce::String& paramId,
                             const juce::StringArray& items,
                             juce::Component& parent);

    HoloShaperProcessor& processor_;
    juce::AudioProcessorValueTreeState& apvts_;

    CurveEditorComponent curveEditor_;
    TriggerMeterComponent meter_;

    juce::GroupComponent triggerGroup_, syncGroup_, volumeGroup_, midiGroup_;
    juce::Label bpmLabel_;

    juce::OwnedArray<juce::Slider> sliders_;
    juce::OwnedArray<juce::ComboBox> combos_;
    juce::OwnedArray<juce::Label> sliderLabels_;
    juce::ToggleButton ccEnableButton_{ "Send MIDI CC" };

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments_;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ccEnableAttachment_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HoloShaperEditor)
};
