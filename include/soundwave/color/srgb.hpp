#pragma once

#include <cstdint>
#include <string_view>

#include "soundwave/color/spectral.hpp"

namespace soundwave {

// sRGB linear (luz), componentes tipicamente em [0,1] mas podendo sair fora
// antes do tratamento de gamut. TODA mistura de cores deve acontecer aqui ou em
// XYZ, nunca em Rgb codificado -- ver blendLinear().
struct LinearRgb {
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
};

// sRGB codificado, 8 bits por canal. Formato de saida apenas.
struct Rgb {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};

// -----------------------------------------------------------------------------
// Tratamento de gamut.
//
// O problema: o sRGB e um triangulo no diagrama de cromaticidade, e TODA a
// curva espectral (o "locus" monocromatico) fica fora dele, exceto onde toca
// as tres primarias. Logo, converter uma cor espectral pura para sRGB quase
// sempre produz pelo menos um componente negativo -- uma quantidade de luz
// negativa, que nenhum monitor pode emitir.
//
// Nao ha solucao correta, so escolhas com perdas diferentes. O que NAO se pode
// fazer e ignorar o problema: cortar negativos silenciosamente desloca o matiz.
// -----------------------------------------------------------------------------
enum class GamutStrategy {
    // Ceifa negativos em 0. Barato; distorce o matiz de forma inconsistente.
    Clip,
    // Soma branco ate o menor componente chegar a zero e, se o maior passar de
    // 1, divide os TRES pelo maior. Preserva o matiz, perde saturacao e brilho.
    // Padrao do projeto -- e a escolha usual em renderizacao espectral.
    //
    // O segundo passo nao e opcional: dessaturar sozinho deixa o maior canal
    // muito acima de 1, e um clamp por canal nesse ponto muda as razoes entre
    // canais, ou seja, muda o matiz (650 nm virava magenta em vez de vermelho).
    Desaturate,
    // Igual, mas normaliza o maior componente para 1 SEMPRE, inclusive quando a
    // cor ja cabia no gamut. Maximiza o brilho de cada comprimento de onda e,
    // com isso, descarta a luminancia relativa entre eles.
    DesaturateAndScale,
};

[[nodiscard]] std::string_view gamutStrategyName(GamutStrategy strategy);
[[nodiscard]] bool parseGamutStrategy(std::string_view name, GamutStrategy& out);

// Matriz XYZ -> sRGB linear, primarias sRGB com ponto branco D65.
[[nodiscard]] LinearRgb xyzToLinearRgb(const Xyz& colour);
[[nodiscard]] Xyz linearRgbToXyz(const LinearRgb& colour);

// true se a cor cabe no gamut sRGB (nenhum componente negativo, nenhum > 1).
[[nodiscard]] bool isInGamut(const LinearRgb& colour, double tolerance = 1e-9);
// Quanto a cor extrapola: 0 dentro do gamut, crescendo conforme sai.
[[nodiscard]] double gamutExcursion(const LinearRgb& colour);

[[nodiscard]] LinearRgb mapIntoGamut(const LinearRgb& colour, GamutStrategy strategy);

// Funcao de transferencia sRGB (a chamada "gamma", que na verdade e uma curva
// composta: um segmento linear perto do preto e uma potencia 1/2.4 acima dele).
// Converte luz linear -> valor codificado.
[[nodiscard]] double encodeSrgb(double linear);
[[nodiscard]] double decodeSrgb(double encoded);

// Pipeline completo: linear -> gamut -> codificacao -> 8 bits.
[[nodiscard]] Rgb toRgb8(const LinearRgb& colour, GamutStrategy strategy = GamutStrategy::Desaturate);
[[nodiscard]] Rgb xyzToRgb8(const Xyz& colour, GamutStrategy strategy = GamutStrategy::Desaturate);

// Mistura em luz linear. Misturar em sRGB codificado -- que e o que quase todo
// codigo de visualizacao faz por engano -- escurece o resultado, porque a media
// de dois valores gamma-codificados nao e a codificacao da media da luz.
[[nodiscard]] LinearRgb blendLinear(const LinearRgb& a, const LinearRgb& b, double weightB);
[[nodiscard]] Xyz blendXyz(const Xyz& a, const Xyz& b, double weightB);

}  // namespace soundwave
