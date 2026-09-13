#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace soundwave {

// Sinal mono em ponto flutuante, escala nominal [-1, 1].
//
// Todo o pipeline trabalha em mono e double. Motivos:
//  - a FFT e definida sobre um sinal escalar; estereo exigiria decidir se cada
//    canal e analisado separadamente ou somado, e essa decisao pertence ao
//    chamador, nao ao analisador;
//  - double elimina ruido de quantizacao do proprio processamento como fonte de
//    erro nos testes, isolando o erro do algoritmo.
struct PcmBuffer {
    double sampleRate = 0.0;
    std::vector<double> samples;
    std::string sourceName;  // rotulo para o manifesto de reprodutibilidade

    [[nodiscard]] std::size_t frameCount() const { return samples.size(); }
    [[nodiscard]] double durationSeconds() const {
        return sampleRate > 0.0 ? static_cast<double>(samples.size()) / sampleRate : 0.0;
    }
    [[nodiscard]] bool empty() const { return samples.empty(); }

    // Maior valor absoluto. 0 para silencio exato.
    [[nodiscard]] double peakAmplitude() const;
    // Raiz do valor quadratico medio.
    [[nodiscard]] double rmsAmplitude() const;
};

// Mistura canais intercalados para mono somando e dividindo pelo numero de
// canais. Isto NAO preserva energia quando os canais sao correlacionados de
// forma diferente entre si; para analise espectral qualitativa e aceitavel, para
// medida absoluta de nivel nao e.
[[nodiscard]] std::vector<double> downmixToMono(const std::vector<double>& interleaved,
                                                std::size_t channels);

}  // namespace soundwave
