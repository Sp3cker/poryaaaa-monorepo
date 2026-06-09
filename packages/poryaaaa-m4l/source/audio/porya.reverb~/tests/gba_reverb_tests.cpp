#include "gba_reverb.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using porya::GbaReverb;
using porya::DelayDepth;
using porya::ReverbRateMode;

static void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

static bool near(double a, double b)
{
    return std::abs(a - b) < 0.000001;
}

static void test_amount_zero_copies_and_does_not_seed_delay()
{
    GbaReverb reverb;
    reverb.setHostSampleRate(GbaReverb::kOriginalRateHz);
    reverb.setAmount(0);

    const double inL[] = {1.0, -0.5, 0.25};
    const double inR[] = {-1.0, 0.5, -0.25};
    double outL[] = {0.0, 0.0, 0.0};
    double outR[] = {0.0, 0.0, 0.0};
    reverb.process(inL, inR, outL, outR, 3);

    require(near(outL[0], 1.0) && near(outL[1], -0.5) && near(outL[2], 0.25),
            "amount 0 copies left input");
    require(near(outR[0], -1.0) && near(outR[1], 0.5) && near(outR[2], -0.25),
            "amount 0 copies right input");

    reverb.setAmount(127);
    const double zero[1] = {0.0};
    double wetL[1] = {1.0};
    double wetR[1] = {1.0};
    reverb.process(zero, zero, wetL, wetR, 1);
    require(near(wetL[0], 0.0) && near(wetR[0], 0.0),
            "amount 0 does not write hidden dry history into delay");
}

static void test_delay_feedback_survives_vector_boundaries()
{
    GbaReverb reverb;
    reverb.setHostSampleRate(GbaReverb::kOriginalRateHz);
    reverb.setAmount(127);

    std::vector<double> left(1000, 0.0);
    std::vector<double> right(1000, 0.0);
    std::vector<double> outL(1000, 0.0);
    std::vector<double> outR(1000, 0.0);
    left[0] = 1.0;
    reverb.process(left.data(), right.data(), outL.data(), outR.data(), 1000);
    require(near(outL[0], 1.0), "dry impulse passes through first vector");

    std::vector<double> zeros(360, 0.0);
    std::vector<double> wetL(360, 0.0);
    std::vector<double> wetR(360, 0.0);
    reverb.process(zeros.data(), zeros.data(), wetL.data(), wetR.data(), 360);
    require(near(wetL[359], 0.0) && near(wetR[359], 0.0),
            "wet tap has not arrived one sample early");

    double oneZero[1] = {0.0};
    double tapL[1] = {0.0};
    double tapR[1] = {0.0};
    reverb.process(oneZero, oneZero, tapL, tapR, 1);
    const double expected = 31.0 / 127.0;
    require(near(tapL[0], expected) && near(tapR[0], expected),
            "other reverb tap arrives across perform vector boundary");
}

static void test_rate_modes_scale_delay()
{
    GbaReverb reverb;
    reverb.setHostSampleRate(44100.0);
    reverb.setRateMode(ReverbRateMode::Host);
    require(reverb.frameSize() == GbaReverb::kBaseFrameSize,
            "Host mode at 44100 keeps base frame size");
    require(reverb.bufferSize() == GbaReverb::kBaseBufferSize,
            "Host mode at 44100 keeps base buffer size");

    reverb.setRateMode(ReverbRateMode::Original);
    require(reverb.frameSize() > GbaReverb::kBaseFrameSize,
            "Original mode scales frame size up at 44100 host rate");
    require(reverb.bufferSize() > GbaReverb::kBaseBufferSize,
            "Original mode scales buffer size up at 44100 host rate");
}

static void test_delay_depth_defaults_and_clamps()
{
    GbaReverb reverb;
    require(reverb.delayDepth() == DelayDepth::Int8, "delay depth defaults to Int8");
    reverb.setDelayDepth(DelayDepth::Int16);
    require(reverb.delayDepth() == DelayDepth::Int16, "delay depth sets Int16");
    reverb.setDelayDepth(DelayDepth::Int32);
    require(reverb.delayDepth() == DelayDepth::Int32, "delay depth sets Int32");
    reverb.setDelayDepth(static_cast<DelayDepth>(99));
    require(reverb.delayDepth() == DelayDepth::Int8, "invalid delay depth returns to Int8");
}

static void test_wider_depth_keeps_delay_but_changes_quantization()
{
    GbaReverb int8Reverb;
    GbaReverb int16Reverb;
    int8Reverb.setHostSampleRate(GbaReverb::kOriginalRateHz);
    int16Reverb.setHostSampleRate(GbaReverb::kOriginalRateHz);
    int8Reverb.setAmount(127);
    int16Reverb.setAmount(127);
    int16Reverb.setDelayDepth(DelayDepth::Int16);

    std::vector<double> left(1000, 0.0);
    std::vector<double> right(1000, 0.0);
    std::vector<double> outL(1000, 0.0);
    std::vector<double> outR(1000, 0.0);
    left[0] = 1.0;
    int8Reverb.process(left.data(), right.data(), outL.data(), outR.data(), 1000);
    int16Reverb.process(left.data(), right.data(), outL.data(), outR.data(), 1000);

    std::vector<double> zeros(361, 0.0);
    std::vector<double> wet8L(361, 0.0);
    std::vector<double> wet8R(361, 0.0);
    std::vector<double> wet16L(361, 0.0);
    std::vector<double> wet16R(361, 0.0);
    int8Reverb.process(zeros.data(), zeros.data(), wet8L.data(), wet8R.data(), 361);
    int16Reverb.process(zeros.data(), zeros.data(), wet16L.data(), wet16R.data(), 361);

    require(near(wet8L[360], 31.0 / 127.0), "Int8 keeps original delayed tap");
    require(wet16L[360] > wet8L[360], "Int16 keeps fractional wet value");
    require(wet16L[360] < 0.25, "Int16 delayed tap remains level-matched");
}

static void test_amount_clamps()
{
    GbaReverb reverb;
    reverb.setAmount(-10);
    require(reverb.amount() == 0, "amount clamps low");
    reverb.setAmount(200);
    require(reverb.amount() == 127, "amount clamps high");
}

int main()
{
    test_amount_zero_copies_and_does_not_seed_delay();
    test_delay_feedback_survives_vector_boundaries();
    test_rate_modes_scale_delay();
    test_delay_depth_defaults_and_clamps();
    test_wider_depth_keeps_delay_but_changes_quantization();
    test_amount_clamps();
    std::cout << "gba_reverb_tests: all tests passed\n";
    return 0;
}
