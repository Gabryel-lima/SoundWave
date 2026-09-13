#pragma once

#include "soundwave/core/constants.hpp"

namespace soundwave {

// Triestimulo CIE 1931 XYZ. Espaco de conexao independente de dispositivo.
struct Xyz {
    double x = 0.0;
    double y = 0.0;  // Y e a luminancia
    double z = 0.0;
};

// ---------------------------------------------------------------------------
// CAMADA 3 -- CONVERSAO
//
// Aqui ha de fato fisica e ciencia da visao medida, mas so ate certo ponto.
// E importante saber onde cada coisa vale:
//
//  1. lambda = c / f            -> FISICA. Exato no vacuo.
//  2. lambda -> XYZ             -> COLORIMETRIA. Media empirica das respostas
//                                  de observadores humanos (CIE 1931, campo de
//                                  2 graus). E uma medida, com incerteza e com
//                                  variacao individual conhecida.
//  3. XYZ -> sRGB               -> CONVENCAO DE ENGENHARIA. Define um monitor
//                                  ideal. Ver color/srgb.hpp.
//
// Limitacao que precisa ficar dita: quase nenhuma cor espectral pura cabe no
// gamut do sRGB. O que a tela mostra e sempre uma APROXIMACAO dessaturada da
// cor monocromatica correspondente. Nao e um bug -- e o limite fisico de um
// display de tres primarias.
// ---------------------------------------------------------------------------

// lambda = c / f. Entrada em Hz, saida em metros. Fisicamente exata.
[[nodiscard]] double wavelengthFromFrequency(double frequencyHz);

// f = c / lambda. Entrada em metros, saida em Hz.
[[nodiscard]] double frequencyFromWavelength(double wavelengthM);

[[nodiscard]] double metresToNanometres(double metres);
[[nodiscard]] double nanometresToMetres(double nanometres);

// true se lambda (em metros) cai na faixa visivel convencional 400..750 nm.
// As bordas sao convencao, nao um limiar fisico: a sensibilidade decai de forma
// continua e nao existe corte objetivo.
[[nodiscard]] bool isVisibleWavelength(double wavelengthM);

// ---------------------------------------------------------------------------
// Funcoes de correspondencia de cor CIE 1931, observador padrao de 2 graus.
//
// Implementadas pela TABELA OFICIAL (380-780 nm, passo de 5 nm, interpolacao
// linear) -- ver color/cie_tables.hpp, que explica em detalhe por que um ajuste
// analitico nao serve aqui.
//
// Resumo: um ajuste com erro absoluto de ~1% do pico e inutilizavel nas bordas
// do visivel, onde as proprias funcoes valem ~1e-5. Cor depende das RAZOES
// entre x_barra, y_barra e z_barra, e um erro absoluto minusculo sobre valores
// minusculos e um erro relativo gigante. Com o ajuste, 736 nm saia VERDE.
// ---------------------------------------------------------------------------
[[nodiscard]] double cieX(double wavelengthNm);
[[nodiscard]] double cieY(double wavelengthNm);
[[nodiscard]] double cieZ(double wavelengthNm);

// Ajuste analitico multi-lobo de Wyman, Sloan & Shirley (JCGT 2(2), 2013).
//
// Mantido NAO como alternativa utilizavel, mas como contra-exemplo verificavel:
// os testes comparam este ajuste com a tabela e documentam onde ele falha. E
// barato, continuo e correto no centro do visivel; e inadequado nas bordas,
// que e exatamente onde este projeto costuma cair quando o mapeamento leva o
// sinal para os extremos da faixa. Nao use para produzir cor.
[[nodiscard]] double cieXAnalyticFit(double wavelengthNm);
[[nodiscard]] double cieYAnalyticFit(double wavelengthNm);
[[nodiscard]] double cieZAnalyticFit(double wavelengthNm);

// Triestimulo de um estimulo monocromatico de potencia unitaria em `wavelengthM`.
// Fora do visivel devolve {0,0,0} -- corretamente, pois nao ha resposta do olho.
[[nodiscard]] Xyz spectralXyz(double wavelengthM);

// Ponto branco D65 normalizado para Y = 1. Referencia do sRGB.
[[nodiscard]] Xyz whitePointD65();

// Reescala para Y = targetLuminance, preservando a cromaticidade.
//
// Escolha de VISUALIZACAO, nao de fisica: a eficiencia luminosa do olho cai
// quase a zero em 400 e 750 nm, entao cores espectrais nas bordas do visivel
// sairiam praticamente pretas. Normalizar torna as cores comparaveis entre si,
// ao custo de descartar a informacao real de brilho. Deve ser configuravel e
// declarado no manifesto.
[[nodiscard]] Xyz normaliseLuminance(const Xyz& colour, double targetLuminance = 1.0);

}  // namespace soundwave
