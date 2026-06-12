#pragma once

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <vector>

#include "tonestack/Node.h"
#include "tonestack/RtAssert.h"

namespace tonestack::nodes {

namespace detail {

// 47-tap halfband lowpass (Kaiser beta 9, cutoff at half the high-rate Nyquist):
// <0.001 dB ripple to 20 kHz at a 48 kHz base rate, -96 dB stopband. Used at the 2x stage,
// where the audio band sits closest to the fold.
inline constexpr float kHalfbandA[47] = {
    -1.26551015e-05f, 0.0f, 0.000118146289f, 0.0f, -0.000445001068f, 0.0f,
    0.00121825885f, 0.0f, -0.00277667512f, 0.0f, 0.00559669487f, 0.0f,
    -0.0103421511f, 0.0f, 0.0180126186f, 0.0f, -0.0304244802f, 0.0f,
    0.0519786238f, 0.0f, -0.0986861663f, 0.0f, 0.315764388f, 0.499996796f,
    0.315764388f, 0.0f, -0.0986861663f, 0.0f, 0.0519786238f, 0.0f,
    -0.0304244802f, 0.0f, 0.0180126186f, 0.0f, -0.0103421511f, 0.0f,
    0.00559669487f, 0.0f, -0.00277667512f, 0.0f, 0.00121825885f, 0.0f,
    -0.000445001068f, 0.0f, 0.000118146289f, 0.0f, -1.26551015e-05f,
};

// 25-tap halfband for the relaxed 4x stage (audio band far below the fold): -88 dB stopband.
inline constexpr float kHalfbandB[25] = {
    0.0f, -0.000211852666f, 0.0f, 0.00208302824f, 0.0f, -0.00934542877f,
    0.0f, 0.0294991158f, 0.0f, -0.0810651988f, 0.0f, 0.309046755f,
    0.499987163f, 0.309046755f, 0.0f, -0.0810651988f, 0.0f, 0.0294991158f,
    0.0f, -0.00934542877f, 0.0f, 0.00208302824f, 0.0f, -0.000211852666f,
    0.0f,
};

// One 2x resampling direction exploiting the halfband structure: every tap at the center's
// parity is zero except the ~0.5 center itself, so each output costs one dense-phase dot
// product plus a single center term instead of the full convolution. Per-channel history is
// a double-written ring (each sample stored at w and w+len), giving O(1) push and a
// contiguous newest-first window for the dot product. Upsampling applies a gain of 2 to
// compensate zero-stuffing; downsampling evaluates at even indices so a matched up/down
// pair contributes an integer overall delay.
class Halfband2x {
public:
    Halfband2x(const float* taps, int numTaps) : numTaps_(numTaps) {
        const int center = (numTaps - 1) / 2;
        denseParity_ = (center & 1) ^ 1;
        centerIdx_ = center >> 1;
        centerTap_ = taps[center];
        for (int k = denseParity_; k < numTaps; k += 2)
            dense_.push_back(taps[k]);
        for (int k = 1 - denseParity_; k < numTaps; k += 2)
            if (k != center && taps[k] != 0.0f)
                throw std::invalid_argument("Halfband2x: taps are not halfband");
        histLen_ = std::max(static_cast<int>(dense_.size()), centerIdx_ + 1);
    }

    void prepare(int channels) {
        hist_.assign(static_cast<size_t>(channels) * 2u * static_cast<size_t>(2 * histLen_),
                     0.0f);
        idx_.assign(static_cast<size_t>(channels) * 2u, 0);
    }
    void reset() noexcept {
        std::fill(hist_.begin(), hist_.end(), 0.0f);
        std::fill(idx_.begin(), idx_.end(), 0);
    }

    int delaySamples() const noexcept { return (numTaps_ - 1) / 2; } // at the high rate

    // n low-rate frames in -> 2n high-rate frames out.
    void upsample(int ch, const float* in, float* out, int n) noexcept {
        float* buf = plane(ch, 0);
        int& w = idx_[static_cast<size_t>(ch) * 2];
        const float* dp = dense_.data();
        const int denseLen = static_cast<int>(dense_.size());
        for (int i = 0; i < n; ++i) {
            push(buf, w, in[i]);
            const float* h = buf + w; // h[m] = x[i-m]
            float acc = 0.0f;         // y[2i+p] = 2 * sum_m taps[2m+p] * x[i-m]
            for (int m = 0; m < denseLen; ++m)
                acc += dp[m] * h[m];
            out[2 * i + denseParity_] = 2.0f * acc;
            out[2 * i + (1 - denseParity_)] = 2.0f * centerTap_ * h[centerIdx_];
        }
    }

    // 2n high-rate frames in -> n low-rate frames out.
    void downsample(int ch, const float* in, float* out, int n) noexcept {
        float* bufE = plane(ch, 0);
        float* bufO = plane(ch, 1);
        int& wE = idx_[static_cast<size_t>(ch) * 2];
        int& wO = idx_[static_cast<size_t>(ch) * 2 + 1];
        const float* dp = dense_.data();
        const int denseLen = static_cast<int>(dense_.size());
        for (int i = 0; i < n; ++i) {
            push(bufE, wE, in[2 * i]);
            // y[i] = sum_k taps[k] * u[2i-k]: even k reads the even-sample history
            // (hE[0] = u[2i]), odd k the odd one (hO[0] = u[2i-1]).
            const float* hE = bufE + wE;
            const float* hO = bufO + wO;
            const float* hd = denseParity_ == 0 ? hE : hO;
            const float* hc = denseParity_ == 0 ? hO : hE;
            float acc = centerTap_ * hc[centerIdx_];
            for (int m = 0; m < denseLen; ++m)
                acc += dp[m] * hd[m];
            out[i] = acc;
            push(bufO, wO, in[2 * i + 1]);
        }
    }

private:
    float* plane(int ch, int phase) noexcept {
        return hist_.data() + (static_cast<size_t>(ch) * 2u + static_cast<size_t>(phase)) *
                                  static_cast<size_t>(2 * histLen_);
    }
    void push(float* buf, int& w, float v) noexcept {
        w = w == 0 ? histLen_ - 1 : w - 1;
        buf[w] = v;
        buf[w + histLen_] = v;
    }

    int numTaps_;
    int denseParity_ = 0; // parity of the nonzero (non-center) taps
    int centerIdx_ = 0;   // center tap's index within its phase
    float centerTap_ = 0.0f;
    std::vector<float> dense_; // taps at denseParity_, in order
    int histLen_ = 0;
    std::vector<float> hist_; // [channels][2 phases][2*histLen_], double-written ring
    std::vector<int>   idx_;  // [channels][2 phases] write positions
};

} // namespace detail

// Runs the inner node at 4x the host rate between two cascaded 2x halfband stages. The
// inner node is prepared with the oversampled spec; this node's parameters() forwards the
// inner node's set. All scratch is sized in prepare().
class OversampledNode : public Node {
public:
    explicit OversampledNode(std::unique_ptr<Node> inner) : inner_(std::move(inner)) {
        if (inner_ == nullptr)
            throw std::invalid_argument("OversampledNode: inner node required");
    }

    static constexpr int factor = 4;

    NodeInfo info() const noexcept override { return {"oversample"}; }

    void prepare(const ProcessSpec& spec) override {
        upA_.prepare(spec.numChannels);
        upB_.prepare(spec.numChannels);
        downB_.prepare(spec.numChannels);
        downA_.prepare(spec.numChannels);

        const size_t ch = static_cast<size_t>(spec.numChannels);
        const size_t n = static_cast<size_t>(spec.maxBlockSize);
        buf2x_.assign(ch * n * 2, 0.0f);
        buf4x_.assign(ch * n * 4, 0.0f);
        ptrs4x_.assign(ch, nullptr);
        for (size_t c = 0; c < ch; ++c)
            ptrs4x_[c] = buf4x_.data() + c * n * 4;
        maxBlockSize_ = spec.maxBlockSize;

        inner_->prepare(ProcessSpec{spec.sampleRate * factor, spec.maxBlockSize * factor,
                                    spec.numChannels});
    }

    void reset() noexcept override {
        upA_.reset();
        upB_.reset();
        downB_.reset();
        downA_.reset();
        std::fill(buf2x_.begin(), buf2x_.end(), 0.0f);
        std::fill(buf4x_.begin(), buf4x_.end(), 0.0f);
        inner_->reset();
    }

    void process(AudioBlock& io) noexcept override {
        const int n = io.numFrames();
        TS_RT_ASSERT(n <= maxBlockSize_ &&
                     io.numChannels() <= static_cast<int>(ptrs4x_.size()));
        const int channels = std::min(io.numChannels(), static_cast<int>(ptrs4x_.size()));

        for (int ch = 0; ch < channels; ++ch) {
            float* b2 = channel2x(ch);
            upA_.upsample(ch, io.channel(ch), b2, n);
            upB_.upsample(ch, b2, ptrs4x_[static_cast<size_t>(ch)], 2 * n);
        }

        AudioBlock hi(ptrs4x_.data(), channels, n * factor);
        inner_->process(hi);

        for (int ch = 0; ch < channels; ++ch) {
            float* b2 = channel2x(ch);
            downB_.downsample(ch, ptrs4x_[static_cast<size_t>(ch)], b2, 2 * n);
            downA_.downsample(ch, b2, io.channel(ch), n);
        }
    }

    ParameterSet& parameters() noexcept override { return inner_->parameters(); }

    // Matched up/down pairs: A contributes its delay at 2x twice -> dA base samples; B at 4x
    // twice -> dB/2; plus the inner node's latency referred to the base rate.
    int latencySamples() const noexcept override {
        const int fir = upA_.delaySamples() + upB_.delaySamples() / 2;
        return fir + (inner_->latencySamples() + factor - 1) / factor;
    }

    Node& inner() noexcept { return *inner_; }

private:
    float* channel2x(int ch) noexcept {
        return buf2x_.data() +
               static_cast<size_t>(ch) * static_cast<size_t>(maxBlockSize_) * 2;
    }

    std::unique_ptr<Node> inner_;
    detail::Halfband2x upA_{detail::kHalfbandA, 47};
    detail::Halfband2x upB_{detail::kHalfbandB, 25};
    detail::Halfband2x downB_{detail::kHalfbandB, 25};
    detail::Halfband2x downA_{detail::kHalfbandA, 47};

    std::vector<float>  buf2x_;
    std::vector<float>  buf4x_;
    std::vector<float*> ptrs4x_;
    int maxBlockSize_ = 0;
};

} // namespace tonestack::nodes
