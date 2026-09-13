#include "soundwave/dsp/analyzer.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace soundwave {

SpectrumAnalyzer::SpectrumAnalyzer(AnalyzerSettings settings, double sampleRate)
    : settings_(settings), sampleRate_(sampleRate) {
    if (!Fft::isPowerOfTwo(settings_.fftSize)) {
        throw std::invalid_argument("SpectrumAnalyzer: fftSize deve ser potencia de dois");
    }
    if (settings_.hopSize == 0) settings_.hopSize = settings_.fftSize / 4;
    if (sampleRate_ <= 0.0) throw std::invalid_argument("SpectrumAnalyzer: sampleRate invalido");

    window_ = makeWindow(settings_.window, settings_.fftSize);
    windowSum_ = 0.0;
    for (double w : window_) windowSum_ += w;
    if (windowSum_ <= 0.0) windowSum_ = static_cast<double>(settings_.fftSize);

    fft_ = std::make_shared<const Fft>(settings_.fftSize);
}

double SpectrumAnalyzer::resolutionHz() const {
    return sampleRate_ / static_cast<double>(settings_.fftSize);
}

std::size_t SpectrumAnalyzer::frameCountFor(std::size_t sampleCount) const {
    if (sampleCount == 0) return 0;
    // Ceil: o ultimo bloco parcial ainda e analisado, com zero-padding.
    return (sampleCount + settings_.hopSize - 1) / settings_.hopSize;
}

Spectrum SpectrumAnalyzer::analyzeBlock(const std::vector<double>& samples,
                                        std::size_t offset) const {
    const std::size_t n = settings_.fftSize;

    std::vector<double> block(n, 0.0);
    const std::size_t available = offset < samples.size() ? samples.size() - offset : 0;
    const std::size_t count = std::min(n, available);
    for (std::size_t i = 0; i < count; ++i) block[i] = samples[offset + i];

    // 1) Remocao de continua, sobre o bloco (nao sobre o sinal inteiro): um
    //    offset que varia lentamente e removido em cada bloco separadamente.
    if (settings_.removeDc && count > 0) {
        double mean = 0.0;
        for (std::size_t i = 0; i < count; ++i) mean += block[i];
        mean /= static_cast<double>(count);
        for (std::size_t i = 0; i < count; ++i) block[i] -= mean;
    }

    // 2) Normalizacao opcional por pico, com piso de silencio.
    if (settings_.normalizeBlocks) {
        double peak = 0.0;
        for (double v : block) peak = std::max(peak, std::abs(v));
        if (peak < settings_.silenceThreshold) {
            std::fill(block.begin(), block.end(), 0.0);
        } else {
            const double scale = 1.0 / peak;
            for (double& v : block) v *= scale;
        }
    }

    // 3) Janelamento.
    for (std::size_t i = 0; i < n; ++i) block[i] *= window_[i];

    // 4) Transformada.
    const std::vector<std::complex<double>> transformed = fft_->forwardReal(block);

    Spectrum spectrum;
    spectrum.sampleRate = sampleRate_;
    spectrum.fftSize = n;
    spectrum.timeOffsetSeconds = static_cast<double>(offset) / sampleRate_;
    spectrum.bins.resize(transformed.size());

    const double df = resolutionHz();
    for (std::size_t k = 0; k < transformed.size(); ++k) {
        // Normalizacao de amplitude: uma senoide de amplitude A produz um pico
        // de A * sum(window) / 2, porque sua energia se divide entre as
        // frequencias +f e -f. Multiplicamos por 2 para juntar as duas metades.
        // DC e Nyquist nao tem par espelhado, entao nao levam o fator 2.
        const bool hasMirror = (k != 0) && (k != n / 2);
        const double scale = (hasMirror ? 2.0 : 1.0) / windowSum_;

        spectrum.bins[k].frequencyHz = static_cast<double>(k) * df;
        spectrum.bins[k].magnitude = std::abs(transformed[k]) * scale;
        spectrum.bins[k].phaseRad = std::arg(transformed[k]);
    }
    return spectrum;
}

std::vector<Spectrum> SpectrumAnalyzer::analyzeAll(const PcmBuffer& buffer) const {
    std::vector<Spectrum> frames;
    if (buffer.empty()) return frames;

    const std::size_t total = frameCountFor(buffer.samples.size());
    frames.reserve(total);
    for (std::size_t frame = 0; frame < total; ++frame) {
        frames.push_back(analyzeBlock(buffer.samples, frame * settings_.hopSize));
    }
    return frames;
}

}  // namespace soundwave
