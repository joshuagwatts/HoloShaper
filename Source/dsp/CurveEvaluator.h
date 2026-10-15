#pragma once
// HoloShaper DSP — JUCE-independent plain C++17.
// Bezier pin curve: pins in 0..1 with per-segment cubic bezier handles.

#include <algorithm>
#include <cmath>
#include <vector>

namespace holoshaper {

struct CurvePin
{
    float x = 0.0f;          // 0..1 phase
    float y = 0.0f;          // 0..1 value
    float handleOutX = 0.0f; // bezier control offset for the outgoing segment
    float handleOutY = 0.0f;
    float handleInX  = 0.0f; // bezier control offset for the incoming segment
    float handleInY  = 0.0f;
};

class CurveEvaluator
{
public:
    static constexpr int   kTableSize   = 512;
    static constexpr float kHandleLimit = 0.5f;
    static constexpr float kPinMergeDist = 0.01f;

    CurveEvaluator() { setPins(defaultDuckCurve()); }

    void setPins(const std::vector<CurvePin>& in)
    {
        pins_ = in;
        for (auto& p : pins_)
        {
            p.x = clamp01(p.x);
            p.y = clamp01(p.y);
            clampHandles(p);
        }
        std::sort(pins_.begin(), pins_.end(),
                  [](const CurvePin& a, const CurvePin& b) { return a.x < b.x; });
        // merge near-duplicate x positions (keep the first)
        std::vector<CurvePin> merged;
        for (auto& p : pins_)
        {
            if (!merged.empty() && std::fabs(p.x - merged.back().x) < kPinMergeDist)
                continue;
            merged.push_back(p);
        }
        pins_ = merged;
        if (pins_.size() < 2)
            pins_ = defaultDuckCurve();
    }

    const std::vector<CurvePin>& getPins() const { return pins_; }
    int getNumPins() const { return (int) pins_.size(); }

    CurvePin getPin(int index) const
    {
        if (index < 0 || index >= (int) pins_.size())
            return CurvePin{};
        return pins_[index];
    }

    // Returns new pin index, or -1 if too close to an existing pin.
    int addPin(float x, float y)
    {
        x = clamp01(x);
        y = clamp01(y);
        for (auto& p : pins_)
            if (std::fabs(p.x - x) < kPinMergeDist)
                return -1;
        CurvePin np;
        np.x = x;
        np.y = y;
        auto it = std::upper_bound(pins_.begin(), pins_.end(), x,
                                  [](float v, const CurvePin& p) { return v < p.x; });
        int idx = (int) (it - pins_.begin());
        pins_.insert(it, np);
        return idx;
    }

    bool removePin(int index)
    {
        if (pins_.size() <= 2)
            return false;
        if (index < 0 || index >= (int) pins_.size())
            return false;
        pins_.erase(pins_.begin() + index);
        return true;
    }

    void movePin(int index, float x, float y)
    {
        if (index < 0 || index >= (int) pins_.size())
            return;
        float lo = (index > 0) ? pins_[index - 1].x + 0.004f : 0.0f;
        float hi = (index + 1 < (int) pins_.size()) ? pins_[index + 1].x - 0.004f : 1.0f;
        pins_[index].x = std::min(std::max(x, lo), hi);
        pins_[index].y = clamp01(y);
    }

    void setHandle(int index, bool isOut, float dx, float dy)
    {
        if (index < 0 || index >= (int) pins_.size())
            return;
        dx = std::min(std::max(dx, -kHandleLimit), kHandleLimit);
        dy = std::min(std::max(dy, -kHandleLimit), kHandleLimit);
        if (isOut) { pins_[index].handleOutX = dx; pins_[index].handleOutY = dy; }
        else       { pins_[index].handleInX  = dx; pins_[index].handleInY  = dy; }
    }

    // Index of nearest pin within (tolX, tolY), else -1.
    int findPinNear(float x, float y, float tolX, float tolY) const
    {
        int best = -1;
        float bestD = 1e9f;
        for (int i = 0; i < (int) pins_.size(); ++i)
        {
            float dx = std::fabs(pins_[i].x - x) / tolX;
            float dy = std::fabs(pins_[i].y - y) / tolY;
            float d = dx * dx + dy * dy;
            if (d < 1.0f && d < bestD) { bestD = d; best = i; }
        }
        return best;
    }

    // Segment index i such that pins[i].x <= x <= pins[i+1].x, else -1.
    int findSegmentAt(float x) const
    {
        for (int i = 0; i + 1 < (int) pins_.size(); ++i)
            if (x >= pins_[i].x && x <= pins_[i + 1].x)
                return i;
        return -1;
    }

    float evaluate(float phase) const
    {
        if (pins_.empty())
            return 1.0f;
        phase = clamp01(phase);
        if (phase <= pins_.front().x) return pins_.front().y;
        if (phase >= pins_.back().x)  return pins_.back().y;

        int seg = findSegmentAt(phase);
        if (seg < 0) return pins_.back().y;

        const CurvePin& A = pins_[seg];
        const CurvePin& B = pins_[seg + 1];
        if (B.x - A.x < 1e-9f)
            return A.y;

        const float x0 = A.x, y0 = A.y;
        const float x1 = x0 + A.handleOutX, y1 = y0 + A.handleOutY;
        const float x2 = B.x + B.handleInX,  y2 = B.y + B.handleInY;
        const float x3 = B.x, y3 = B.y;

        // Invert x(s) by bisection (x is monotonic for sane handles).
        float lo = 0.0f, hi = 1.0f;
        for (int i = 0; i < 20; ++i)
        {
            float mid = 0.5f * (lo + hi);
            if (bezier1(x0, x1, x2, x3, mid) < phase) lo = mid;
            else                                      hi = mid;
        }
        float s = 0.5f * (lo + hi);
        return clamp01(bezier1(y0, y1, y2, y3, s));
    }

    void renderTable(float* table, int size) const
    {
        if (size < 2 || table == nullptr)
            return;
        for (int i = 0; i < size; ++i)
            table[i] = evaluate((float) i / (float) (size - 1));
    }

    static std::vector<CurvePin> defaultDuckCurve()
    {
        // Duck: 1.0 -> dip -> 1.0, 4 pins.
        CurvePin p0, p1, p2, p3;
        p0.x = 0.00f; p0.y = 1.00f; p0.handleOutX =  0.06f; p0.handleOutY =  0.00f;
        p1.x = 0.18f; p1.y = 0.06f; p1.handleInX  = -0.05f; p1.handleInY  =  0.02f;
                                            p1.handleOutX =  0.06f; p1.handleOutY = -0.02f;
        p2.x = 0.42f; p2.y = 0.92f; p2.handleInX  = -0.06f; p2.handleInY  =  0.02f;
                                            p2.handleOutX =  0.10f; p2.handleOutY =  0.00f;
        p3.x = 1.00f; p3.y = 1.00f; p3.handleInX  = -0.10f; p3.handleInY  =  0.00f;
        return { p0, p1, p2, p3 };
    }

private:
    std::vector<CurvePin> pins_;

    static float clamp01(float v) { return std::min(std::max(v, 0.0f), 1.0f); }

    static void clampHandles(CurvePin& p)
    {
        p.handleOutX = std::min(std::max(p.handleOutX, -kHandleLimit), kHandleLimit);
        p.handleOutY = std::min(std::max(p.handleOutY, -kHandleLimit), kHandleLimit);
        p.handleInX  = std::min(std::max(p.handleInX,  -kHandleLimit), kHandleLimit);
        p.handleInY  = std::min(std::max(p.handleInY,  -kHandleLimit), kHandleLimit);
    }

    static float bezier1(float p0, float p1, float p2, float p3, float s)
    {
        float u = 1.0f - s;
        return u*u*u*p0 + 3.0f*u*u*s*p1 + 3.0f*u*s*s*p2 + s*s*s*p3;
    }
};

} // namespace holoshaper
