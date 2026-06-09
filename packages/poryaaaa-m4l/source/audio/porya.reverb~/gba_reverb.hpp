#pragma once

#include <cstdint>
#include <vector>

namespace porya {

enum class ReverbRateMode : long {
    Original = 0,
    Host = 1,
};

enum class DelayDepth : long {
    Int8 = 0,
    Int16 = 1,
    Int32 = 2,
};

class GbaReverb {
public:
    static constexpr double kOriginalRateHz = 13379.0;
    static constexpr double kHostRateHz = 44100.0;
    static constexpr long kBaseFrameSize = 224;
    static constexpr long kBaseBufferSize = 1584;

    GbaReverb();

    void reset();
    void setAmount(long amount) noexcept;
    void setHostSampleRate(double sampleRate);
    void setRateMode(ReverbRateMode mode);
    void setDelayDepth(DelayDepth depth);
    void process(const double *inL, const double *inR,
                 double *outL, double *outR, long frames) noexcept;

    [[nodiscard]] long amount() const noexcept { return amount_; }
    [[nodiscard]] ReverbRateMode rateMode() const noexcept { return rateMode_; }
    [[nodiscard]] DelayDepth delayDepth() const noexcept { return delayDepth_; }
    [[nodiscard]] long frameSize() const noexcept { return frameSize_; }
    [[nodiscard]] long bufferSize() const noexcept { return bufferSize_; }

private:
    static int64_t arithmeticShiftRight9(int64_t value) noexcept;
    static int64_t clampForDepth(int64_t value, DelayDepth depth) noexcept;
    static int64_t scaleForDepth(DelayDepth depth) noexcept;

    [[nodiscard]] int64_t sampleToDelay(double sample) const noexcept;
    [[nodiscard]] double delayToSample(int64_t sample) const noexcept;

    [[nodiscard]] double modelRate() const noexcept;
    void configureDelay();

    std::vector<int32_t> bufL_;
    std::vector<int32_t> bufR_;
    long pos_ = 0;
    long frameSize_ = kBaseFrameSize;
    long bufferSize_ = kBaseBufferSize;
    long amount_ = 0;
    double hostSampleRate_ = kHostRateHz;
    ReverbRateMode rateMode_ = ReverbRateMode::Original;
    DelayDepth delayDepth_ = DelayDepth::Int8;
};

} // namespace porya
