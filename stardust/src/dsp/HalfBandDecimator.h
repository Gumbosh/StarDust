// HalfBandDecimator.h — shared 15-tap polyphase half-band decimation filters
// Used by Saturation, DestroyDrive, and TapeEngine for oversampled processing.
#pragma once

namespace HalfBand
{

// History buffer size required by both kernels.
static constexpr int kHistorySize = 8;

// ---------------------------------------------------------------------------
// Kaiser-window kernel (~90 dB stopband rejection)
// Used by Saturation (8x) and DestroyDrive (4x).
// in0/in1 = two consecutive input samples (even/odd polyphase branches).
// h[] must point to at least kHistorySize floats, zeroed on init.
// Returns the decimated output sample.
// ---------------------------------------------------------------------------
inline float decimateKaiser(float in0, float in1, float* h) noexcept
{
    h[0] = h[1]; h[1] = h[2]; h[2] = h[3];
    h[3] = h[4]; h[4] = h[5]; h[5] = h[6];
    h[6] = h[7]; h[7] = in0;

    const float even = -0.001586f * (h[0] + h[7])
                     +  0.011750f * (h[1] + h[6])
                     + -0.052800f * (h[2] + h[5])
                     +  0.295636f * (h[3] + h[4]);
    return even + 0.5f * in1;
}

// ---------------------------------------------------------------------------
// Parks-McClellan kernel (~60 dB stopband, narrower transition band)
// Used by TapeEngine (8x multi-band decimation).
// Same buffer / calling convention as decimateKaiser.
// ---------------------------------------------------------------------------
inline float decimatePM(float in0, float in1, float* h) noexcept
{
    h[0] = h[1]; h[1] = h[2]; h[2] = h[3];
    h[3] = h[4]; h[4] = h[5]; h[5] = h[6];
    h[6] = h[7]; h[7] = in0;

    const float even =  0.000457f * (h[0] + h[7])
                     + -0.007324f * (h[1] + h[6])
                     + -0.054264f * (h[2] + h[5])
                     +  0.302631f * (h[3] + h[4]);
    return even + 0.5f * in1;
}

} // namespace HalfBand
