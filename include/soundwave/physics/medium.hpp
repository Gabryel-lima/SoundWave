#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "soundwave/physics/fidelity.hpp"

namespace soundwave {

// ---------------------------------------------------------------------------
// MEIOS DE PROPAGACAO
//
// Som e luz atravessam meios materiais e sao afetados por eles -- mas NAO da
// forma que a intuicao sugere. O ponto que este modulo existe para deixar
// impossivel de errar:
//
//   >>> A FREQUENCIA NAO MUDA AO CRUZAR PARA OUTRO MEIO. <<<
//
// O que muda e a velocidade e, por consequencia, o comprimento de onda. A razao
// e a continuidade de fase na interface: a fronteira e forcada a oscilar na
// frequencia da onda incidente, e e ela a fonte da onda transmitida. Se a
// frequencia mudasse, os dois lados deixariam de casar em fase.
//
// Consequencia perceptual verificavel: um objeto vermelho continua vermelho
// visto debaixo d'agua, embora lambda tenha encolhido de 650 para ~488 nm.
// COR SEGUE FREQUENCIA, nao comprimento de onda no meio.
//
// Corolario que aparece em MediumComparison abaixo: trocar de meio multiplica
// todos os comprimentos de onda de uma faixa por um fator constante, e um fator
// constante NAO altera a razao entre os extremos da faixa. A compressao de ~11x
// entre audivel e visivel e, portanto, invariante sob mudanca de meio. Ver
// docs/physics.md.
// ---------------------------------------------------------------------------

// Condicoes ambientais. Os modelos empiricos abaixo sao funcoes destas.
struct Conditions {
    double temperatureC = 20.0;
    double pressureKPa = 101.325;   // ar
    double relativeHumidity = 70.0; // ar, em %
    double salinityPpt = 35.0;      // agua do mar
    double depthM = 0.0;            // agua
    double pH = 8.1;                // agua do mar
};

// Como cada meio calcula suas propriedades. Explicito para que o relatorio de
// fidelidade possa citar o modelo exato usado.
enum class SoundSpeedModel {
    None,          // meio sem som (vacuo)
    IdealGas,      // c = 331.3 * sqrt(1 + T/273.15)
    Mackenzie1981, // agua (doce e do mar)
    ConstantSolid, // solidos: valor unico tabelado
};

enum class SoundAbsorptionModel {
    None,
    Iso9613Air,          // ISO 9613-1:1993
    FrancoisGarrison1982, // agua
    NotModelled,         // solidos: nao modelado neste projeto
};

enum class RefractiveIndexModel {
    Vacuum,       // n = 1 exatamente
    ConstantAir,  // n ~ 1.000273, dispersao desprezivel aqui
    SellmeierWater20C,   // Daimon & Masumura (2007), 20 C
    SellmeierFusedSilica, // Malitson (1965)
    ConstantIce,  // n ~ 1.31
};

enum class OpticalAbsorptionModel {
    Transparent,       // vacuo
    NegligibleAir,
    Segelstein1981Water,
    NotModelled,
};

// Um meio, com as propriedades dos DOIS dominios.
struct Medium {
    std::string name;
    std::string description;

    SoundSpeedModel soundSpeed = SoundSpeedModel::None;
    SoundAbsorptionModel soundAbsorption = SoundAbsorptionModel::None;
    RefractiveIndexModel refractiveIndex = RefractiveIndexModel::Vacuum;
    OpticalAbsorptionModel opticalAbsorption = OpticalAbsorptionModel::Transparent;

    double constantSoundSpeedMs = 0.0;  // para ConstantSolid
    double constantRefractiveIndex = 1.0;

    // Som mecanico exige meio material. O vacuo e o caso em que a pergunta do
    // projeto nem chega a ser formulavel -- e vale te-lo explicito.
    [[nodiscard]] bool carriesSound() const { return soundSpeed != SoundSpeedModel::None; }
};

// Meios disponiveis, com dados de fontes citadas.
namespace media {
[[nodiscard]] Medium vacuum();
[[nodiscard]] Medium air();
[[nodiscard]] Medium freshWater();
[[nodiscard]] Medium seaWater();
[[nodiscard]] Medium ice();
[[nodiscard]] Medium fusedSilica();

[[nodiscard]] std::vector<Medium> all();
[[nodiscard]] std::optional<Medium> byName(std::string_view name);
}  // namespace media

// ---------------------------------------------------------------------------
// Propriedades acusticas
// ---------------------------------------------------------------------------

// Velocidade do som (m/s). 0 quando o meio nao propaga som.
[[nodiscard]] double soundSpeedMs(const Medium& medium, const Conditions& conditions);

// Atenuacao acustica em dB/m. 0 quando nao modelada -- e a reivindicacao de
// fidelidade correspondente diz isso, em vez de fingir que a atenuacao e nula.
[[nodiscard]] double soundAbsorptionDbPerMetre(const Medium& medium, double frequencyHz,
                                               const Conditions& conditions);

// lambda_som = v / f, no meio dado.
[[nodiscard]] double soundWavelengthM(const Medium& medium, double frequencyHz,
                                      const Conditions& conditions);

// ---------------------------------------------------------------------------
// Propriedades opticas
// ---------------------------------------------------------------------------

// Indice de refracao no comprimento de onda DE VACUO dado (nm).
[[nodiscard]] double refractiveIndexAt(const Medium& medium, double vacuumWavelengthNm);

// Atenuacao optica em dB/m no comprimento de onda de vacuo dado.
[[nodiscard]] double opticalAbsorptionDbPerMetre(const Medium& medium, double vacuumWavelengthNm);

// lambda no meio = lambda_vacuo / n(lambda_vacuo).
[[nodiscard]] double lightWavelengthInMediumM(const Medium& medium, double vacuumWavelengthNm);

// Inverso: dado um lambda medido DENTRO do meio, qual lambda de vacuo o produz?
// Resolvido por iteracao de ponto fixo, porque n depende de lambda.
// std::nullopt quando nao converge ou cai fora da faixa de validade do modelo.
[[nodiscard]] std::optional<double> vacuumWavelengthFromMediumNm(const Medium& medium,
                                                                 double mediumWavelengthNm);

// ---------------------------------------------------------------------------
// Reivindicacoes de fidelidade de cada modelo
// ---------------------------------------------------------------------------
[[nodiscard]] FidelityClaim soundSpeedClaim(const Medium& medium);
[[nodiscard]] FidelityClaim soundAbsorptionClaim(const Medium& medium);
[[nodiscard]] FidelityClaim refractiveIndexClaim(const Medium& medium);
[[nodiscard]] FidelityClaim opticalAbsorptionClaim(const Medium& medium, double vacuumWavelengthNm);

// ---------------------------------------------------------------------------
// A demonstracao da invariancia
// ---------------------------------------------------------------------------

// Extremos de uma faixa em um meio, e a razao entre eles.
struct RangeSpan {
    double atLowFrequencyM = 0.0;   // lambda no extremo de baixa frequencia
    double atHighFrequencyM = 0.0;  // lambda no extremo de alta frequencia
    double ratio = 0.0;             // atLow / atHigh
    double octaves = 0.0;           // log2(ratio)
};

// Faixa de comprimentos de onda acusticos de [fLow, fHigh] neste meio.
//
// O resultado interessante: `ratio` sai SEMPRE igual a fHigh/fLow, qualquer que
// seja o meio, porque v cancela. Verificado em tests/test_physics.cpp.
[[nodiscard]] RangeSpan acousticSpan(const Medium& medium, double lowHz, double highHz,
                                     const Conditions& conditions);

// Idem para a luz, entre dois comprimentos de onda de vacuo.
//
// Aqui a razao muda um pouquinho ao trocar de meio -- mas SO por causa da
// dispersao (n depende de lambda). Na agua o efeito e ~+1%: real, e muito longe
// de fechar o fator de 11.
[[nodiscard]] RangeSpan opticalSpan(const Medium& medium, double lowNm, double highNm);

}  // namespace soundwave
