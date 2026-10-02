// DSP primitives: FFT against the O(N^2) reference, tone location/amplitude, FIR response, mixing, SNR.
#include <random>
#include <vector>

#include "check.hpp"
#include "instrument_config.hpp"
#include "nmr/dsp.hpp"

using namespace nmr::dsp;
static const double PI = 3.14159265358979323846;
static const double FS_RAW = nmr::cfg::ACQUISITION_RAW_RATE_HZ, FS_DEC = nmr::cfg::ACQUISITION_DECIMATED_RATE_HZ,
                    F_IF = nmr::cfg::RF_IF_FREQUENCY_HZ;

int main() {
    std::mt19937_64 rng(3);
    std::normal_distribution<double> g(0, 1);
    // FFT == DFT for random data; inverse round trip
    for (size_t n : {8u, 64u, 1024u}) {
        std::vector<cplx> x(n), a(n), b(n);
        for (auto& v : x) v = {g(rng), g(rng)};
        a = x;
        FftPlan(n).execute(a.data());
        dft_reference(x.data(), b.data(), n);
        double err = 0;
        for (size_t k = 0; k < n; ++k) err = std::max(err, std::abs(a[k] - b[k]));
        CHECK_NEAR(err, 0, 1e-10 * std::sqrt(static_cast<double>(n)), 0, "FFT vs DFT reference (max abs error)");
        FftPlan(n).execute(a.data(), true);
        double rt = 0;
        for (size_t k = 0; k < n; ++k) rt = std::max(rt, std::abs(a[k] / static_cast<double>(n) - x[k]));
        CHECK_NEAR(rt, 0, 1e-12, 0, "FFT inverse round trip");
    }
    // a complex tone: peak at its frequency (parabolic interpolation), |S| = amplitude with the rect window on-bin
    {
        const double fs = FS_DEC, f = 1234.5, A = 0.37;
        const size_t n = 4096;
        std::vector<cplx> x(n);
        for (size_t k = 0; k < n; ++k) x[k] = A * std::polar(1.0, 2 * PI * f * k / fs + 0.3);
        const Spectrum s = spectrum(x.data(), n, fs, Window::Hann, 0, 4);
        const Peak p = find_peak(s, -fs / 2, fs / 2, 50);
        CHECK_NEAR(p.freq_hz, f, 0.05 * fs / (4 * n), 0, "tone frequency (Hann, 4x zero-padding, parabolic)");
        CHECK_NEAR(p.amplitude, A, 0, 0.02, "tone amplitude |S| (Hann, scaled by sum(w))");
        // negative frequency maps to the negative half of the shifted axis
        for (size_t k = 0; k < n; ++k) x[k] = std::polar(1.0, -2 * PI * f * k / fs);
        const Peak q = find_peak(spectrum(x.data(), n, fs, Window::Rect, 0, 4), -fs / 2, fs / 2, 50);
        CHECK_NEAR(q.freq_hz, -f, 0.5, 0, "negative-frequency tone");
    }
    // FIR: unity DC gain, passband flat to 0.1 dB at 0.25 fs_out, stopband below -60 dB beyond fs_out/2 + transition
    {
        const double fs = FS_RAW;
        const FirDecimator fir(0.4 * FS_DEC, fs, 63, static_cast<size_t>(nmr::cfg::ACQUISITION_DECIMATION));
        double sum = 0;
        for (double h : fir.taps()) sum += h;
        CHECK_NEAR(sum, 1.0, 1e-12, 0, "FIR DC gain");
        const auto resp = [&](double f) {
            cplx r = 0;
            for (size_t k = 0; k < fir.taps().size(); ++k) r += fir.taps()[k] * std::polar(1.0, -2 * PI * f * k / fs);
            return std::abs(r);
        };
        CHECK_NEAR(20 * std::log10(resp(F_IF)), 0, 0.1, 0, "FIR passband at the IF (dB)");
        CHECK(20 * std::log10(resp(1.2 * FS_DEC)) < -60, "FIR stopband at 1.2 x the output rate below -60 dB");
    }
    // mix_down moves a tone at f_if (absolute time origin t0) to DC with the right phase
    {
        const double fs = FS_RAW, fif = F_IF, t0 = 0.0123, ph = 0.7;
        const size_t n = 1000;
        std::vector<double> i(n), q(n);
        for (size_t k = 0; k < n; ++k) {
            const double t = t0 + k / fs;
            i[k] = std::cos(2 * PI * fif * t + ph) + 0.1;
            q[k] = std::sin(2 * PI * fif * t + ph) - 0.2;
        }
        std::vector<cplx> out(n);
        mix_down(i.data(), q.data(), n, {0.1, -0.2}, fif, fs, t0, out.data());
        CHECK_NEAR(std::arg(out[0]), ph, 1e-12, 0, "mix_down phase");
        CHECK_NEAR(std::arg(out[n - 1]), ph, 1e-12, 0, "mix_down phase stays constant");
    }
    // SNR estimate of a tone in white noise: expected A * sqrt(n) / sigma for the rect window (per quadrature)
    {
        const double fs = FS_DEC, f = 500, A = 1.0, sigma = 4.0;
        const size_t n = 8192;
        std::vector<cplx> x(n);
        for (size_t k = 0; k < n; ++k) x[k] = A * std::polar(1.0, 2 * PI * f * k / fs) + cplx(sigma * g(rng), sigma * g(rng));
        const Peak p = find_peak(spectrum(x.data(), n, fs, Window::Rect, 0, 1), -fs / 2, fs / 2, 20);
        CHECK_NEAR(p.snr, A * std::sqrt(static_cast<double>(n)) / sigma, 0, 0.15, "SNR = A sqrt(N)/sigma (rect, on-bin)");
    }
    return finish("test_dsp");
}
