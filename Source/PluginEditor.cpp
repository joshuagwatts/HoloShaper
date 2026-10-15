#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
const juce::Colour kBg(0xff0d1117);
const juce::Colour kPanel(0xff161b22);
const juce::Colour kGrid(0xff232a35);
const juce::Colour kAccent(0xff58a6ff);
const juce::Colour kCurve(0xff79c0ff);
const juce::Colour kText(0xffc9d1d9);
const juce::Colour kDim(0xff8b949e);
const juce::Colour kLedOn(0xff3fb950);
} // namespace

//==============================================================================
// CurveEditorComponent
//==============================================================================

CurveEditorComponent::CurveEditorComponent(HoloShaperProcessor& p)
    : processor_(p)
{
    startTimerHz(30);
}

juce::Rectangle<float> CurveEditorComponent::plotArea() const
{
    return getLocalBounds().reduced(12).toFloat();
}

juce::Point<float> CurveEditorComponent::toNorm(juce::Point<float> pos) const
{
    auto r = plotArea();
    float nx = (pos.x - r.getX()) / r.getWidth();
    float ny = 1.0f - (pos.y - r.getY()) / r.getHeight();
    return { juce::jlimit(0.0f, 1.0f, nx), juce::jlimit(0.0f, 1.0f, ny) };
}

juce::Point<float> CurveEditorComponent::toScreen(float nx, float ny) const
{
    auto r = plotArea();
    return { r.getX() + nx * r.getWidth(), r.getY() + (1.0f - ny) * r.getHeight() };
}

void CurveEditorComponent::editPins(
    const std::function<void(holoshaper::CurveEvaluator&)>& fn)
{
    holoshaper::CurveEvaluator ce;
    ce.setPins(processor_.getCurvePins());
    fn(ce);
    processor_.setCurvePins(ce.getPins());
}

void CurveEditorComponent::paint(juce::Graphics& g)
{
    g.fillAll(kBg);
    auto r = plotArea();

    // grid: 8 vertical (8th-note-ish), 4 horizontal (25% steps)
    g.setColour(kGrid);
    for (int i = 0; i <= 8; ++i)
    {
        float x = r.getX() + r.getWidth() * (float) i / 8.0f;
        g.drawVerticalLine((int) x, r.getY(), r.getBottom());
    }
    for (int i = 0; i <= 4; ++i)
    {
        float y = r.getY() + r.getHeight() * (float) i / 4.0f;
        g.drawHorizontalLine((int) y, r.getX(), r.getRight());
    }
    g.setColour(kPanel);
    g.drawRect(r, 1.0f);

    // curve
    juce::Path path;
    const int steps = 256;
    for (int i = 0; i <= steps; ++i)
    {
        float ph = (float) i / (float) steps;
        float v = processor_.evaluateCurve(ph);
        auto pt = toScreen(ph, v);
        if (i == 0) path.startNewSubPath(pt);
        else        path.lineTo(pt);
    }
    g.setColour(kCurve);
    g.strokePath(path, juce::PathStrokeType(2.5f));

    auto pins = processor_.getCurvePins();

    // handles
    for (const auto& p : pins)
    {
        auto pc = toScreen(p.x, p.y);
        g.setColour(kDim);
        if (std::abs(p.handleOutX) + std::abs(p.handleOutY) > 1e-4f)
        {
            auto h = toScreen(p.x + p.handleOutX, p.y + p.handleOutY);
            g.drawLine(pc.x, pc.y, h.x, h.y, 1.0f);
            g.fillRect(juce::Rectangle<float>(5.0f, 5.0f).withCentre(h));
        }
        if (std::abs(p.handleInX) + std::abs(p.handleInY) > 1e-4f)
        {
            auto h = toScreen(p.x + p.handleInX, p.y + p.handleInY);
            g.drawLine(pc.x, pc.y, h.x, h.y, 1.0f);
            g.fillRect(juce::Rectangle<float>(5.0f, 5.0f).withCentre(h));
        }
    }

    // pins
    for (const auto& p : pins)
    {
        auto pc = toScreen(p.x, p.y);
        g.setColour(kAccent);
        g.fillEllipse(juce::Rectangle<float>(13.0f, 13.0f).withCentre(pc));
        g.setColour(kBg);
        g.fillEllipse(juce::Rectangle<float>(6.0f, 6.0f).withCentre(pc));
    }

    // playhead
    float phase = processor_.getUiPhase();
    float px = r.getX() + phase * r.getWidth();
    g.setColour(juce::Colours::white);
    g.fillRect(juce::Rectangle<float>(2.0f, r.getHeight())
                   .withX(px - 1.0f).withY(r.getY()));
}

void CurveEditorComponent::mouseDown(const juce::MouseEvent& e)
{
    auto n = toNorm(e.position);
    auto r = plotArea();
    float tolX = 16.0f / r.getWidth();
    float tolY = 16.0f / r.getHeight();

    holoshaper::CurveEvaluator ce;
    ce.setPins(processor_.getCurvePins());

    int idx = ce.findPinNear(n.x, n.y, tolX, tolY);
    if (idx >= 0)
    {
        dragMode_ = DragMode::pin;
        dragIndex_ = idx;
        return;
    }

    int seg = ce.findSegmentAt(n.x);
    if (seg >= 0)
    {
        dragMode_ = DragMode::bend;
        dragIndex_ = seg;
        dragStart_ = e.position;
        float midX = 0.5f * (ce.getPin(seg).x + ce.getPin(seg + 1).x);
        bendLeft_ = (n.x < midX);
        const auto& pin = ce.getPin(bendLeft_ ? seg : seg + 1);
        if (bendLeft_) { origHandleX_ = pin.handleOutX; origHandleY_ = pin.handleOutY; }
        else           { origHandleX_ = pin.handleInX;  origHandleY_ = pin.handleInY;  }
    }
}

void CurveEditorComponent::mouseDrag(const juce::MouseEvent& e)
{
    auto n = toNorm(e.position);
    auto r = plotArea();

    if (dragMode_ == DragMode::pin && dragIndex_ >= 0)
    {
        int idx = dragIndex_;
        editPins([idx, n](holoshaper::CurveEvaluator& ce) { ce.movePin(idx, n.x, n.y); });
    }
    else if (dragMode_ == DragMode::bend && dragIndex_ >= 0)
    {
        float dx = (e.position.x - dragStart_.x) / r.getWidth() * 1.5f;
        float dy = -(e.position.y - dragStart_.y) / r.getHeight() * 1.5f;
        int seg = dragIndex_;
        bool left = bendLeft_;
        float ox = origHandleX_, oy = origHandleY_;
        editPins([seg, left, ox, oy, dx, dy](holoshaper::CurveEvaluator& ce)
        {
            ce.setHandle(left ? seg : seg + 1, left, ox + dx, oy + dy);
        });
    }
    repaint();
}

void CurveEditorComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    auto n = toNorm(e.position);
    auto r = plotArea();

    holoshaper::CurveEvaluator probe;
    probe.setPins(processor_.getCurvePins());
    int idx = probe.findPinNear(n.x, n.y, 16.0f / r.getWidth(), 16.0f / r.getHeight());

    if (idx >= 0)
    {
        editPins([idx](holoshaper::CurveEvaluator& ce) { ce.removePin(idx); });
    }
    else
    {
        float x = juce::jlimit(0.01f, 0.99f, n.x);
        float y = processor_.evaluateCurve(x); // insert on the curve
        editPins([x, y](holoshaper::CurveEvaluator& ce) { ce.addPin(x, y); });
    }
    dragMode_ = DragMode::none;
    repaint();
}

//==============================================================================
// TriggerMeterComponent
//==============================================================================

TriggerMeterComponent::TriggerMeterComponent(HoloShaperProcessor& p)
    : processor_(p)
{
    lastCount_ = p.getTriggerCount();
    startTimerHz(30);
}

void TriggerMeterComponent::timerCallback()
{
    auto c = processor_.getTriggerCount();
    if (c != lastCount_)
    {
        lastCount_ = c;
        lastFireMs_ = juce::Time::getMillisecondCounter();
    }
    float target = juce::jmin(processor_.getUiLevel(), 1.2f) / 1.2f;
    shownLevel_ += 0.35f * (target - shownLevel_);
    repaint();
}

void TriggerMeterComponent::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour(kPanel);
    g.fillRoundedRectangle(r, 4.0f);

    float w = r.getWidth() * juce::jlimit(0.0f, 1.0f, shownLevel_);
    g.setColour(kAccent);
    g.fillRoundedRectangle(r.withWidth(w), 4.0f);

    bool lit = (juce::Time::getMillisecondCounter() - lastFireMs_) < 150;
    g.setColour(lit ? kLedOn : kGrid);
    float d = r.getHeight() - 6.0f;
    if (d > 2.0f)
        g.fillEllipse(juce::Rectangle<float>(d, d).withCentre(
            juce::Point<float>(r.getRight() - d * 0.5f - 4.0f, r.getCentreY())));
}

//==============================================================================
// HoloShaperEditor
//==============================================================================

HoloShaperEditor::HoloShaperEditor(HoloShaperProcessor& p)
    : AudioProcessorEditor(p),
      processor_(p),
      apvts_(p.apvts),
      curveEditor_(p),
      meter_(p)
{
    setSize(900, 620);

    addAndMakeVisible(curveEditor_);

    triggerGroup_.setText("Trigger");
    syncGroup_.setText("Sync");
    volumeGroup_.setText("Volume");
    midiGroup_.setText("MIDI CC Out");
    addAndMakeVisible(triggerGroup_);
    addAndMakeVisible(syncGroup_);
    addAndMakeVisible(volumeGroup_);
    addAndMakeVisible(midiGroup_);

    addAndMakeVisible(meter_);

    bpmLabel_.setText("120.0 BPM", juce::dontSendNotification);
    bpmLabel_.setFont(juce::Font(22.0f, juce::Font::bold));
    bpmLabel_.setColour(juce::Label::textColourId, kText);
    bpmLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(bpmLabel_);

    // Trigger (sliders 0-4, combos 0-1)
    addRotary("trigGain",  "Gain",    triggerGroup_);
    addRotary("filterFreq","Freq",    triggerGroup_);
    addRotary("threshold", "Thresh",  triggerGroup_);
    addRotary("rmsTime",   "RMS ms",  triggerGroup_);
    addRotary("holdoff",   "Hold ms", triggerGroup_);
    addCombo("filterType", { "Off", "Lowpass", "Highpass" }, triggerGroup_);
    addCombo("mode",       { "Free", "Retrigger" },          triggerGroup_);

    // Sync (combo 2)
    addCombo("rate", { "1/4", "1/2", "1/1", "2/1" }, syncGroup_);

    // Volume (sliders 5-6)
    addRotary("mix",    "Mix",    volumeGroup_);
    addRotary("amount", "Amount", volumeGroup_);

    // MIDI (sliders 7-10)
    ccEnableButton_.setColour(juce::ToggleButton::textColourId, kText);
    addAndMakeVisible(ccEnableButton_);
    ccEnableAttachment_ = std::make_unique<
        juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts_, "ccEnable", ccEnableButton_);
    addRotary("ccNum",  "CC #", midiGroup_);
    addRotary("ccChan", "Ch",   midiGroup_);
    addRotary("ccMin",  "Min",  midiGroup_);
    addRotary("ccMax",  "Max",  midiGroup_);

    startTimerHz(10);
}

juce::Slider& HoloShaperEditor::addRotary(const juce::String& paramId,
                                         const juce::String& label,
                                         juce::Component& parent,
                                         const juce::String& /*suffix*/)
{
    auto* s = new juce::Slider();
    s->setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 16);
    s->setColour(juce::Slider::rotarySliderFillColourId, kAccent);
    s->setColour(juce::Slider::textBoxTextColourId, kText);
    s->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    sliders_.add(s);
    // NOTE: controls are direct children of the editor (not of the
    // GroupComponents) — positioned in editor coordinates in resized().
    juce::ignoreUnused(parent);
    addAndMakeVisible(s);

    auto* l = new juce::Label();
    l->setText(label, juce::dontSendNotification);
    l->setColour(juce::Label::textColourId, kDim);
    l->setFont(juce::Font(11.0f));
    l->setJustificationType(juce::Justification::centred);
    sliderLabels_.add(l);
    addAndMakeVisible(l);

    sliderAttachments_.push_back(
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts_, paramId, *s));
    return *s;
}

juce::ComboBox& HoloShaperEditor::addCombo(const juce::String& paramId,
                                          const juce::StringArray& items,
                                          juce::Component& parent)
{
    auto* c = new juce::ComboBox();
    c->addItemList(items, 1);
    c->setColour(juce::ComboBox::backgroundColourId, kPanel);
    c->setColour(juce::ComboBox::textColourId, kText);
    combos_.add(c);
    juce::ignoreUnused(parent);
    addAndMakeVisible(c);

    comboAttachments_.push_back(
        std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            apvts_, paramId, *c));
    return *c;
}

void HoloShaperEditor::paint(juce::Graphics& g)
{
    g.fillAll(kBg);
}

void HoloShaperEditor::resized()
{
    const int W = getWidth();
    curveEditor_.setBounds(0, 0, W, 372);

    const int top = 380, h = 232;
    triggerGroup_.setBounds(8, top, 300, h);
    syncGroup_.setBounds(316, top, 130, h);
    volumeGroup_.setBounds(454, top, 170, h);
    midiGroup_.setBounds(632, top, W - 640, h);

    // NOTE: every knob/combo/label below is a direct child of the editor
    // (NOT of the GroupComponents) and is positioned in editor coordinates.

    // Trigger group: meter + 5 rotaries (sliders 0-4) + 2 combos (0-1)
    {
        const int gx = triggerGroup_.getX();
        const int gy = triggerGroup_.getY();
        auto r = triggerGroup_.getLocalBounds().reduced(8);
        r.removeFromTop(14);
        meter_.setBounds(gx + r.getX(), gy + r.getY(), r.getWidth(), 16);
        r.removeFromTop(22);
        const int n = 5;
        int sw = r.getWidth() / n;
        for (int i = 0; i < n && i < sliders_.size(); ++i)
        {
            int x = gx + r.getX() + i * sw;
            int y = gy + r.getY();
            sliders_[i]->setBounds(x, y, sw, 64);
            sliderLabels_[i]->setBounds(x, y + 62, sw, 14);
        }
        int cy = gy + r.getY() + 84;
        if (combos_.size() >= 2)
        {
            combos_[0]->setBounds(gx + r.getX(), cy, r.getWidth() / 2 - 4, 24);
            combos_[1]->setBounds(gx + r.getX() + r.getWidth() / 2 + 4, cy,
                                 r.getWidth() / 2 - 4, 24);
        }
    }

    // Sync group: rate combo (combo 2) + BPM label
    {
        const int gx = syncGroup_.getX();
        const int gy = syncGroup_.getY();
        auto r = syncGroup_.getLocalBounds().reduced(8);
        r.removeFromTop(14);
        if (combos_.size() >= 3)
            combos_[2]->setBounds(gx + r.getX(), gy + r.getY(), r.getWidth(), 24);
        bpmLabel_.setBounds(gx + r.getX(), gy + r.getY() + 40, r.getWidth(), 30);
    }

    // Volume group: sliders 5-6
    {
        const int gx = volumeGroup_.getX();
        const int gy = volumeGroup_.getY();
        auto r = volumeGroup_.getLocalBounds().reduced(8);
        r.removeFromTop(14);
        const int n = 2;
        int sw = r.getWidth() / n;
        for (int i = 0; i < n; ++i)
        {
            int si = 5 + i;
            if (si < sliders_.size())
            {
                int x = gx + r.getX() + i * sw;
                int y = gy + r.getY();
                sliders_[si]->setBounds(x, y, sw, 76);
                sliderLabels_[si]->setBounds(x, y + 74, sw, 14);
            }
        }
    }

    // MIDI group: toggle + sliders 7-10
    {
        const int gx = midiGroup_.getX();
        const int gy = midiGroup_.getY();
        auto r = midiGroup_.getLocalBounds().reduced(8);
        r.removeFromTop(14);
        ccEnableButton_.setBounds(gx + r.getX(), gy + r.getY(), r.getWidth(), 24);
        int ry = gy + r.getY() + 32;
        const int n = 4;
        int sw = r.getWidth() / n;
        for (int i = 0; i < n; ++i)
        {
            int si = 7 + i;
            if (si < sliders_.size())
            {
                int x = gx + r.getX() + i * sw;
                sliders_[si]->setBounds(x, ry, sw, 76);
                sliderLabels_[si]->setBounds(x, ry + 74, sw, 14);
            }
        }
    }
}

void HoloShaperEditor::timerCallback()
{
    float bpm = processor_.getUiBpm();
    bpmLabel_.setText(juce::String(bpm, 1) + " BPM", juce::dontSendNotification);
}
