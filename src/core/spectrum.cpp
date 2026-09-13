#include "soundwave/core/spectrum.hpp"

#include <algorithm>
#include <cmath>

#include "soundwave/core/constants.hpp"

namespace soundwave {
namespace {

// Indices [first, last] cujos centros de bin caem em [minHz, maxHz].
struct BinRange {
    std::size_t first = 1;
    std::size_t last = 0;
    [[nodiscard]] bool valid() const { return last >= first; }
};

BinRange clampRange(const Spectrum& spectrum, double minHz, double maxHz) {
    BinRange range;
    if (spectrum.bins.empty() || maxHz <= minHz) return range;

    const double df = spectrum.resolutionHz();
    if (df <= 0.0) return range;

    // Ignoramos DC (k=0) por padrao: offset de continua nao e uma "frequencia"
    // audivel e domina o espectro de gravacoes mal acopladas.
    const auto lo = static_cast<long long>(std::ceil(minHz / df));
    const auto hi = static_cast<long long>(std::floor(maxHz / df));
    const auto top = static_cast<long long>(spectrum.bins.size()) - 1;

    const long long first = std::max<long long>(1, lo);
    const long long last = std::min<long long>(top, hi);
    if (last < first) return range;

    range.first = static_cast<std::size_t>(first);
    range.last = static_cast<std::size_t>(last);
    return range;
}

}  // namespace

double amplitudeToDecibels(double amplitude) {
    return 20.0 * std::log10(std::abs(amplitude) + kAmplitudeEpsilon);
}

std::optional<SpectralPeak> dominantBin(const Spectrum& spectrum, double minHz, double maxHz) {
    const BinRange range = clampRange(spectrum, minHz, maxHz);
    if (!range.valid()) return std::nullopt;

    std::size_t best = range.first;
    for (std::size_t k = range.first; k <= range.last; ++k) {
        if (spectrum.bins[k].magnitude > spectrum.bins[best].magnitude) best = k;
    }

    const SpectralBin& bin = spectrum.bins[best];
    return SpectralPeak{best, bin.frequencyHz, bin.magnitude, bin.phaseRad};
}

std::optional<SpectralPeak> dominantPeak(const Spectrum& spectrum, double minHz, double maxHz) {
    auto coarse = dominantBin(spectrum, minHz, maxHz);
    if (!coarse) return std::nullopt;

    const std::size_t k = coarse->binIndex;
    if (k == 0 || k + 1 >= spectrum.bins.size()) return coarse;

    // Interpolacao parabolica sobre log-magnitude. Em escala logaritmica o lobo
    // principal de uma janela Hann e quase exatamente uma parabola, o que torna
    // esta estimativa muito mais precisa que o centro do bin.
    const double a = amplitudeToDecibels(spectrum.bins[k - 1].magnitude);
    const double b = amplitudeToDecibels(spectrum.bins[k].magnitude);
    const double c = amplitudeToDecibels(spectrum.bins[k + 1].magnitude);

    const double denom = a - 2.0 * b + c;
    if (std::abs(denom) < 1e-12) return coarse;  // platô: sem curvatura utilizavel

    double delta = 0.5 * (a - c) / denom;
    // Um maximo verdadeiro fica a menos de meio bin do maior bin. Fora disso o
    // ajuste e ruido (ou a parabola abriu para cima) e o descartamos.
    if (!std::isfinite(delta) || std::abs(delta) > 0.5) return coarse;

    SpectralPeak peak = *coarse;
    peak.frequencyHz = (static_cast<double>(k) + delta) * spectrum.resolutionHz();
    // Vertice da parabola em dB, convertido de volta para amplitude linear.
    const double peakDb = b - 0.25 * (a - c) * delta;
    peak.magnitude = std::pow(10.0, peakDb / 20.0);
    return peak;
}

std::vector<SpectralPeak> findPeaks(const Spectrum& spectrum, double minHz, double maxHz,
                                    std::size_t maxPeaks, double floorRatio) {
    std::vector<SpectralPeak> peaks;
    const BinRange range = clampRange(spectrum, minHz, maxHz);
    if (!range.valid() || maxPeaks == 0) return peaks;

    double maxMagnitude = 0.0;
    for (std::size_t k = range.first; k <= range.last; ++k) {
        maxMagnitude = std::max(maxMagnitude, spectrum.bins[k].magnitude);
    }
    if (maxMagnitude <= 0.0) return peaks;

    const double threshold = maxMagnitude * floorRatio;
    const std::size_t lo = std::max<std::size_t>(range.first, 1);
    const std::size_t hi = std::min(range.last, spectrum.bins.size() - 2);

    for (std::size_t k = lo; k <= hi; ++k) {
        const double m = spectrum.bins[k].magnitude;
        if (m < threshold) continue;
        // Maximo local estrito a esquerda evita duplicar picos em platôs.
        if (m <= spectrum.bins[k - 1].magnitude) continue;
        if (m < spectrum.bins[k + 1].magnitude) continue;

        const double df = spectrum.resolutionHz();
        auto refined = dominantPeak(spectrum, (static_cast<double>(k) - 0.5) * df,
                                    (static_cast<double>(k) + 0.5) * df);
        peaks.push_back(refined ? *refined
                                : SpectralPeak{k, spectrum.bins[k].frequencyHz, m,
                                               spectrum.bins[k].phaseRad});
    }

    std::sort(peaks.begin(), peaks.end(),
              [](const SpectralPeak& a, const SpectralPeak& b) {
                  if (a.magnitude != b.magnitude) return a.magnitude > b.magnitude;
                  return a.frequencyHz < b.frequencyHz;  // desempate determinista
              });
    if (peaks.size() > maxPeaks) peaks.resize(maxPeaks);
    return peaks;
}

double bandEnergy(const Spectrum& spectrum, double lowHz, double highHz) {
    const BinRange range = clampRange(spectrum, lowHz, highHz);
    if (!range.valid()) return 0.0;

    double energy = 0.0;
    for (std::size_t k = range.first; k <= range.last; ++k) {
        const double m = spectrum.bins[k].magnitude;
        energy += m * m;
    }
    return energy;
}

}  // namespace soundwave
