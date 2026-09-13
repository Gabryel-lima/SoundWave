#pragma once

#include <cstddef>

#include "soundwave/color/spectral.hpp"

// Tabela oficial das funcoes de correspondencia de cor CIE 1931 (observador de
// 2 graus), 380 a 780 nm em passos de 5 nm, com interpolacao linear.
//
// Isolada em seu proprio modulo para que a fonte dos dados colorimetricos seja
// substituivel (observador de 10 graus, CIE 2006, dados de um observador
// especifico) sem tocar em nenhuma outra camada do pipeline.
namespace soundwave::cie {

// Triestimulo de um estimulo monocromatico de potencia unitaria.
// Fora de [380, 780] nm devolve {0,0,0}: nao ha resposta visual.
[[nodiscard]] Xyz lookup(double wavelengthNm);

[[nodiscard]] double firstWavelengthNm();
[[nodiscard]] double lastWavelengthNm();
[[nodiscard]] std::size_t sampleCount();

}  // namespace soundwave::cie
