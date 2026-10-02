#include "nmr/dsp.hpp"

#include <algorithm>
#include <cmath>

namespace nmr::dsp {

static constexpr double kPi = 3.14159265358979323846;

size_t next_pow2(size_t n) {
    size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

FftPlan::FftPlan(size_t n) : n_(n), tw_(n / 2), rev_(n) {
    for (size_t k = 0; k < n / 2; ++k) tw_[k] = std::polar(1.0, -2.0 * kPi * static_cast<double>(k) / static_cast<double>(n));
    size_t bits = 0;
    while ((size_t{1} << bits) < n) ++bits;
    for (size_t i = 0; i < n; ++i) {
        size_t r = 0;
        for (size_t b = 0; b < bits; ++b)
            if (i & (size_t{1} << b)) r |= size_t{1} << (bits - 1 - b);
        rev_[i] = r;
    }
}

void FftPlan::execute(cplx* a, bool inverse) const {
    for (size_t i = 0; i < n_; ++i)
        if (i < rev_[i]) std::swap(a[i], a[rev_[i]]);
    for (size_t len = 2; len <= n_; len <<= 1) {
        const size_t half = len / 2, step = n_ / len;
        for (size_t i = 0; i < n_; i += len)
            for (size_t j = 0; j < half; ++j) {
                cplx w = tw_[j * step];
                if (inverse) w = std::conj(w);
                const cplx u = a[i + j], v = a[i + j + half] * w;
                a[i + j] = u + v;
                a[i + j + half] = u - v;
            }
    }
}

void dft_reference(const cplx* in, cplx* out, size_t n) {
    for (size_t k = 0; k < n; ++k) {
        cplx s = 0;
        for (size_t j = 0; j < n; ++j)
            s += in[j] * std::polar(1.0, -2.0 * kPi * static_cast<double>((k * j) % n) / static_cast<double>(n));
        out[k] = s;
    }
}

FirDecimator::FirDecimator(double fc, double fs, size_t taps, size_t decim) : h_(taps), decim_(decim) {
    const double m = static_cast<double>(taps) - 1.0, w = 2.0 * fc / fs;
    double sum = 0;
    for (size_t k = 0; k < taps; ++k) {
        const double x = static_cast<double>(k) - m / 2.0;
        const double sinc = x == 0 ? w : std::sin(kPi * w * x) / (kPi * x);
        const double bl = 0.42 - 0.5 * std::cos(2 * kPi * k / m) + 0.08 * std::cos(4 * kPi * k / m);
        h_[k] = sinc * bl;
        sum += h_[k];
    }
    for (double& v : h_) v /= sum;   // unity DC gain
}

size_t FirDecimator::process(const cplx* x, size_t n, cplx* out, size_t cap) const {
    const size_t t = h_.size();
    if (n < t) return 0;
    size_t m = 0;
    for (size_t s = 0; s + t <= n && m < cap; s += decim_, ++m) {
        cplx acc = 0;
        for (size_t k = 0; k < t; ++k) acc += h_[k] * x[s + k];
        out[m] = acc;
    }
    return m;
}

void mix_down(const double* i, const double* q, size_t n, cplx off, double f_if, double fs, double t0, cplx* out) {
    for (size_t k = 0; k < n; ++k) {
        const double ph = 2.0 * kPi * f_if * (t0 + static_cast<double>(k) / fs);
        out[k] = (cplx(i[k], q[k]) - off) * std::polar(1.0, -ph);
    }
}

void frac_delay(const double* x, size_t n, double delay, size_t taps, double* out) {
    if (delay == 0.0) {   // exact copy: the common no-skew path must not perturb the data
        for (size_t k = 0; k < n; ++k) out[k] = x[k];
        return;
    }
    if (taps < 3) taps = 3;
    if (taps % 2 == 0) ++taps;              // odd: one centre tap
    const long c = static_cast<long>(taps / 2);
    const double m = static_cast<double>(taps) - 1.0;
    std::vector<double> h(taps);
    double sum = 0;
    for (size_t j = 0; j < taps; ++j) {
        const double arg = (static_cast<double>(j) - static_cast<double>(c)) - delay;   // sinc centred, shifted by delay
        const double sinc = std::fabs(arg) < 1e-12 ? 1.0 : std::sin(kPi * arg) / (kPi * arg);
        const double bl = 0.42 - 0.5 * std::cos(2 * kPi * j / m) + 0.08 * std::cos(4 * kPi * j / m);
        h[j] = sinc * bl;
        sum += h[j];
    }
    for (double& v : h) v /= sum;           // unity DC gain
    for (size_t k = 0; k < n; ++k) {
        double acc = 0;
        for (size_t j = 0; j < taps; ++j) {
            const long xi = static_cast<long>(k) - (static_cast<long>(j) - c);   // out[k] = sum_j h[j] x[k-(j-c)]
            if (xi >= 0 && xi < static_cast<long>(n)) acc += h[j] * x[xi];
        }
        out[k] = acc;
    }
}

cplx mean(const cplx* x, size_t n) {
    cplx s = 0;
    for (size_t k = 0; k < n; ++k) s += x[k];
    return n ? s / static_cast<double>(n) : s;
}

void rotate(cplx* x, size_t n, double rad) {
    const cplx r = std::polar(1.0, -rad);
    for (size_t k = 0; k < n; ++k) x[k] *= r;
}

void accumulate(cplx* acc, const cplx* x, size_t n) {
    for (size_t k = 0; k < n; ++k) acc[k] += x[k];
}

static double wval(Window w, size_t k, size_t n, double fs, double tc) {
    const double nn = static_cast<double>(n), kk = static_cast<double>(k);
    switch (w) {
        case Window::Rect: return 1.0;
        case Window::Hann: return 0.5 - 0.5 * std::cos(2 * kPi * kk / (nn - 1));
        case Window::Exponential: return std::exp(-kk / fs / tc);
        case Window::HalfHann: return 0.5 + 0.5 * std::cos(kPi * kk / nn);
    }
    return 1.0;
}

void apply_window(cplx* x, size_t n, Window w, double fs, double tc) {
    for (size_t k = 0; k < n; ++k) x[k] *= wval(w, k, n, fs, tc);
}

Spectrum spectrum(const cplx* x, size_t n, double fs, Window w, double tc, int pad) {
    const size_t nf = next_pow2(n * static_cast<size_t>(std::max(1, pad)));
    std::vector<cplx> buf(nf, cplx(0));
    double wsum = 0;
    for (size_t k = 0; k < n; ++k) {
        const double v = wval(w, k, n, fs, tc);
        buf[k] = x[k] * v;
        wsum += v;
    }
    FftPlan(nf).execute(buf.data());
    Spectrum s;
    s.freq_hz.resize(nf);
    s.value.resize(nf);
    for (size_t k = 0; k < nf; ++k) {
        const size_t src = (k + nf / 2) % nf;
        const double kk = static_cast<double>(k) - static_cast<double>(nf / 2);
        s.freq_hz[k] = kk * fs / static_cast<double>(nf);
        s.value[k] = buf[src] / wsum;
    }
    return s;
}

Peak find_peak(const Spectrum& s, double f_min, double f_max, double guard) {
    Peak p;
    size_t best = 0;
    double bv = -1;
    for (size_t k = 0; k < s.freq_hz.size(); ++k)
        if (s.freq_hz[k] >= f_min && s.freq_hz[k] <= f_max && std::abs(s.value[k]) > bv) { bv = std::abs(s.value[k]); best = k; }
    if (bv < 0) return p;
    p.amplitude = bv;
    p.freq_hz = s.freq_hz[best];
    if (best > 0 && best + 1 < s.value.size()) {
        const double a = std::abs(s.value[best - 1]), b = bv, c = std::abs(s.value[best + 1]);
        const double den = a - 2 * b + c;
        if (den != 0) p.freq_hz += 0.5 * (a - c) / den * (s.freq_hz[1] - s.freq_hz[0]);
    }
    double acc = 0;
    size_t cnt = 0;
    for (size_t k = 0; k < s.freq_hz.size(); ++k)
        if (s.freq_hz[k] >= f_min && s.freq_hz[k] <= f_max && std::fabs(s.freq_hz[k] - p.freq_hz) > guard) {
            acc += std::norm(s.value[k]);
            ++cnt;
        }
    p.noise_rms = cnt ? std::sqrt(acc / cnt / 2.0) : 0;
    p.snr = p.noise_rms > 0 ? p.amplitude / p.noise_rms : 0;
    return p;
}

}  // namespace nmr::dsp
