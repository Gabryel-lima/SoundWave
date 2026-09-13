#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace soundwave {

// Uma FFT assume que o bloco analisado se repete periodicamente. Se as bordas
// do bloco nao casam, a descontinuidade artificial espalha energia por todos os
// bins -- vazamento espectral. A janela atenua as bordas para suavizar isso, ao
// custo de alargar o lobo principal (pior resolucao). Nao existe janela otima:
// e sempre uma troca entre resolucao e vazamento.
enum class WindowType {
    Rectangular,     // sem janela. Melhor resolucao, pior vazamento.
    Hann,            // padrao do projeto. Bom equilibrio geral.
    Hamming,         // primeiro lobo lateral menor, decaimento mais lento.
    Blackman,        // vazamento bem menor, lobo principal mais largo.
    BlackmanHarris,  // vazamento minimo (-92 dB), pior resolucao.
};

[[nodiscard]] std::string_view windowTypeName(WindowType type);
// Retorna false se o nome nao for reconhecido; `out` fica intacto.
[[nodiscard]] bool parseWindowType(std::string_view name, WindowType& out);

// Coeficientes da janela, definicao periodica (w[n] usa n/N, nao n/(N-1)).
// A forma periodica e a correta para analise espectral: a simetrica introduz um
// vies de meia amostra que desloca levemente a fase estimada.
[[nodiscard]] std::vector<double> makeWindow(WindowType type, std::size_t size);

// Ganho coerente = media dos coeficientes. Uma senoide de amplitude A janelada
// produz um pico de A * ganhoCoerente * N / 2, entao dividimos por ele para
// recuperar A. Calculado a partir dos coeficientes reais, nao tabelado.
[[nodiscard]] double coherentGain(const std::vector<double>& window);

// Largura de banda equivalente de ruido, em bins. Para converter magnitude em
// densidade espectral de potencia e preciso dividir por ENBW -- caso contrario
// sinais de banda larga sao superestimados. Hann ~ 1.5, Rectangular = 1.0.
[[nodiscard]] double equivalentNoiseBandwidth(const std::vector<double>& window);

}  // namespace soundwave
