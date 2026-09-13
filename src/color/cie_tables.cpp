#include "soundwave/color/cie_tables.hpp"

#include <algorithm>
#include <cmath>

namespace soundwave::cie {
namespace {

// -----------------------------------------------------------------------------
// Funcoes de correspondencia de cor CIE 1931, observador padrao de 2 graus.
// Tabela oficial, 380 a 780 nm em passos de 5 nm.
//
// POR QUE A TABELA E NAO UM AJUSTE ANALITICO
//
// A primeira versao deste modulo usava o ajuste multi-lobo de gaussianas de
// Wyman, Sloan & Shirley (2013), justificado por ter "erro abaixo de ~1% do
// pico". Esse raciocinio estava errado, e de um jeito instrutivo.
//
// O erro de 1% e ABSOLUTO. A cor, porem, e determinada pelas RAZOES entre
// x_barra, y_barra e z_barra -- e acima de ~700 nm as tres funcoes valem menos
// de 1e-3, entao um erro absoluto minusculo vira um erro relativo enorme. Na
// pratica, as caudas gaussianas de x_barra e y_barra decaem em ritmos
// diferentes e se CRUZAM perto de 712 nm. Consequencia: o ajuste dava
// x_barra/y_barra = 0,23 em 750 nm, quando o valor real e 2,77 -- constante em
// todo o vermelho profundo. O resultado visivel era 736 nm renderizado como
// VERDE.
//
// A normalizacao de luminancia agrava o problema em vez de escondê-lo: ela
// multiplica o triestimulo por 1/Y, que nas bordas do visivel vale milhares.
//
// Licao geral que vale alem deste arquivo: "erro pequeno" so significa alguma
// coisa em relacao a grandeza que se vai usar. Aqui o que importa e a razao, e
// nao o valor -- entao a tolerancia tinha de ser relativa desde o inicio.
//
// Fonte dos valores: tabela CIE 1931 2 graus, conferida contra tres ancoras
// independentes -- y_barra(555) = 1,00000 (pico da eficiencia luminosa
// fotopica), x_barra(600) = 1,06220 e z_barra(445) = 1,78260.
// -----------------------------------------------------------------------------
struct Entry {
    double nm;
    double x;
    double y;
    double z;
};

constexpr Entry kTable[] = {
    {380.0, 0.001368, 0.000039, 0.006450},
    {385.0, 0.002236, 0.000064, 0.010550},
    {390.0, 0.004243, 0.000120, 0.020050},
    {395.0, 0.007650, 0.000217, 0.036210},
    {400.0, 0.014310, 0.000396, 0.067850},
    {405.0, 0.023190, 0.000640, 0.110200},
    {410.0, 0.043510, 0.001210, 0.207400},
    {415.0, 0.077630, 0.002180, 0.371300},
    {420.0, 0.134380, 0.004000, 0.645600},
    {425.0, 0.214770, 0.007300, 1.039050},
    {430.0, 0.283900, 0.011600, 1.385600},
    {435.0, 0.328500, 0.016840, 1.622960},
    {440.0, 0.348280, 0.023000, 1.747060},
    {445.0, 0.348060, 0.029800, 1.782600},
    {450.0, 0.336200, 0.038000, 1.772110},
    {455.0, 0.318700, 0.048000, 1.744100},
    {460.0, 0.290800, 0.060000, 1.669200},
    {465.0, 0.251100, 0.073900, 1.528100},
    {470.0, 0.195360, 0.090980, 1.287640},
    {475.0, 0.142100, 0.112600, 1.041900},
    {480.0, 0.095640, 0.139020, 0.812950},
    {485.0, 0.057950, 0.169300, 0.616200},
    {490.0, 0.032010, 0.208020, 0.465180},
    {495.0, 0.014700, 0.258600, 0.353300},
    {500.0, 0.004900, 0.323000, 0.272000},
    {505.0, 0.002400, 0.407300, 0.212300},
    {510.0, 0.009300, 0.503000, 0.158200},
    {515.0, 0.029100, 0.608200, 0.111700},
    {520.0, 0.063270, 0.710000, 0.078250},
    {525.0, 0.109600, 0.793200, 0.057250},
    {530.0, 0.165500, 0.862000, 0.042160},
    {535.0, 0.225750, 0.914850, 0.029840},
    {540.0, 0.290400, 0.954000, 0.020300},
    {545.0, 0.359700, 0.980300, 0.013400},
    {550.0, 0.433450, 0.994950, 0.008750},
    {555.0, 0.512050, 1.000000, 0.005750},
    {560.0, 0.594500, 0.995000, 0.003900},
    {565.0, 0.678400, 0.978600, 0.002750},
    {570.0, 0.762100, 0.952000, 0.002100},
    {575.0, 0.842500, 0.915400, 0.001800},
    {580.0, 0.916300, 0.870000, 0.001650},
    {585.0, 0.978600, 0.816300, 0.001400},
    {590.0, 1.026300, 0.757000, 0.001100},
    {595.0, 1.056700, 0.694900, 0.001000},
    {600.0, 1.062200, 0.631000, 0.000800},
    {605.0, 1.045600, 0.566800, 0.000600},
    {610.0, 1.002600, 0.503000, 0.000340},
    {615.0, 0.938400, 0.441200, 0.000240},
    {620.0, 0.854450, 0.381000, 0.000190},
    {625.0, 0.751400, 0.321000, 0.000100},
    {630.0, 0.642400, 0.265000, 0.000050},
    {635.0, 0.541900, 0.217000, 0.000030},
    {640.0, 0.447900, 0.175000, 0.000020},
    {645.0, 0.360800, 0.138200, 0.000010},
    {650.0, 0.283500, 0.107000, 0.000000},
    {655.0, 0.218700, 0.081600, 0.000000},
    {660.0, 0.164900, 0.061000, 0.000000},
    {665.0, 0.121200, 0.044580, 0.000000},
    {670.0, 0.087400, 0.032000, 0.000000},
    {675.0, 0.063600, 0.023200, 0.000000},
    {680.0, 0.046770, 0.017000, 0.000000},
    {685.0, 0.032900, 0.011920, 0.000000},
    {690.0, 0.022700, 0.008210, 0.000000},
    {695.0, 0.015840, 0.005723, 0.000000},
    {700.0, 0.011359, 0.004102, 0.000000},
    {705.0, 0.008111, 0.002929, 0.000000},
    {710.0, 0.005790, 0.002091, 0.000000},
    {715.0, 0.004109, 0.001484, 0.000000},
    {720.0, 0.002899, 0.001047, 0.000000},
    {725.0, 0.002049, 0.000740, 0.000000},
    {730.0, 0.001440, 0.000520, 0.000000},
    {735.0, 0.001000, 0.000361, 0.000000},
    {740.0, 0.000690, 0.000249, 0.000000},
    {745.0, 0.000476, 0.000172, 0.000000},
    {750.0, 0.000332, 0.000120, 0.000000},
    {755.0, 0.000235, 0.000085, 0.000000},
    {760.0, 0.000166, 0.000060, 0.000000},
    {765.0, 0.000117, 0.000042, 0.000000},
    {770.0, 0.000083, 0.000030, 0.000000},
    {775.0, 0.000059, 0.000021, 0.000000},
    {780.0, 0.000042, 0.000015, 0.000000},};

constexpr std::size_t kCount = sizeof(kTable) / sizeof(kTable[0]);
constexpr double kFirstNm = kTable[0].nm;
constexpr double kLastNm = kTable[kCount - 1].nm;
constexpr double kStepNm = 5.0;

}  // namespace

double firstWavelengthNm() { return kFirstNm; }
double lastWavelengthNm() { return kLastNm; }
std::size_t sampleCount() { return kCount; }

Xyz lookup(double wavelengthNm) {
    if (!std::isfinite(wavelengthNm) || wavelengthNm < kFirstNm || wavelengthNm > kLastNm) {
        return Xyz{0.0, 0.0, 0.0};
    }

    // Interpolacao linear entre amostras de 5 nm. E o que a propria CIE
    // recomenda para uso geral; as funcoes sao suaves nessa escala e o erro de
    // interpolacao fica muito abaixo da variacao entre observadores humanos.
    const double position = (wavelengthNm - kFirstNm) / kStepNm;
    const auto index = static_cast<std::size_t>(position);
    if (index + 1 >= kCount) {
        const Entry& last = kTable[kCount - 1];
        return Xyz{last.x, last.y, last.z};
    }

    const Entry& low = kTable[index];
    const Entry& high = kTable[index + 1];
    const double t = position - static_cast<double>(index);

    return Xyz{low.x + (high.x - low.x) * t, low.y + (high.y - low.y) * t,
               low.z + (high.z - low.z) * t};
}

}  // namespace soundwave::cie
