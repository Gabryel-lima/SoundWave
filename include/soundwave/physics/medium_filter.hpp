#pragma once

#include <vector>

#include "soundwave/core/spectrum.hpp"
#include "soundwave/physics/medium.hpp"

namespace soundwave {

// ---------------------------------------------------------------------------
// FILTRAGEM PELO MEIO
//
// Aqui esta a parte da ideia "inserir o espectro no ambiente" que de fato
// funciona -- e funciona porque nao depende de frequencia mudar.
//
// O meio NAO desloca frequencias. Mas ele ATENUA de forma dependente da
// frequencia, nos dois dominios. Isso e estrutura compartilhada real, medida, e
// nao uma funcao inventada.
//
// O resultado central, e ele e contraintuitivo:
//
//     A AGUA E PASSA-BAIXA PARA SOM E PASSA-ALTA PARA LUZ.
//
//   * Som: absorcao cresce ~f^2. Graves atravessam milhares de quilometros
//     (o canal SOFAR); ultrassom morre em metros.
//   * Luz: o vermelho e absorvido em poucos metros; o azul penetra dezenas de
//     metros. E por isso que o mar e azul e que tudo fica azulado no fundo.
//
// Ou seja: o mesmo meio preserva os GRAVES do som e os AZUIS da luz. Se o seu
// mapeamento leva grave em vermelho -- como todos os monotonicos deste projeto
// fazem -- entao o meio destroi exatamente aquilo que o mapeamento tentou
// preservar. Os dois filtros sao antagonicos.
//
// Isso e um resultado, nao um obstaculo: mostra que "o mesmo ambiente para os
// dois" nao produz equivalencia, e sim conflito.
// ---------------------------------------------------------------------------

struct MediumFilterSettings {
    Medium acoustic = media::seaWater();
    Medium optical = media::seaWater();
    Conditions conditions;
    double pathLengthM = 10.0;  // distancia percorrida em cada dominio
};

// Transmitancia (0..1) de uma frequencia sonora apos `pathLengthM` no meio.
[[nodiscard]] double acousticTransmittance(const Medium& medium, double frequencyHz,
                                           double pathLengthM, const Conditions& conditions);

// Transmitancia (0..1) de um comprimento de onda de vacuo apos `pathLengthM`.
[[nodiscard]] double opticalTransmittance(const Medium& medium, double vacuumWavelengthNm,
                                          double pathLengthM);

// Aplica a atenuacao acustica a um espectro, bin a bin. Devolve uma copia:
// o espectro original continua sendo o dado medido, e o filtrado e um derivado.
[[nodiscard]] Spectrum applyAcousticFilter(const Spectrum& spectrum,
                                           const MediumFilterSettings& settings);

// Perfil do filtro em uma frequencia/comprimento de onda, para relatorio.
struct FilterPoint {
    double frequencyHz = 0.0;
    double wavelengthNm = 0.0;      // so para o lado optico
    double attenuationDbPerM = 0.0;
    double transmittance = 0.0;     // apos pathLengthM
    double halfDistanceM = 0.0;     // distancia para cair a metade (-3 dB)
};

[[nodiscard]] std::vector<FilterPoint> acousticProfile(const MediumFilterSettings& settings,
                                                       const std::vector<double>& frequenciesHz);

[[nodiscard]] std::vector<FilterPoint> opticalProfile(const MediumFilterSettings& settings,
                                                      const std::vector<double>& wavelengthsNm);

// Comprimento de onda de vacuo (nm) de maior transmitancia no visivel -- o
// "azul" do meio. Para a agua cai perto do minimo de absorcao.
[[nodiscard]] double mostTransmittedVisibleNm(const Medium& medium);

// Frequencia sonora de maior transmitancia dentro de [lowHz, highHz].
// Para meios com absorcao crescente em f, sai sempre no extremo grave -- o que
// e o resultado, nao um artefato.
[[nodiscard]] double mostTransmittedAudibleHz(const Medium& medium, double lowHz, double highHz,
                                              const Conditions& conditions);

[[nodiscard]] FidelityBudget filterFidelity(const MediumFilterSettings& settings,
                                            double vacuumWavelengthNm);

}  // namespace soundwave
