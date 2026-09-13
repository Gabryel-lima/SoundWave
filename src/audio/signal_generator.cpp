#include "soundwave/audio/signal_generator.hpp"

#include <cmath>
#include <numbers>

namespace soundwave::signals {
namespace {

constexpr double kTwoPi = 2.0 * std::numbers::pi;

std::size_t sampleCount(double durationSeconds, double sampleRate) {
    if (durationSeconds <= 0.0 || sampleRate <= 0.0) return 0;
    return static_cast<std::size_t>(durationSeconds * sampleRate);
}

PcmBuffer makeBuffer(double sampleRate, std::size_t count, const char* name) {
    PcmBuffer buffer;
    buffer.sampleRate = sampleRate;
    buffer.samples.assign(count, 0.0);
    buffer.sourceName = name;
    return buffer;
}

}  // namespace

PcmBuffer sine(double frequencyHz, double durationSeconds, double sampleRate, double amplitude,
               double phaseRad) {
    PcmBuffer buffer = makeBuffer(sampleRate, sampleCount(durationSeconds, sampleRate), "sine");
    const double step = kTwoPi * frequencyHz / sampleRate;
    for (std::size_t n = 0; n < buffer.samples.size(); ++n) {
        buffer.samples[n] = amplitude * std::sin(step * static_cast<double>(n) + phaseRad);
    }
    return buffer;
}

PcmBuffer chord(const std::vector<double>& frequenciesHz, double durationSeconds,
                double sampleRate, double amplitude) {
    PcmBuffer buffer = makeBuffer(sampleRate, sampleCount(durationSeconds, sampleRate), "chord");
    if (frequenciesHz.empty()) return buffer;

    const double perTone = amplitude / static_cast<double>(frequenciesHz.size());
    for (double f : frequenciesHz) {
        const double step = kTwoPi * f / sampleRate;
        for (std::size_t n = 0; n < buffer.samples.size(); ++n) {
            buffer.samples[n] += perTone * std::sin(step * static_cast<double>(n));
        }
    }
    return buffer;
}

PcmBuffer harmonicSeries(double fundamentalHz, std::size_t harmonics, double durationSeconds,
                         double sampleRate, double amplitude) {
    PcmBuffer buffer =
        makeBuffer(sampleRate, sampleCount(durationSeconds, sampleRate), "harmonic-series");
    if (harmonics == 0) return buffer;

    // Normaliza pela soma da serie 1/n para que a amplitude de pico fique
    // proxima de `amplitude` independentemente do numero de harmonicos.
    double norm = 0.0;
    for (std::size_t h = 1; h <= harmonics; ++h) norm += 1.0 / static_cast<double>(h);

    const double nyquist = sampleRate * 0.5;
    for (std::size_t h = 1; h <= harmonics; ++h) {
        const double f = fundamentalHz * static_cast<double>(h);
        if (f >= nyquist) break;  // acima de Nyquist o harmonico seria rebatido (aliasing)
        const double gain = amplitude / (static_cast<double>(h) * norm);
        const double step = kTwoPi * f / sampleRate;
        for (std::size_t n = 0; n < buffer.samples.size(); ++n) {
            buffer.samples[n] += gain * std::sin(step * static_cast<double>(n));
        }
    }
    return buffer;
}

PcmBuffer logSweep(double startHz, double endHz, double durationSeconds, double sampleRate,
                   double amplitude) {
    PcmBuffer buffer = makeBuffer(sampleRate, sampleCount(durationSeconds, sampleRate), "log-sweep");
    if (buffer.samples.empty() || startHz <= 0.0 || endHz <= 0.0) return buffer;

    // Para f(t) = f0 * (f1/f0)^(t/T), a fase e a integral de 2*pi*f(t):
    //   phi(t) = 2*pi*f0*T/ln(f1/f0) * ((f1/f0)^(t/T) - 1)
    // Integrar a frequencia (em vez de usar f(t) direto no argumento do seno) e
    // o que garante continuidade de fase; caso contrario a varredura tem saltos
    // que aparecem como raias espurias no espectrograma.
    const double ratio = endHz / startHz;
    const double lnRatio = std::log(ratio);
    const double T = durationSeconds;

    for (std::size_t n = 0; n < buffer.samples.size(); ++n) {
        const double t = static_cast<double>(n) / sampleRate;
        const double phase = std::abs(lnRatio) < 1e-12
                                 ? kTwoPi * startHz * t
                                 : kTwoPi * startHz * T / lnRatio * (std::pow(ratio, t / T) - 1.0);
        buffer.samples[n] = amplitude * std::sin(phase);
    }
    return buffer;
}

PcmBuffer whiteNoise(double durationSeconds, double sampleRate, double amplitude,
                     std::uint64_t seed) {
    PcmBuffer buffer = makeBuffer(sampleRate, sampleCount(durationSeconds, sampleRate), "white-noise");

    // splitmix64: gerador pequeno, rapido e com resultado identico em qualquer
    // plataforma -- ao contrario de std::mt19937 com std::uniform_real_distribution,
    // cuja saida nao e portavel entre implementacoes da biblioteca padrao.
    std::uint64_t state = seed;
    auto next = [&state]() {
        state += 0x9E3779B97F4A7C15ULL;
        std::uint64_t z = state;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    };

    for (double& sample : buffer.samples) {
        const double unit = static_cast<double>(next() >> 11) / 9007199254740992.0;  // [0,1)
        sample = amplitude * (2.0 * unit - 1.0);
    }
    return buffer;
}

PcmBuffer silence(double durationSeconds, double sampleRate) {
    return makeBuffer(sampleRate, sampleCount(durationSeconds, sampleRate), "silence");
}

}  // namespace soundwave::signals
