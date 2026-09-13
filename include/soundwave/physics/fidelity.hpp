#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace soundwave {

// ---------------------------------------------------------------------------
// CLASSIFICACAO DE FIDELIDADE
//
// Um simulador so e confiavel se disser em que medida e confiavel. Este modulo
// existe para que cada grandeza do pipeline declare TRES coisas:
//
//   1. de que natureza ela e  (exata? medida? convencao? escolha?)
//   2. qual a incerteza       (quando isso sequer faz sentido)
//   3. de onde ela veio       (citacao verificavel)
//
// A distincao decisiva -- e o motivo deste modulo existir -- e que nem toda
// imprecisao e do mesmo tipo:
//
//   * "lambda = c/f tem erro de 1e-16" e uma afirmacao sobre aritmetica.
//   * "a velocidade do som na agua tem erro de 0,05%" e uma afirmacao sobre
//     uma MEDIDA.
//   * "o mapeamento f_som -> f_EM tem erro de X%" NAO SIGNIFICA NADA, porque
//     nao existe valor verdadeiro contra o qual comparar.
//
// Juntar os tres em uma barra de erro unica seria pseudo-precisao: daria ao
// resultado final uma aparencia de medida que ele nao tem. Por isso a regra de
// propagacao abaixo e deliberadamente brutal.
// ---------------------------------------------------------------------------

enum class Fidelity {
    // Exato por definicao ou por identidade matematica. Erro limitado apenas
    // pela aritmetica de ponto flutuante. Ex.: c no SI, lambda = c/f, E = hf.
    Exact,

    // Medida empirica com incerteza declarada. Ex.: velocidade do som na agua.
    Measured,

    // Ajuste empirico valido em uma faixa declarada, com erro declarado fora
    // ou dentro dela. Ex.: Sellmeier, Francois-Garrison, ISO 9613-1.
    ModelFit,

    // Convencao padronizada: nao ha valor "verdadeiro", mas ha um acordo
    // amplamente adotado e sem ambiguidade. Ex.: primarias sRGB, D65, os
    // limites 400-750 nm do visivel, o observador CIE 1931 2 graus.
    Convention,

    // Parametro livre sem base fisica. NAO tem incerteza: nao existe valor
    // verdadeiro. Ex.: a funcao de mapeamento f_som -> f_EM, a constante de
    // escala K, a normalizacao de luminancia.
    Arbitrary,
};

[[nodiscard]] std::string_view fidelityName(Fidelity tier);

// Frase curta explicando o que a classe significa para quem le um resultado.
[[nodiscard]] std::string_view fidelityMeaning(Fidelity tier);

// Ordem de "confiabilidade decrescente": Exact < Measured < ModelFit <
// Convention < Arbitrary. Usada para achar o elo mais fraco de uma cadeia.
[[nodiscard]] int fidelityRank(Fidelity tier);

// Uma afirmacao sobre uma grandeza usada no pipeline.
struct FidelityClaim {
    std::string quantity;        // "velocidade do som (agua do mar)"
    Fidelity tier = Fidelity::Arbitrary;

    // Incerteza RELATIVA (1 = 100%). NaN quando nao se aplica -- que e sempre
    // o caso de Arbitrary, e as vezes de Convention.
    double relativeUncertainty = 0.0;

    std::string source;          // "Mackenzie (1981), JASA 70(3)"
    std::string validity;        // "0-30 C, 0-40 ppt, 0-8000 m"
    std::string caveat;          // limitacao conhecida, em uma frase

    [[nodiscard]] bool hasUncertainty() const;
    // Linha de uma so linha para relatorio/manifesto.
    [[nodiscard]] std::string summary() const;
};

// Orcamento de incerteza de uma cadeia de calculo.
//
// Reporta DUAS coisas separadas de proposito:
//   - o elo mais fraco (a classe que governa a interpretacao do resultado);
//   - a incerteza combinada das etapas que de fato tem incerteza.
//
// Se qualquer etapa for Arbitrary, a incerteza combinada e reportada como
// indefinida. Isso nao e conservadorismo: e correcao. Um numero como
// "566,1 +/- 0,3 nm" para um comprimento de onda que so existe porque alguem
// escolheu uma funcao seria uma mentira com casas decimais.
class FidelityBudget {
public:
    void add(FidelityClaim claim);
    void merge(const FidelityBudget& other);

    [[nodiscard]] const std::vector<FidelityClaim>& claims() const { return claims_; }
    [[nodiscard]] bool empty() const { return claims_.empty(); }

    // Classe do elo mais fraco da cadeia.
    [[nodiscard]] Fidelity weakestTier() const;

    // true se alguma etapa for Arbitrary -- ou seja, se o resultado final NAO
    // e uma medida e nao admite barra de erro.
    [[nodiscard]] bool hasArbitraryStep() const;

    // Raiz da soma dos quadrados das incertezas relativas das etapas que tem
    // incerteza. Devolve NaN quando hasArbitraryStep(), de proposito.
    //
    // A soma quadratica pressupoe independencia entre as fontes de erro; isso
    // vale aqui porque as etapas vem de medidas independentes (acustica,
    // colorimetria, optica). Onde nao valesse, a soma linear seria o limite
    // conservador correto.
    [[nodiscard]] double combinedRelativeUncertainty() const;

    // Relatorio legivel, pronto para o manifesto.
    [[nodiscard]] std::string report() const;

private:
    std::vector<FidelityClaim> claims_;
};

// Afirmacoes reutilizaveis para as etapas fixas do pipeline.
namespace claims {
[[nodiscard]] FidelityClaim speedOfLight();
[[nodiscard]] FidelityClaim wavelengthFromFrequency();
[[nodiscard]] FidelityClaim cie1931Observer();
[[nodiscard]] FidelityClaim srgbEncoding();
[[nodiscard]] FidelityClaim visibleRangeConvention();
[[nodiscard]] FidelityClaim gamutMapping();
[[nodiscard]] FidelityClaim luminanceNormalisation();
[[nodiscard]] FidelityClaim arbitraryMapping(const std::string& mapperName);
[[nodiscard]] FidelityClaim fftResolution(double resolutionHz, double frequencyHz);
}  // namespace claims

}  // namespace soundwave
