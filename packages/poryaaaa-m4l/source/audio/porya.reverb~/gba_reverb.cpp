#include "gba_reverb.hpp"

#include <algorithm>
#include <cmath>

namespace porya {

GbaReverb::GbaReverb()
{
    configureDelay();
}

void GbaReverb::reset()
{
    std::fill(bufL_.begin(), bufL_.end(), 0);
    std::fill(bufR_.begin(), bufR_.end(), 0);
    pos_ = 0;
}

void GbaReverb::setAmount(long amount) noexcept
{
    amount_ = std::clamp(amount, 0L, 127L);
}

void GbaReverb::setHostSampleRate(double sampleRate)
{
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0) {
        sampleRate = kHostRateHz;
    }
    if (sampleRate == hostSampleRate_) {
        return;
    }

    hostSampleRate_ = sampleRate;
    configureDelay();
}

void GbaReverb::setRateMode(ReverbRateMode mode)
{
    if (mode != ReverbRateMode::Host) {
        mode = ReverbRateMode::Original;
    }
    if (mode == rateMode_) {
        return;
    }

    rateMode_ = mode;
    configureDelay();
}

void GbaReverb::setDelayDepth(DelayDepth depth)
{
    if (depth != DelayDepth::Int16 && depth != DelayDepth::Int32) {
        depth = DelayDepth::Int8;
    }
    if (depth == delayDepth_) {
        return;
    }

    delayDepth_ = depth;
    reset();
}

void GbaReverb::process(const double *inL, const double *inR,
                        double *outL, double *outR, long frames) noexcept
{
    if (!inL || !inR || !outL || !outR || frames <= 0) {
        return;
    }

    if (amount_ == 0) {
        for (long i = 0; i < frames; ++i) {
            outL[i] = inL[i];
            outR[i] = inR[i];
        }
        return;
    }

    const int32_t amount = static_cast<int32_t>(amount_);
    const long frameSize = frameSize_;
    const long bufferSize = bufferSize_;
    const DelayDepth depth = delayDepth_;

    for (long i = 0; i < frames; ++i) {
        const long otherPos = (pos_ + frameSize) % bufferSize;
        const int64_t sum = static_cast<int64_t>(bufL_[pos_])
                          + static_cast<int64_t>(bufR_[pos_])
                          + static_cast<int64_t>(bufL_[otherPos])
                          + static_cast<int64_t>(bufR_[otherPos]);
        const int64_t wet = arithmeticShiftRight9(sum * amount);

        const int64_t mixedL = sampleToDelay(inL[i]) + wet;
        const int64_t mixedR = sampleToDelay(inR[i]) + wet;
        const int64_t clampedL = clampForDepth(mixedL, depth);
        const int64_t clampedR = clampForDepth(mixedR, depth);

        outL[i] = delayToSample(clampedL);
        outR[i] = delayToSample(clampedR);
        bufL_[pos_] = static_cast<int32_t>(clampedL);
        bufR_[pos_] = static_cast<int32_t>(clampedR);

        ++pos_;
        if (pos_ >= bufferSize) {
            pos_ = 0;
        }
    }
}

int64_t GbaReverb::arithmeticShiftRight9(int64_t value) noexcept
{
    if (value >= 0) {
        return value >> 9;
    }
    return -(((-value) + 511) >> 9);
}

int64_t GbaReverb::clampForDepth(int64_t value, DelayDepth depth) noexcept
{
    switch (depth) {
        case DelayDepth::Int16:
            return std::clamp<int64_t>(value, -32768, 32767);
        case DelayDepth::Int32:
            return std::clamp<int64_t>(value, -2147483648LL, 2147483647LL);
        case DelayDepth::Int8:
        default:
            return std::clamp<int64_t>(value, -128, 127);
    }
}

int64_t GbaReverb::scaleForDepth(DelayDepth depth) noexcept
{
    switch (depth) {
        case DelayDepth::Int16:
            return 256;
        case DelayDepth::Int32:
            return 16777216;
        case DelayDepth::Int8:
        default:
            return 1;
    }
}

int64_t GbaReverb::sampleToDelay(double sample) const noexcept
{
    if (!std::isfinite(sample)) {
        return 0;
    }
    if (sample <= -1.0) {
        return clampForDepth(-128 * scaleForDepth(delayDepth_), delayDepth_);
    }
    if (sample >= 1.0) {
        return clampForDepth(127 * scaleForDepth(delayDepth_), delayDepth_);
    }
    return clampForDepth(std::llround(sample * 127.0 * scaleForDepth(delayDepth_)),
                         delayDepth_);
}

double GbaReverb::delayToSample(int64_t sample) const noexcept
{
    return static_cast<double>(sample) / static_cast<double>(127 * scaleForDepth(delayDepth_));
}

double GbaReverb::modelRate() const noexcept
{
    return rateMode_ == ReverbRateMode::Host ? kHostRateHz : kOriginalRateHz;
}

void GbaReverb::configureDelay()
{
    const double scale = hostSampleRate_ / modelRate();
    frameSize_ = std::max(1L, std::lround(static_cast<double>(kBaseFrameSize) * scale));
    bufferSize_ = std::max(frameSize_ + 1,
                           std::lround(static_cast<double>(kBaseBufferSize) * scale));
    bufL_.assign(static_cast<size_t>(bufferSize_), 0);
    bufR_.assign(static_cast<size_t>(bufferSize_), 0);
    pos_ = 0;
}

} // namespace porya
