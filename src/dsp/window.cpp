#include "soundwave/dsp/window.hpp"

#include <cmath>
#include <numbers>

namespace soundwave {
namespace {

constexpr double kTwoPi = 2.0 * std::numbers::pi;

// Janelas cosseno generalizadas: w[n] = sum_j (-1)^j a_j cos(2*pi*j*n/N).
double cosineSum(const double* a, std::size_t terms, std::size_t n, std::size_t size) {
    const double x = kTwoPi * static_cast<double>(n) / static_cast<double>(size);
    double value = 0.0;
    for (std::size_t j = 0; j < terms; ++j) {
        const double term = a[j] * std::cos(static_cast<double>(j) * x);
        value += (j % 2 == 0) ? term : -term;
    }
    return value;
}

}  // namespace

std::string_view windowTypeName(WindowType type) {
    switch (type) {
        case WindowType::Rectangular:    return "rectangular";
        case WindowType::Hann:           return "hann";
        case WindowType::Hamming:        return "hamming";
        case WindowType::Blackman:       return "blackman";
        case WindowType::BlackmanHarris: return "blackman-harris";
    }
    return "unknown";
}

bool parseWindowType(std::string_view name, WindowType& out) {
    if (name == "rectangular" || name == "rect" || name == "none") {
        out = WindowType::Rectangular;
    } else if (name == "hann" || name == "hanning") {
        out = WindowType::Hann;
    } else if (name == "hamming") {
        out = WindowType::Hamming;
    } else if (name == "blackman") {
        out = WindowType::Blackman;
    } else if (name == "blackman-harris" || name == "blackmanharris" || name == "bh4") {
        out = WindowType::BlackmanHarris;
    } else {
        return false;
    }
    return true;
}

std::vector<double> makeWindow(WindowType type, std::size_t size) {
    std::vector<double> w(size, 1.0);
    if (size == 0 || type == WindowType::Rectangular) return w;

    static constexpr double kHann[]     = {0.5, 0.5};
    static constexpr double kHamming[]  = {0.54, 0.46};
    static constexpr double kBlackman[] = {0.42, 0.5, 0.08};
    static constexpr double kBH4[]      = {0.35875, 0.48829, 0.14128, 0.01168};

    const double* a = nullptr;
    std::size_t terms = 0;
    switch (type) {
        case WindowType::Hann:           a = kHann;     terms = 2; break;
        case WindowType::Hamming:        a = kHamming;  terms = 2; break;
        case WindowType::Blackman:       a = kBlackman; terms = 3; break;
        case WindowType::BlackmanHarris: a = kBH4;      terms = 4; break;
        case WindowType::Rectangular:    return w;
    }

    for (std::size_t n = 0; n < size; ++n) w[n] = cosineSum(a, terms, n, size);
    return w;
}

double coherentGain(const std::vector<double>& window) {
    if (window.empty()) return 0.0;
    double sum = 0.0;
    for (double v : window) sum += v;
    return sum / static_cast<double>(window.size());
}

double equivalentNoiseBandwidth(const std::vector<double>& window) {
    if (window.empty()) return 0.0;
    double sum = 0.0;
    double sumSquares = 0.0;
    for (double v : window) {
        sum += v;
        sumSquares += v * v;
    }
    if (sum == 0.0) return 0.0;
    return static_cast<double>(window.size()) * sumSquares / (sum * sum);
}

}  // namespace soundwave
