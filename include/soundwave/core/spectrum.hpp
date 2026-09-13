#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace soundwave {

// Um bin da FFT. Nomes de campo fixados pelo plano (ROUND 3, secao 6).
struct SpectralBin {
    double frequencyHz = 0.0;  // centro do bin, k * sampleRate / fftSize
    double magnitude   = 0.0;  // amplitude linear, corrigida pelo ganho da janela
    double phaseRad    = 0.0;  // fase em radianos, em (-pi, pi]
};

// Resultado de uma unica analise FFT sobre um bloco de amostras.
struct Spectrum {
    double sampleRate = 0.0;
    std::size_t fftSize = 0;
    double timeOffsetSeconds = 0.0;  // inicio do bloco no sinal de origem
    std::vector<SpectralBin> bins;   // fftSize/2 + 1 bins (DC ate Nyquist)

    // Resolucao espectral: df = fs / N. Duas senoides separadas por menos que
    // isto nao sao resolviveis, independente de interpolacao.
    [[nodiscard]] double resolutionHz() const {
        return fftSize == 0 ? 0.0 : sampleRate / static_cast<double>(fftSize);
    }

    [[nodiscard]] double nyquistHz() const { return sampleRate * 0.5; }
    [[nodiscard]] bool empty() const { return bins.empty(); }
};

// Pico espectral com frequencia refinada por interpolacao parabolica.
struct SpectralPeak {
    std::size_t binIndex = 0;
    double frequencyHz = 0.0;  // refinada, pode cair entre centros de bin
    double magnitude   = 0.0;  // refinada
    double phaseRad    = 0.0;  // fase do bin central (nao interpolada)
};

// ---------------------------------------------------------------------------
// Consultas sobre um espectro.
// ---------------------------------------------------------------------------

// Bin de maior magnitude dentro de [minHz, maxHz], SEM interpolacao.
[[nodiscard]] std::optional<SpectralPeak> dominantBin(
    const Spectrum& spectrum, double minHz, double maxHz);

// Idem, com refinamento parabolico sobre o log-magnitude dos tres bins ao redor
// do maximo. Reduz o erro de frequencia de ~df/2 para uma fracao pequena de df
// quando a janela e Hann. Ver docs/theory.md, secao "Interpolacao de pico".
[[nodiscard]] std::optional<SpectralPeak> dominantPeak(
    const Spectrum& spectrum, double minHz, double maxHz);

// Ate maxPeaks picos locais com magnitude >= floorRatio * maiorMagnitude,
// ordenados por magnitude decrescente. Usado pelos modos Weighted/Spectral.
[[nodiscard]] std::vector<SpectralPeak> findPeaks(
    const Spectrum& spectrum, double minHz, double maxHz,
    std::size_t maxPeaks, double floorRatio = 0.01);

// Soma de magnitude ao quadrado em [lowHz, highHz). Proporcional a energia da
// banda; NAO e energia fisica absoluta -- depende de escala do arquivo, janela
// e normalizacao. Use apenas para comparacoes relativas.
[[nodiscard]] double bandEnergy(const Spectrum& spectrum, double lowHz, double highHz);

// dB = 20*log10(amplitude + eps).
[[nodiscard]] double amplitudeToDecibels(double amplitude);

}  // namespace soundwave
