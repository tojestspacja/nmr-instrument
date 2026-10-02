// Signal processing shared by the firmware, the simulator, the host tools and the browser (via WASM).
// The real-time-safe parts (FftPlan::execute, FirDecimator::process, the in-place helpers) never allocate:
// all buffers are sized when the plan/filter is constructed, before an experiment starts.
#pragma once

#include <complex>
#include <cstddef>
#include <vector>

namespace nmr::dsp {

using cplx = std::complex<double>;

size_t next_pow2(size_t n);

// Radix-2 iterative FFT with a precomputed twiddle table. Forward: X[k] = sum x[n] e^{-i 2 pi k n / N}.
class FftPlan {
public:
    explicit FftPlan(size_t n);     // n must be a power of two
    size_t size() const { return n_; }
    void execute(cplx* data, bool inverse = false) const;   // in place; inverse is unnormalised
private:
    size_t n_;
    std::vector<cplx> tw_;
    std::vector<size_t> rev_;
};

// O(N^2) reference used only by tests.
void dft_reference(const cplx* in, cplx* out, size_t n);

// Windowed-sinc (Blackman) low-pass FIR, decimating by `decim`. Group delay = (taps-1)/2 input samples.
class FirDecimator {
public:
    FirDecimator(double cutoff_hz, double fs_hz, size_t taps, size_t decim);
    size_t decim() const { return decim_; }
    double group_delay_samples() const { return 0.5 * (static_cast<double>(h_.size()) - 1.0); }
    const std::vector<double>& taps() const { return h_; }
    // y[m] = sum_k h[k] x[m*decim + k], m = 0 .. (n - taps)/decim. Returns outputs written (<= out_cap).
    size_t process(const cplx* x, size_t n, cplx* out, size_t out_cap) const;
private:
    std::vector<double> h_;
    size_t decim_;
};

// Complex IF -> baseband: out[k] = (I[k] + j Q[k] - offset) e^{-i (2 pi f_if (t0 + k/fs))}.
void mix_down(const double* i, const double* q, size_t n, cplx offset, double f_if, double fs, double t0, cplx* out);

cplx mean(const cplx* x, size_t n);
void rotate(cplx* x, size_t n, double radians);        // x *= e^{-i radians}
void accumulate(cplx* acc, const cplx* x, size_t n);   // acc += x

enum class Window { Rect, Hann, Exponential, HalfHann };
// w[k] for a record of n samples at fs; Exponential uses time constant tc_s (matched filter for T2* = tc_s).
void apply_window(cplx* x, size_t n, Window w, double fs, double tc_s = 0);

struct Spectrum {
    std::vector<double> freq_hz;   // fftshifted axis, relative to the baseband zero
    std::vector<cplx> value;       // scaled by 1/sum(window) so a tone of amplitude A has |S| = A
};
// Zero-pads to >= pad_factor * n (power of two), windows, FFTs, shifts.
Spectrum spectrum(const cplx* x, size_t n, double fs, Window w, double tc_s = 0, int pad_factor = 2);

struct Peak {
    double freq_hz = 0;     // parabolic interpolation on |S|
    double amplitude = 0;   // |S| at the peak bin
    double noise_rms = 0;   // per-quadrature rms of S outside +-guard_hz of the peak
    double snr = 0;         // amplitude / noise_rms
};
Peak find_peak(const Spectrum& s, double f_min, double f_max, double guard_hz);

}  // namespace nmr::dsp
