#include "soundwave/color/spectral.hpp"

#include <cmath>

#include "soundwave/color/cie_tables.hpp"
#include "soundwave/core/constants.hpp"

namespace soundwave {
namespace {

// Gaussiana por partes: largura distinta de cada lado do pico. E o que permite
// a poucos lobos reproduzirem funcoes visivelmente assimetricas como x_barra.
double piecewiseGaussian(double x, double mu, double sigmaLeft, double sigmaRight) {
    const double sigma = (x < mu) ? sigmaLeft : sigmaRight;
    if (sigma <= 0.0) return 0.0;
    const double t = (x - mu) / sigma;
    return std::exp(-0.5 * t * t);
}

}  // namespace

double wavelengthFromFrequency(double frequencyHz) {
    if (!(frequencyHz > 0.0) || !std::isfinite(frequencyHz)) return 0.0;
    return kSpeedOfLight / frequencyHz;
}

double frequencyFromWavelength(double wavelengthM) {
    if (!(wavelengthM > 0.0) || !std::isfinite(wavelengthM)) return 0.0;
    return kSpeedOfLight / wavelengthM;
}

double metresToNanometres(double metres) { return metres / kNanometre; }
double nanometresToMetres(double nanometres) { return nanometres * kNanometre; }

bool isVisibleWavelength(double wavelengthM) {
    return wavelengthM >= kVisibleMinWavelengthM && wavelengthM <= kVisibleMaxWavelengthM;
}

// Fonte primaria: tabela CIE oficial.
double cieX(double lambda) { return cie::lookup(lambda).x; }
double cieY(double lambda) { return cie::lookup(lambda).y; }
double cieZ(double lambda) { return cie::lookup(lambda).z; }

// Coeficientes de Wyman, Sloan & Shirley (2013). Mantidos apenas para os testes
// que documentam a divergencia nas bordas -- ver cie_tables.cpp.
double cieXAnalyticFit(double lambda) {
    return 1.056 * piecewiseGaussian(lambda, 599.8, 37.9, 31.0) +
           0.362 * piecewiseGaussian(lambda, 442.0, 16.0, 26.7) -
           0.065 * piecewiseGaussian(lambda, 501.1, 20.4, 26.2);
}

double cieYAnalyticFit(double lambda) {
    return 0.821 * piecewiseGaussian(lambda, 568.8, 46.9, 40.5) +
           0.286 * piecewiseGaussian(lambda, 530.9, 16.3, 31.1);
}

double cieZAnalyticFit(double lambda) {
    return 1.217 * piecewiseGaussian(lambda, 437.0, 11.8, 36.0) +
           0.681 * piecewiseGaussian(lambda, 459.0, 26.0, 13.8);
}

Xyz spectralXyz(double wavelengthM) {
    if (!isVisibleWavelength(wavelengthM)) return Xyz{0.0, 0.0, 0.0};
    return cie::lookup(metresToNanometres(wavelengthM));
}

Xyz whitePointD65() {
    // Valores tabelados do iluminante D65 para o observador de 2 graus,
    // normalizados para Y = 1.
    return Xyz{0.95047, 1.0, 1.08883};
}

Xyz normaliseLuminance(const Xyz& colour, double targetLuminance) {
    if (!(colour.y > 0.0)) return colour;
    const double scale = targetLuminance / colour.y;
    return Xyz{colour.x * scale, colour.y * scale, colour.z * scale};
}

}  // namespace soundwave
