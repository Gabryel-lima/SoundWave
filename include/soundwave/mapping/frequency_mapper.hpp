#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "soundwave/core/constants.hpp"

namespace soundwave {

// ---------------------------------------------------------------------------
// CAMADA 2 -- TRANSFORMACAO
//
// Esta camada e a unica do pipeline que NAO e fisica nem perceptual. Ela e uma
// escolha.
//
// Nao existe relacao natural entre uma frequencia sonora e uma frequencia
// eletromagnetica. Som e uma onda de pressao em um meio material; luz e um campo
// eletromagnetico que se propaga no vacuo. Elas nao compartilham mecanismo,
// unidade de escala ou ponto de referencia comum. M(f_som) = f_EM e uma funcao
// inventada por nos, e trocar a funcao troca completamente o resultado.
//
// Por isso a interface obriga toda implementacao a se descrever: describe() e o
// que entra no manifesto de reprodutibilidade. Um resultado do SoundWave sem a
// funcao de mapeamento declarada nao significa nada.
// ---------------------------------------------------------------------------

// O que fazer com frequencias fora de [sourceMinHz, sourceMaxHz].
enum class OutOfRangePolicy {
    Clamp,       // prende na borda. Padrao: seguro, mas achata os extremos.
    Extrapolate, // continua a funcao. Pode sair do visivel -- deliberadamente.
    Discard,     // devolve NaN. O chamador decide ignorar o bin.
};

[[nodiscard]] std::string_view outOfRangePolicyName(OutOfRangePolicy policy);
[[nodiscard]] bool parseOutOfRangePolicy(std::string_view name, OutOfRangePolicy& out);

// Dominio e contradominio da transformacao. Os quatro valores sao convencao,
// nao medida -- inclusive os limites do visivel (ver core/constants.hpp).
struct MappingDomain {
    double sourceMinHz = kAudibleMinHz;
    double sourceMaxHz = kAudibleMaxHz;
    double targetMinHz = kVisibleMinHz;
    double targetMaxHz = kVisibleMaxHz;

    [[nodiscard]] bool valid() const;

    // Quantas oitavas (fatores de 2) cada faixa cobre.
    //
    // Esta e a assimetria mais importante do projeto e vale encarar de frente:
    // a faixa audivel cobre ~9,97 oitavas (20 Hz a 20 kHz e um fator de 1000);
    // o visivel cobre ~0,91 (400 a 750 THz e um fator de apenas 1,875). Qualquer
    // mapeamento monotonico portanto comprime a escala por um fator de ~11.
    // Consequencia direta: uma oitava musical -- a relacao
    // perceptual mais forte que existe em musica -- vira um deslocamento de
    // frequencia EM de ~6,5%, praticamente invisivel. Ver docs/mapping.md.
    [[nodiscard]] double sourceOctaves() const;
    [[nodiscard]] double targetOctaves() const;
    [[nodiscard]] double compressionRatio() const;  // sourceOctaves / targetOctaves
};

class FrequencyMapper {
public:
    virtual ~FrequencyMapper() = default;

    // f_som (Hz) -> f_EM (Hz). Devolve NaN quando a politica e Discard e a
    // entrada esta fora do dominio.
    [[nodiscard]] virtual double map(double frequencyHz) const = 0;

    // Inversa, quando existe. std::nullopt quando o mapeamento nao e injetivo
    // (OctaveMapper) ou o valor esta fora da imagem. Nao e um detalhe: um
    // mapeamento sem inversa nao permite responder "que som gerou esta cor?".
    [[nodiscard]] virtual std::optional<double> inverse(double emFrequencyHz) const {
        (void)emFrequencyHz;
        return std::nullopt;
    }

    [[nodiscard]] virtual std::string name() const = 0;

    // Frase legivel que vai para o manifesto do experimento. Deve dizer a
    // formula e o que ela preserva ou destroi.
    [[nodiscard]] virtual std::string describe() const = 0;

    [[nodiscard]] virtual const MappingDomain& domain() const = 0;

    // true se map() for estritamente crescente em todo o dominio.
    [[nodiscard]] virtual bool isMonotonic() const { return true; }

    // true se f e 2f mapearem para o mesmo f_EM.
    [[nodiscard]] virtual bool preservesOctaveEquivalence() const { return false; }
};

}  // namespace soundwave
