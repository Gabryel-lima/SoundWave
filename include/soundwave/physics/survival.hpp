#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include "soundwave/mapping/frequency_mapper.hpp"
#include "soundwave/physics/medium_filter.hpp"

namespace soundwave {

// ---------------------------------------------------------------------------
// SOBREVIVENCIA CONJUNTA
//
// Todos os mapeamentos deste projeto sao escolhas. Ate aqui, nenhum tinha um
// argumento a favor que nao fosse "parece razoavel". Este modulo introduz o
// primeiro CRITERIO EXTERNO, mensuravel, contra o qual compara-los:
//
//     J(M) = media sobre log f de  T_som(f) * T_luz(M(f))
//
// Le-se: se o som percorre um meio real e a luz correspondente percorre o mesmo
// meio, que fracao da informacao sobrevive aos dois trajetos?
//
// O criterio existe porque os dois filtros sao antagonicos (docs/physics.md):
// a agua preserva os GRAVES do som e os AZUIS da luz, mas todo mapeamento
// monotonico crescente leva grave em vermelho -- ou seja, manda o que o som
// preserva exatamente para o que a luz destroi.
//
// A medida em log f, e nao em f: a percepcao de altura e logaritmica, entao
// ponderar linearmente daria a ultima oitava metade do peso total.
//
// >>> ISTO NAO TORNA NENHUM MAPEAMENTO FISICO. <<<
// O criterio em si e uma escolha (por que sobrevivencia? em que meio? por que
// quanto caminho?). O ganho e outro: a escolha passa a ser OTIMIZADA CONTRA
// ALGO MENSURAVEL em vez de estetica. A classe de fidelidade continua
// Arbitrary -- mas por uma razao diferente, e a documentacao diz qual.
// ---------------------------------------------------------------------------

struct SurvivalScore {
    // O objetivo: media de T_som * T_luz sobre a banda audivel, em [0,1].
    double joint = 0.0;

    // Media de T_som sozinho. E o teto se a luz nao atenuasse nada.
    double acousticMean = 0.0;

    // Melhor T_luz disponivel no visivel -- o "azul" do meio.
    double opticalBest = 0.0;
    double opticalBestNm = 0.0;

    // Teto real: media de T_som(f) * melhor T_luz. Nenhum mapeamento alcanca,
    // porque alcancar exigiria mandar TODA frequencia ao mesmo lambda.
    double ceiling = 0.0;

    // joint / ceiling, em [0,1]. Quanto do possivel o mapeamento captura.
    double efficiency = 0.0;

    // Fracao do audivel cuja imagem cai no visivel. Um mapeamento pode ter J
    // alto por mandar tudo para fora do visivel (onde nao modelamos absorcao),
    // entao este campo e necessario para o numero nao enganar.
    double visibleFraction = 0.0;
};

// Pontua QUALQUER mapeador. E este o ponto: o criterio e externo aos
// mapeadores, e nao propriedade de nenhum deles.
[[nodiscard]] SurvivalScore jointSurvival(const FrequencyMapper& mapper,
                                          const MediumFilterSettings& settings,
                                          const MappingDomain& domain,
                                          std::size_t samples = 512);

// ---------------------------------------------------------------------------
// SEPARACAO DE ESCALAS -- por que o alinhamento e indefinido
//
// Um filtro so carrega informacao onde DISCRIMINA, isto e, onde o contraste
// entre o que ele preserva e o que ele destroi e apreciavel. Fora dessa faixa de
// distancia ele e transparente (nao filtra nada) ou opaco (mata tudo); nos dois
// casos, nao distingue.
//
// Medindo a janela de transicao de cada filtro em agua do mar:
//
//     luz : 0,285 m  a      1,76 m
//     som : 2.590 m  a  16.100 m
//
// Elas NAO SE SOBREPOEM -- estao separadas por um fator de ~1500 a 9000.
//
// Consequencia, e este e o resultado central deste modulo:
//
//   >>> Nao existe comprimento de caminho em que os dois filtros discriminem ao
//       mesmo tempo. Onde a luz distingue cores, o som e uniformemente
//       transparente. Onde o som distingue graves de agudos, a luz ja e
//       exatamente zero em todo o visivel. <<<
//
// Portanto alinhar os dois filtros nao e apenas pouco util: e INDEFINIDO, porque
// nao ha regime em que ambos carreguem informacao. Os dois dominios nao sao so
// antagonicos em direcao -- sao disjuntos em escala.
// ---------------------------------------------------------------------------

// Faixa de distancias em que um filtro discrimina, definida por contraste entre
// `minContrast` e `maxContrast` vezes entre o extremo preservado e o destruido.
struct TransitionWindow {
    double lowM = 0.0;
    double highM = 0.0;
    bool found = false;

    [[nodiscard]] double centreM() const;  // media geometrica
    [[nodiscard]] double decades() const;  // largura em decadas
};

struct ScaleSeparation {
    TransitionWindow acoustic;
    TransitionWindow optical;

    // true se as duas janelas se intersectam -- isto e, se existe alguma
    // distancia em que ambos os filtros carregam informacao.
    bool overlaps = false;

    // Razao entre os centros das janelas. Quantas ordens de grandeza separam os
    // dois regimes.
    double separationFactor = 0.0;
    double separationDecades = 0.0;
};

// Varre comprimentos de caminho e localiza as duas janelas de transicao.
[[nodiscard]] ScaleSeparation scaleSeparation(const MediumFilterSettings& settings,
                                              const MappingDomain& domain,
                                              double minContrast = 2.0,
                                              double maxContrast = 100.0);

// ---------------------------------------------------------------------------
// MAPEADOR ALINHADO
//
// Escolhe, dentro de uma familia DECLARADA de candidatos, aquele que maximiza
// J em um meio e caminho dados.
//
// A familia e pequena e explicita de proposito. Otimizar sobre todas as funcoes
// monotonicas seria degenerado: sem penalidade por comprimir, o otimo empilha
// quase toda a banda no lambda de melhor transmitancia e colapsa a cor. E o
// rearranjo de Hardy-Littlewood (o otimo entre mapas que preservam medida)
// produz lambda saltando dos dois lados do pico optico, visualmente caotico.
//
// Uma escolha entre quatro candidatos nomeados nao tem esses problemas e ainda
// assim e decidida por MEDIDA, nao por gosto.
//
// RESULTADO MEDIDO, E E NEGATIVO: em agua, os quatro candidatos empatam dentro
// de 0,1%. A razao e a separacao de escalas acima -- nas distancias em que a luz
// discrimina, T_som e praticamente constante em toda a banda audivel, e entao
// J vira a media de T_luz sobre a imagem. Como os quatro candidatos induzem a
// MESMA medida no visivel (uniforme em log lambda), so percorrida em ordens
// diferentes, J e identico por simetria.
//
// O mapeador e mantido porque MEDIR isso e o valor: ele documenta um resultado
// nulo de forma verificavel, em vez de deixar a hipotese em aberto. E em um meio
// hipotetico cujas janelas se sobrepusessem, ele encontraria o alinhamento.
// ---------------------------------------------------------------------------
class AlignedMapper final : public FrequencyMapper {
public:
    enum class Candidate {
        LogAscending,     // grave -> vermelho. O padrao classico.
        LogDescending,    // grave -> violeta. Inverte a orientacao.
        OctaveAscending,  // classe de altura, crescente
        OctaveDescending, // classe de altura, decrescente
    };

    [[nodiscard]] static std::string_view candidateName(Candidate candidate);

    AlignedMapper(MediumFilterSettings settings, MappingDomain domain = {},
                  double referenceHz = 440.0);

    [[nodiscard]] double map(double frequencyHz) const override;
    [[nodiscard]] std::optional<double> inverse(double emFrequencyHz) const override;
    [[nodiscard]] std::string name() const override { return "aligned"; }
    [[nodiscard]] std::string describe() const override;
    [[nodiscard]] const MappingDomain& domain() const override { return domain_; }
    [[nodiscard]] bool isMonotonic() const override;
    [[nodiscard]] bool preservesOctaveEquivalence() const override;

    [[nodiscard]] Candidate chosen() const { return chosen_; }
    [[nodiscard]] const SurvivalScore& score() const { return score_; }
    // Pontuacao de cada candidato, na ordem do enum. Para relatorio.
    [[nodiscard]] const std::array<SurvivalScore, 4>& allScores() const { return scores_; }

    [[nodiscard]] FidelityBudget fidelity() const;

    // Avalia um candidato especifico -- usado internamente e nos testes.
    [[nodiscard]] double evaluate(Candidate candidate, double frequencyHz) const;

private:
    MediumFilterSettings settings_;
    MappingDomain domain_;
    double referenceHz_;
    Candidate chosen_ = Candidate::LogAscending;
    SurvivalScore score_;
    std::array<SurvivalScore, 4> scores_{};
};

}  // namespace soundwave
