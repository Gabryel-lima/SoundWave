#include "soundwave/physics/medium.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace soundwave {
namespace {

// =============================================================================
// DADOS E MODELOS EMPIRICOS
//
// Cada bloco cita a fonte e traz, em comentario, os valores de referencia usados
// para validar o port em tests/test_physics.cpp. Nenhuma constante aqui foi
// escrita de memoria.
// =============================================================================

// -----------------------------------------------------------------------------
// Absorcao optica da agua pura -- Segelstein (1981), tese de mestrado, Univ. of
// Missouri-Rolla: "The complex refractive index of water". Compilacao classica,
// amplamente citada, cobrindo 10 nm a 10 m.
//
// Convertido de k (parte imaginaria do indice) para alpha via alpha = 4*pi*k/lambda,
// em 1/m na base e.
//
// LIMITACAO IMPORTANTE, declarada e nao escondida:
// Na janela de transparencia da agua (380-500 nm) esta compilacao SUPERESTIMA a
// absorcao frente a medida moderna de Pope & Fry (1997), por um fator de ~3 a 8,
// e coloca o minimo em ~475 nm em vez de ~420 nm. A agua e tao transparente ali
// que a medida e experimentalmente dificil, e compilacoes antigas herdam erro.
//
// Por que ainda assim e utilizavel aqui: a conclusao que o projeto extrai destes
// dados -- "a agua e um passa-banda optico que mata o vermelho e deixa passar o
// azul" -- e ROBUSTA ao erro. Com Segelstein a razao alpha(700nm)/alpha(min) e
// ~32x; com Pope & Fry seria ~139x. O erro atenua a conclusao, nao a inverte.
// A reivindicacao de fidelidade correspondente declara a incerteza por faixa.
// -----------------------------------------------------------------------------
struct AbsorptionSample {
    double nm;
    double perMetre;  // alpha em 1/m, base e
};

constexpr AbsorptionSample kWaterAbsorption[] = {
    {374.9730, 6.807694e-02},
    {380.1894, 6.412096e-02},
    {384.5918, 6.011737e-02},
    {389.9420, 5.675446e-02},
    {395.3666, 5.284453e-02},
    {399.9447, 4.965923e-02},
    {404.5759, 4.623810e-02},
    {410.2041, 4.355119e-02},
    {414.9540, 4.055085e-02},
    {419.7590, 3.767038e-02},
    {424.6196, 3.459394e-02},
    {429.5364, 3.184198e-02},
    {434.5102, 2.944422e-02},
    {439.5416, 2.685344e-02},
    {444.6313, 2.454709e-02},
    {449.7798, 2.259436e-02},
    {454.9880, 2.152782e-02},
    {460.2566, 2.074914e-02},
    {464.5153, 2.027683e-02},
    {469.8941, 1.949845e-02},
    {475.3352, 1.853532e-02},
    {479.7334, 1.857805e-02},
    {485.2885, 1.853532e-02},
    {489.7788, 1.883649e-02},
    {495.4502, 1.990673e-02},
    {500.0345, 2.322737e-02},
    {504.6613, 2.685345e-02},
    {510.5050, 3.118890e-02},
    {515.2286, 3.564511e-02},
    {519.9960, 3.793150e-02},
    {524.8075, 3.926449e-02},
    {529.6635, 4.168694e-02},
    {534.5644, 4.436087e-02},
    {539.5106, 4.886524e-02},
    {544.5027, 5.236004e-02},
    {549.5409, 5.584702e-02},
    {554.6257, 6.025596e-02},
    {559.7576, 6.441693e-02},
    {564.9370, 6.966265e-02},
    {570.1643, 7.568329e-02},
    {575.4400, 8.394600e-02},
    {579.4287, 9.616123e-02},
    {584.7901, 1.122018e-01},
    {590.2011, 1.355189e-01},
    {595.6621, 1.629296e-01},
    {599.7911, 2.018366e-01},
    {605.3409, 2.349633e-01},
    {609.5369, 2.552701e-01},
    {615.1769, 2.716439e-01},
    {619.4411, 2.837919e-01},
    {625.1727, 2.958012e-01},
    {629.5062, 2.999162e-01},
    {635.3309, 3.069022e-01},
    {639.7348, 3.083188e-01},
    {645.6542, 3.126079e-01},
    {650.1297, 3.235936e-01},
    {654.6362, 3.411929e-01},
    {660.6935, 3.689776e-01},
    {665.2731, 3.837073e-01},
    {669.8846, 3.935501e-01},
    {674.5280, 4.055085e-01},
    {680.7694, 4.246196e-01},
    {685.4882, 4.528976e-01},
    {690.2398, 4.830588e-01},
    {695.0243, 5.357967e-01},
    {699.8420, 6.011738e-01},
    {704.6931, 7.311391e-01},
    {709.5778, 8.851156e-01},
    {714.4963, 1.054387e+00},
    {719.4490, 1.273503e+00},
    {724.4359, 1.584893e+00},
    {729.4575, 1.981527e+00},
    {734.5139, 2.306747e+00},
    {739.6052, 2.477422e+00},
    {744.7319, 2.582260e+00},
    {749.8942, 2.612161e+00},
    {755.0923, 2.630268e+00},
    {760.3263, 2.612161e+00},
    {765.5966, 2.576321e+00},
    {769.1304, 2.493778e+00},
    {774.4618, 2.398833e+00},
    {779.8301, 2.269865e+00},
    {785.2356, 2.142891e+00},
    {790.6787, 2.037042e+00},
    {794.3282, 1.990673e+00},
    {799.8343, 1.963360e+00},
    {805.3784, 1.981527e+00},
    {809.0959, 2.065380e+00},
};
constexpr std::size_t kWaterAbsorptionCount =
    sizeof(kWaterAbsorption) / sizeof(kWaterAbsorption[0]);

// Interpolacao log-linear: alpha varia por ordens de grandeza na faixa, entao
// interpolar linearmente em alpha subestimaria grosseiramente entre amostras.
double waterAbsorptionPerMetre(double nm) {
    if (!std::isfinite(nm)) return 0.0;
    const AbsorptionSample& first = kWaterAbsorption[0];
    const AbsorptionSample& last = kWaterAbsorption[kWaterAbsorptionCount - 1];
    if (nm <= first.nm) return first.perMetre;
    if (nm >= last.nm) return last.perMetre;

    std::size_t hi = 1;
    while (hi < kWaterAbsorptionCount && kWaterAbsorption[hi].nm < nm) ++hi;
    const AbsorptionSample& a = kWaterAbsorption[hi - 1];
    const AbsorptionSample& b = kWaterAbsorption[hi];

    const double t = (nm - a.nm) / (b.nm - a.nm);
    return std::exp(std::log(a.perMetre) + t * (std::log(b.perMetre) - std::log(a.perMetre)));
}

// -----------------------------------------------------------------------------
// Sellmeier. lambda em MICROMETROS nas duas formas.
//
//   forma 1 (Malitson): n^2 = 1 + sum Bi*L^2 / (L^2 - Ci^2)   <- Ci ao quadrado
//   forma 2 (Daimon):   n^2 = 1 + sum Bi*L^2 / (L^2 - Ci)     <- Ci ja quadrado
//
// Validacao: SiO2 n(587,6 nm) = 1,458462 contra o valor publicado da linha d,
// 1,45846 -- casa em todos os digitos dados.
// -----------------------------------------------------------------------------

// Malitson (1965), J. Opt. Soc. Am. 55(10), silica fundida, 0,21-6,7 um.
double sellmeierFusedSilica(double um) {
    const double L2 = um * um;
    const double s = 1.0
        + 0.6961663 * L2 / (L2 - 0.0684043 * 0.0684043)
        + 0.4079426 * L2 / (L2 - 0.1162414 * 0.1162414)
        + 0.8974794 * L2 / (L2 - 9.896161 * 9.896161);
    return s > 0.0 ? std::sqrt(s) : 1.0;
}

// Daimon & Masumura (2007), Appl. Opt. 46(18), agua a 20,0 C, 0,182-1,129 um.
//
// Em 589,3 nm devolve 1,333349. O valor classico/IAPWS a 20 C e 1,33299:
// diferenca de 0,027%, que e a divergencia real entre as duas fontes e nao um
// erro de port. Declarada na reivindicacao de fidelidade.
double sellmeierWater20C(double um) {
    const double L2 = um * um;
    const double s = 1.0
        + 0.5684027565 * L2 / (L2 - 0.005101829712)
        + 0.1726177391 * L2 / (L2 - 0.01821153936)
        + 0.02086189578 * L2 / (L2 - 0.02620722293)
        + 0.1130748688 * L2 / (L2 - 10.69792721);
    return s > 0.0 ? std::sqrt(s) : 1.0;
}

// -----------------------------------------------------------------------------
// Mackenzie (1981), J. Acoust. Soc. Am. 70(3): velocidade do som na agua.
// Valido 0-30 C, 25-40 ppt, 0-8000 m. Erro padrao declarado: 0,070 m/s (~0,005%).
//
// Validacao do port: (27 C, 35 ppt, 10 m) = 1539,0866 m/s.
// -----------------------------------------------------------------------------
double mackenzie1981(double T, double S, double D) {
    double c = 1448.96 + 4.591 * T - 5.304e-2 * T * T + 2.374e-4 * T * T * T;
    c += 1.340 * (S - 35.0) + 1.630e-2 * D + 1.675e-7 * D * D;
    c += -1.025e-2 * T * (S - 35.0) - 7.139e-13 * T * D * D * D;
    return c;
}

// -----------------------------------------------------------------------------
// Francois & Garrison (1982), J. Acoust. Soc. Am. 72(3) e 72(6): absorcao
// acustica na agua do mar, em dB/km. Tres termos: acido borico, sulfato de
// magnesio e viscosidade da agua pura.
//
// Validacao do port: 50 kHz a (27 C, 35 ppt, 10 m, pH 8,1) = 10,7103 dB/km.
// -----------------------------------------------------------------------------
double francoisGarrison1982DbPerKm(double frequencyHz, double T, double S, double D, double pH) {
    const double f = frequencyHz / 1000.0;  // kHz
    const double c = 1412.0 + 3.21 * T + 1.19 * S + 0.0167 * D;

    const double A1 = 8.86 / c * std::pow(10.0, 0.78 * pH - 5.0);
    const double f1 = 2.8 * std::sqrt(S / 35.0) * std::pow(10.0, 4.0 - 1245.0 / (T + 273.0));

    const double A2 = 21.44 * S / c * (1.0 + 0.025 * T);
    const double P2 = 1.0 - 1.37e-4 * D + 6.2e-9 * D * D;
    const double f2 = 8.17 * std::pow(10.0, 8.0 - 1990.0 / (T + 273.0)) / (1.0 + 0.0018 * (S - 35.0));

    const double P3 = 1.0 - 3.83e-5 * D + 4.9e-10 * D * D;
    const double A3 = (T < 20.0)
        ? 4.937e-4 - 2.59e-5 * T + 9.11e-7 * T * T - 1.5e-8 * T * T * T
        : 3.964e-4 - 1.146e-5 * T + 1.45e-7 * T * T - 6.5e-10 * T * T * T;

    return A1 * f1 * f * f / (f1 * f1 + f * f)
         + A2 * P2 * f2 * f * f / (f2 * f2 + f * f)
         + A3 * P3 * f * f;
}

// -----------------------------------------------------------------------------
// ISO 9613-1:1993: absorcao atmosferica, com relaxacao de oxigenio e nitrogenio.
//
// Validacao do port contra a tabela publicada em ISO 9613-2 (10 C, 70% UR,
// 101,325 kPa), em dB/km:
//   125 Hz  0,41 | 250  1,04 | 500  1,92 | 1k  3,66 | 2k  9,70 | 4k 33,06 | 8k 118,4
// -----------------------------------------------------------------------------
double iso9613AirDbPerMetre(double frequencyHz, double T_C, double rh, double p_kPa) {
    constexpr double kTref = 293.15;
    constexpr double kPref = 101.325;
    constexpr double kTripleT = 273.16;

    const double T = T_C + 273.15;
    if (T <= 0.0 || p_kPa <= 0.0) return 0.0;

    // Pressao de saturacao do vapor d'agua.
    const double psat = kPref * std::pow(10.0, -6.8346 * std::pow(kTripleT / T, 1.261) + 4.6151);
    const double h = rh * psat / p_kPa;  // concentracao molar de vapor, em %

    const double frO = p_kPa / kPref * (24.0 + 4.04e4 * h * (0.02 + h) / (0.391 + h));
    const double frN = p_kPa / kPref * std::pow(T / kTref, -0.5)
        * (9.0 + 280.0 * h * std::exp(-4.170 * (std::pow(T / kTref, -1.0 / 3.0) - 1.0)));

    const double f2 = frequencyHz * frequencyHz;
    const double classical = 1.84e-11 * (kPref / p_kPa) * std::sqrt(T / kTref);
    const double oxygen = 0.01275 * std::exp(-2239.1 / T) / (frO + f2 / frO);
    const double nitrogen = 0.1068 * std::exp(-3352.0 / T) / (frN + f2 / frN);

    return 8.686 * f2 * (classical + std::pow(T / kTref, -2.5) * (oxygen + nitrogen));
}

}  // namespace

// =============================================================================
// CATALOGO DE MEIOS
// =============================================================================
namespace media {

Medium vacuum() {
    Medium m;
    m.name = "vacuum";
    m.description = "Vacuo: opticamente perfeito, acusticamente impossivel.";
    m.soundSpeed = SoundSpeedModel::None;
    m.soundAbsorption = SoundAbsorptionModel::None;
    m.refractiveIndex = RefractiveIndexModel::Vacuum;
    m.opticalAbsorption = OpticalAbsorptionModel::Transparent;
    return m;
}

Medium air() {
    Medium m;
    m.name = "air";
    m.description = "Ar seco a nivel do mar. Meio de referencia para som.";
    m.soundSpeed = SoundSpeedModel::IdealGas;
    m.soundAbsorption = SoundAbsorptionModel::Iso9613Air;
    m.refractiveIndex = RefractiveIndexModel::ConstantAir;
    m.opticalAbsorption = OpticalAbsorptionModel::NegligibleAir;
    m.constantRefractiveIndex = 1.000273;
    return m;
}

Medium freshWater() {
    Medium m;
    m.name = "fresh-water";
    m.description = "Agua doce. Mackenzie com salinidade 0.";
    m.soundSpeed = SoundSpeedModel::Mackenzie1981;
    m.soundAbsorption = SoundAbsorptionModel::FrancoisGarrison1982;
    m.refractiveIndex = RefractiveIndexModel::SellmeierWater20C;
    m.opticalAbsorption = OpticalAbsorptionModel::Segelstein1981Water;
    return m;
}

Medium seaWater() {
    Medium m;
    m.name = "sea-water";
    m.description = "Agua do mar, 35 ppt. O meio onde os dois modelos sao mais confiaveis.";
    m.soundSpeed = SoundSpeedModel::Mackenzie1981;
    m.soundAbsorption = SoundAbsorptionModel::FrancoisGarrison1982;
    m.refractiveIndex = RefractiveIndexModel::SellmeierWater20C;
    m.opticalAbsorption = OpticalAbsorptionModel::Segelstein1981Water;
    return m;
}

Medium ice() {
    Medium m;
    m.name = "ice";
    m.description = "Gelo puro. Transparente a luz visivel -- ao contrario do que a intuicao diz.";
    m.soundSpeed = SoundSpeedModel::ConstantSolid;
    m.soundAbsorption = SoundAbsorptionModel::NotModelled;
    m.refractiveIndex = RefractiveIndexModel::ConstantIce;
    m.opticalAbsorption = OpticalAbsorptionModel::NotModelled;
    // Onda longitudinal em gelo policristalino a -16 C. Varia bastante com
    // temperatura, densidade e orientacao cristalina; por isso a reivindicacao
    // correspondente declara ~5% e nao mais.
    m.constantSoundSpeedMs = 3840.0;
    m.constantRefractiveIndex = 1.31;
    return m;
}

Medium fusedSilica() {
    Medium m;
    m.name = "fused-silica";
    m.description = "Silica fundida (vidro optico). Dispersao muito bem caracterizada.";
    m.soundSpeed = SoundSpeedModel::ConstantSolid;
    m.soundAbsorption = SoundAbsorptionModel::NotModelled;
    m.refractiveIndex = RefractiveIndexModel::SellmeierFusedSilica;
    m.opticalAbsorption = OpticalAbsorptionModel::NotModelled;
    m.constantSoundSpeedMs = 5968.0;  // onda longitudinal
    return m;
}

std::vector<Medium> all() {
    return {vacuum(), air(), freshWater(), seaWater(), ice(), fusedSilica()};
}

std::optional<Medium> byName(std::string_view name) {
    for (const Medium& m : all()) {
        if (m.name == name) return m;
    }
    return std::nullopt;
}

}  // namespace media

// =============================================================================
// PROPRIEDADES ACUSTICAS
// =============================================================================

double soundSpeedMs(const Medium& medium, const Conditions& conditions) {
    switch (medium.soundSpeed) {
        case SoundSpeedModel::None:
            return 0.0;
        case SoundSpeedModel::IdealGas:
            // c = c0 * sqrt(T/T0) para gas ideal. 331.3 m/s a 0 C.
            return 331.3 * std::sqrt(1.0 + conditions.temperatureC / 273.15);
        case SoundSpeedModel::Mackenzie1981: {
            const double salinity =
                medium.name == "fresh-water" ? 0.0 : conditions.salinityPpt;
            return mackenzie1981(conditions.temperatureC, salinity, conditions.depthM);
        }
        case SoundSpeedModel::ConstantSolid:
            return medium.constantSoundSpeedMs;
    }
    return 0.0;
}

double soundAbsorptionDbPerMetre(const Medium& medium, double frequencyHz,
                                 const Conditions& conditions) {
    if (!(frequencyHz > 0.0) || !std::isfinite(frequencyHz)) return 0.0;
    switch (medium.soundAbsorption) {
        case SoundAbsorptionModel::None:
        case SoundAbsorptionModel::NotModelled:
            return 0.0;
        case SoundAbsorptionModel::Iso9613Air:
            return iso9613AirDbPerMetre(frequencyHz, conditions.temperatureC,
                                        conditions.relativeHumidity, conditions.pressureKPa);
        case SoundAbsorptionModel::FrancoisGarrison1982: {
            const double salinity =
                medium.name == "fresh-water" ? 0.0 : conditions.salinityPpt;
            return francoisGarrison1982DbPerKm(frequencyHz, conditions.temperatureC, salinity,
                                               conditions.depthM, conditions.pH) / 1000.0;
        }
    }
    return 0.0;
}

double soundWavelengthM(const Medium& medium, double frequencyHz, const Conditions& conditions) {
    if (!(frequencyHz > 0.0) || !std::isfinite(frequencyHz)) return 0.0;
    const double v = soundSpeedMs(medium, conditions);
    return v > 0.0 ? v / frequencyHz : 0.0;
}

// =============================================================================
// PROPRIEDADES OPTICAS
// =============================================================================

double refractiveIndexAt(const Medium& medium, double vacuumWavelengthNm) {
    if (!std::isfinite(vacuumWavelengthNm) || vacuumWavelengthNm <= 0.0) return 1.0;
    const double um = vacuumWavelengthNm / 1000.0;
    switch (medium.refractiveIndex) {
        case RefractiveIndexModel::Vacuum:
            return 1.0;
        case RefractiveIndexModel::ConstantAir:
        case RefractiveIndexModel::ConstantIce:
            return medium.constantRefractiveIndex;
        case RefractiveIndexModel::SellmeierWater20C:
            // Fora da faixa de validade (0,182-1,129 um) prendemos na borda em
            // vez de extrapolar: um polo de Sellmeier fora de faixa produz
            // numeros sem sentido, as vezes n < 1 ou negativo.
            return sellmeierWater20C(std::clamp(um, 0.182, 1.129));
        case RefractiveIndexModel::SellmeierFusedSilica:
            return sellmeierFusedSilica(std::clamp(um, 0.21, 6.7));
    }
    return 1.0;
}

double opticalAbsorptionDbPerMetre(const Medium& medium, double vacuumWavelengthNm) {
    switch (medium.opticalAbsorption) {
        case OpticalAbsorptionModel::Transparent:
        case OpticalAbsorptionModel::NegligibleAir:
        case OpticalAbsorptionModel::NotModelled:
            return 0.0;
        case OpticalAbsorptionModel::Segelstein1981Water: {
            const double perMetre = waterAbsorptionPerMetre(vacuumWavelengthNm);
            // Napier -> decibel: 1 Np = 20/ln(10) dB = 8,6859 dB.
            return perMetre * 20.0 / std::log(10.0);
        }
    }
    return 0.0;
}

double lightWavelengthInMediumM(const Medium& medium, double vacuumWavelengthNm) {
    if (!std::isfinite(vacuumWavelengthNm) || vacuumWavelengthNm <= 0.0) return 0.0;
    const double n = refractiveIndexAt(medium, vacuumWavelengthNm);
    return n > 0.0 ? (vacuumWavelengthNm * 1e-9) / n : 0.0;
}

std::optional<double> vacuumWavelengthFromMediumNm(const Medium& medium,
                                                   double mediumWavelengthNm) {
    if (!std::isfinite(mediumWavelengthNm) || mediumWavelengthNm <= 0.0) return std::nullopt;

    // lambda_vac = n(lambda_vac) * lambda_meio. n depende do proprio resultado,
    // entao iteramos. A dispersao e fraca (dn/dlambda pequeno), logo a iteracao
    // de ponto fixo contrai rapidamente -- converge em poucas voltas.
    double guess = mediumWavelengthNm * refractiveIndexAt(medium, mediumWavelengthNm);
    for (int iteration = 0; iteration < 50; ++iteration) {
        const double next = mediumWavelengthNm * refractiveIndexAt(medium, guess);
        if (std::abs(next - guess) < 1e-9) return next;
        guess = next;
    }
    return std::nullopt;  // nao convergiu: melhor nada que um numero errado
}

// =============================================================================
// A DEMONSTRACAO DA INVARIANCIA
// =============================================================================

RangeSpan acousticSpan(const Medium& medium, double lowHz, double highHz,
                       const Conditions& conditions) {
    RangeSpan span;
    if (!(lowHz > 0.0) || !(highHz > lowHz)) return span;

    span.atLowFrequencyM = soundWavelengthM(medium, lowHz, conditions);
    span.atHighFrequencyM = soundWavelengthM(medium, highHz, conditions);
    if (span.atHighFrequencyM > 0.0) {
        span.ratio = span.atLowFrequencyM / span.atHighFrequencyM;
        span.octaves = std::log2(span.ratio);
    }
    return span;
}

RangeSpan opticalSpan(const Medium& medium, double lowNm, double highNm) {
    RangeSpan span;
    if (!(lowNm > 0.0) || !(highNm > lowNm)) return span;

    // Convencao: "baixa frequencia" = comprimento de onda longo, para casar com
    // a orientacao de acousticSpan.
    span.atLowFrequencyM = lightWavelengthInMediumM(medium, highNm);
    span.atHighFrequencyM = lightWavelengthInMediumM(medium, lowNm);
    if (span.atHighFrequencyM > 0.0) {
        span.ratio = span.atLowFrequencyM / span.atHighFrequencyM;
        span.octaves = std::log2(span.ratio);
    }
    return span;
}

// =============================================================================
// REIVINDICACOES DE FIDELIDADE
// =============================================================================

FidelityClaim soundSpeedClaim(const Medium& medium) {
    FidelityClaim claim;
    claim.quantity = "velocidade do som (" + medium.name + ")";
    switch (medium.soundSpeed) {
        case SoundSpeedModel::None:
            claim.tier = Fidelity::Exact;
            claim.relativeUncertainty = 0.0;
            claim.source = "o vacuo nao propaga onda mecanica";
            claim.caveat = "nao ha som a modelar";
            break;
        case SoundSpeedModel::IdealGas:
            claim.tier = Fidelity::ModelFit;
            // ~0,1%: o gas ideal ignora umidade e composicao, que deslocam c em
            // ate ~0,3% em ar muito umido.
            claim.relativeUncertainty = 1e-3;
            claim.source = "aproximacao de gas ideal, c = 331,3*sqrt(1 + T/273,15)";
            claim.validity = "-20 a 50 C, pressao proxima de 1 atm, ar seco";
            claim.caveat = "ignora umidade e composicao; ar saturado desloca c em ate ~0,3%";
            break;
        case SoundSpeedModel::Mackenzie1981:
            claim.tier = Fidelity::ModelFit;
            // Erro padrao declarado de 0,070 m/s sobre ~1500 m/s.
            claim.relativeUncertainty = 0.070 / 1500.0;
            claim.source = "Mackenzie (1981), J. Acoust. Soc. Am. 70(3), 807-812";
            claim.validity = "0-30 C, 25-40 ppt, 0-8000 m";
            claim.caveat = "com salinidade 0 (agua doce) esta fora da faixa ajustada; "
                           "trate o valor como indicativo, com erro possivelmente maior";
            break;
        case SoundSpeedModel::ConstantSolid:
            claim.tier = Fidelity::Measured;
            claim.relativeUncertainty = 0.05;
            claim.source = "valor tabelado unico para onda longitudinal";
            claim.validity = "temperatura e densidade nominais";
            claim.caveat = "solidos variam muito com temperatura, porosidade e orientacao "
                           "cristalina; nao ha dependencia de temperatura modelada aqui";
            break;
    }
    return claim;
}

FidelityClaim soundAbsorptionClaim(const Medium& medium) {
    FidelityClaim claim;
    claim.quantity = "absorcao acustica (" + medium.name + ")";
    switch (medium.soundAbsorption) {
        case SoundAbsorptionModel::None:
            claim.tier = Fidelity::Exact;
            claim.relativeUncertainty = 0.0;
            claim.source = "sem som, sem absorcao";
            break;
        case SoundAbsorptionModel::NotModelled:
            claim.tier = Fidelity::Arbitrary;
            claim.source = "NAO MODELADO -- devolve zero";
            claim.caveat = "zero aqui significa 'nao sabemos', nao 'nao ha atenuacao'. "
                           "Solidos reais atenuam, e fortemente. Nao use este meio para "
                           "conclusoes sobre propagacao.";
            break;
        case SoundAbsorptionModel::Iso9613Air:
            claim.tier = Fidelity::ModelFit;
            claim.relativeUncertainty = 0.10;
            claim.source = "ISO 9613-1:1993";
            claim.validity = "-20 a 50 C, 0-100% UR, 50 Hz-10 MHz, proximo de 1 atm";
            claim.caveat = "exatidao declarada de +/-10% dentro da faixa; fora dela "
                           "o desvio cresce rapido";
            break;
        case SoundAbsorptionModel::FrancoisGarrison1982:
            claim.tier = Fidelity::ModelFit;
            claim.relativeUncertainty = 0.05;
            claim.source = "Francois & Garrison (1982), J. Acoust. Soc. Am. 72(3) e 72(6)";
            claim.validity = "-2 a 22 C, 30-35 ppt, 0-3500 m, 400 Hz-1 MHz";
            claim.caveat = "abaixo de ~400 Hz a absorcao e minuscula e o ajuste e "
                           "extrapolacao; em agua doce o termo de sulfato de magnesio "
                           "desaparece e o modelo perde parte do seu dominio";
            break;
    }
    return claim;
}

FidelityClaim refractiveIndexClaim(const Medium& medium) {
    FidelityClaim claim;
    claim.quantity = "indice de refracao (" + medium.name + ")";
    switch (medium.refractiveIndex) {
        case RefractiveIndexModel::Vacuum:
            claim.tier = Fidelity::Exact;
            claim.relativeUncertainty = 0.0;
            claim.source = "n = 1 por definicao";
            break;
        case RefractiveIndexModel::ConstantAir:
            claim.tier = Fidelity::Measured;
            claim.relativeUncertainty = 1e-5;
            claim.source = "n ~ 1,000273 em condicoes normais";
            claim.validity = "visivel, 1 atm, 15 C, ar seco";
            claim.caveat = "dispersao do ar desprezada: varia ~3e-6 no visivel";
            break;
        case RefractiveIndexModel::SellmeierWater20C:
            claim.tier = Fidelity::ModelFit;
            // Divergencia medida entre Daimon e o valor classico/IAPWS a 20 C.
            claim.relativeUncertainty = 2.7e-4;
            claim.source = "Daimon & Masumura (2007), Appl. Opt. 46(18), 3811 -- agua a 20,0 C";
            claim.validity = "182-1129 nm, 20 C";
            claim.caveat = "em 589,3 nm devolve 1,333349 contra 1,33299 do valor classico "
                           "IAPWS: divergencia real de 0,027% entre fontes. Sem dependencia "
                           "de temperatura: dn/dT ~ -9e-5 por grau";
            break;
        case RefractiveIndexModel::SellmeierFusedSilica:
            claim.tier = Fidelity::ModelFit;
            claim.relativeUncertainty = 1e-5;
            claim.source = "Malitson (1965), J. Opt. Soc. Am. 55(10), 1205";
            claim.validity = "210-6700 nm, 20 C";
            claim.caveat = "reproduz a linha d publicada (1,45846) em todos os digitos dados";
            break;
        case RefractiveIndexModel::ConstantIce:
            claim.tier = Fidelity::Measured;
            claim.relativeUncertainty = 0.01;
            claim.source = "n ~ 1,31 para gelo Ih no visivel";
            claim.validity = "visivel, proximo de 0 C";
            claim.caveat = "sem dispersao nem dependencia de temperatura modeladas";
            break;
    }
    return claim;
}

FidelityClaim opticalAbsorptionClaim(const Medium& medium, double vacuumWavelengthNm) {
    FidelityClaim claim;
    claim.quantity = "absorcao optica (" + medium.name + ")";
    switch (medium.opticalAbsorption) {
        case OpticalAbsorptionModel::Transparent:
            claim.tier = Fidelity::Exact;
            claim.relativeUncertainty = 0.0;
            claim.source = "vacuo nao absorve";
            break;
        case OpticalAbsorptionModel::NegligibleAir:
            claim.tier = Fidelity::ModelFit;
            claim.relativeUncertainty = 1.0;
            claim.source = "tratada como zero";
            claim.caveat = "ar limpo absorve pouquissimo no visivel, mas ESPALHA "
                           "(Rayleigh, ~lambda^-4) -- e por isso que o ceu e azul. "
                           "Espalhamento nao e modelado aqui";
            break;
        case OpticalAbsorptionModel::NotModelled:
            claim.tier = Fidelity::Arbitrary;
            claim.source = "NAO MODELADO -- devolve zero";
            claim.caveat = "zero significa 'nao sabemos', nao 'transparente'";
            break;
        case OpticalAbsorptionModel::Segelstein1981Water:
            claim.tier = Fidelity::ModelFit;
            // Incerteza declarada POR FAIXA: na janela de transparencia esta
            // compilacao e conhecidamente ruim, e esconder isso atras de um
            // numero unico seria o oposto do que este modulo existe para fazer.
            if (vacuumWavelengthNm < 500.0) {
                claim.relativeUncertainty = 5.0;  // ate ~8x
                claim.caveat = "NA JANELA DE TRANSPARENCIA (380-500 nm) esta compilacao "
                               "superestima a absorcao em ~3 a 8 vezes frente a Pope & Fry "
                               "(1997), e poe o minimo em ~475 nm em vez de ~420 nm. Use "
                               "apenas como ordem de grandeza. A conclusao qualitativa "
                               "(agua mata o vermelho, passa o azul) sobrevive: o erro "
                               "atenua o contraste, nao o inverte";
            } else if (vacuumWavelengthNm <= 700.0) {
                claim.relativeUncertainty = 0.10;
                claim.caveat = "concorda com Pope & Fry (1997) dentro de ~10% nesta faixa";
            } else {
                claim.relativeUncertainty = 0.20;
                claim.caveat = "regiao fortemente absorvente; concordancia razoavel entre fontes";
            }
            claim.source = "Segelstein (1981), tese, Univ. of Missouri-Rolla";
            claim.validity = "375-810 nm neste projeto (a compilacao original vai de 10 nm a 10 m)";
            break;
    }
    return claim;
}

}  // namespace soundwave
