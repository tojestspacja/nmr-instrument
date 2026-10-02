// Strongly typed SI quantities for public APIs. Internally everything is double in SI units; at the API
// boundary a frequency cannot be passed where a time is expected. Construct explicitly: Hz{89.4e3}, Seconds{1e-3}.
#pragma once

namespace nmr {

template <class Tag>
struct Quantity {
    double v = 0.0;
    constexpr Quantity() = default;
    constexpr explicit Quantity(double x) : v(x) {}
    constexpr double value() const { return v; }
    constexpr Quantity operator+(Quantity o) const { return Quantity{v + o.v}; }
    constexpr Quantity operator-(Quantity o) const { return Quantity{v - o.v}; }
    constexpr Quantity operator*(double k) const { return Quantity{v * k}; }
    constexpr Quantity operator/(double k) const { return Quantity{v / k}; }
    constexpr double operator/(Quantity o) const { return v / o.v; }
    constexpr bool operator<(Quantity o) const { return v < o.v; }
    constexpr bool operator>(Quantity o) const { return v > o.v; }
};

struct HzTag {};
struct SecondsTag {};
struct TeslaTag {};
struct RadTag {};
struct AmpTag {};
struct VoltTag {};
struct MetreTag {};

using Hz = Quantity<HzTag>;
using Seconds = Quantity<SecondsTag>;
using Tesla = Quantity<TeslaTag>;
using Radians = Quantity<RadTag>;
using Amps = Quantity<AmpTag>;
using Volts = Quantity<VoltTag>;
using Metres = Quantity<MetreTag>;

inline constexpr double kPi = 3.14159265358979323846;

// the only physically meaningful products are spelled out
constexpr Radians phase(Hz f, Seconds t) { return Radians{2.0 * kPi * f.v * t.v}; }
constexpr Hz larmor(double gamma_bar_hz_per_t, Tesla b) { return Hz{gamma_bar_hz_per_t * b.v}; }

}  // namespace nmr
