#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "soundwave/audio/pcm_buffer.hpp"
#include "soundwave/core/spectrum.hpp"
#include "soundwave/dsp/fft.hpp"
#include "soundwave/dsp/window.hpp"

namespace soundwave {

struct AnalyzerSettings {
    std::size_t fftSize = 4096;
    std::size_t hopSize = 1024;  // avanco entre blocos; < fftSize gera sobreposicao
    WindowType window = WindowType::Hann;

    // Remove a media do bloco antes de janelar. Um offset de continua aparece
    // como um pico enorme em DC que vaza para os bins vizinhos e pode dominar a
    // deteccao de frequencia dominante. Praticamente sempre deve ficar ligado.
    bool removeDc = true;

    // Divide o bloco pelo seu proprio pico. Torna a cor independente do volume
    // -- o que pode ser desejavel (comparar timbres) ou enganoso (trechos de
    // silencio viram ruido amplificado). Desligado por padrao.
    bool normalizeBlocks = false;

    // Abaixo deste pico o bloco e tratado como silencio e devolvido zerado, em
    // vez de amplificar ruido de fundo. Relevante so com normalizeBlocks.
    double silenceThreshold = 1e-6;
};

// Converte blocos de amostras em espectros. Reutiliza os mesmos buffers entre
// chamadas: nenhuma alocacao no caminho quente depois da construcao.
class SpectrumAnalyzer {
public:
    explicit SpectrumAnalyzer(AnalyzerSettings settings, double sampleRate);

    [[nodiscard]] const AnalyzerSettings& settings() const { return settings_; }
    [[nodiscard]] double sampleRate() const { return sampleRate_; }
    [[nodiscard]] double resolutionHz() const;

    // Analisa exatamente fftSize amostras a partir de `samples[offset]`. Se o
    // sinal acabar antes, o restante e preenchido com zeros (zero-padding), o
    // que interpola o espectro sem acrescentar informacao real.
    [[nodiscard]] Spectrum analyzeBlock(const std::vector<double>& samples, std::size_t offset) const;

    // Analisa o sinal inteiro em blocos sobrepostos, do inicio ao fim.
    [[nodiscard]] std::vector<Spectrum> analyzeAll(const PcmBuffer& buffer) const;

    // Quantos blocos analyzeAll produzira para um sinal deste tamanho.
    [[nodiscard]] std::size_t frameCountFor(std::size_t sampleCount) const;

private:
    AnalyzerSettings settings_;
    double sampleRate_;
    std::vector<double> window_;
    double windowSum_ = 0.0;  // = N * ganhoCoerente; normaliza a magnitude
    std::shared_ptr<const Fft> fft_;
};

}  // namespace soundwave
