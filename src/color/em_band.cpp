#include "soundwave/color/em_band.hpp"

#include <algorithm>
#include <cmath>

#include "soundwave/color/spectral.hpp"
#include "soundwave/core/constants.hpp"

namespace soundwave {
namespace {

Rgb mix(Rgb a, Rgb b, double t) {
    // Interpolamos em luz linear e re-codificamos: interpolar diretamente os
    // bytes sRGB produz um meio-termo mais escuro que o correto.
    const double clamped = std::clamp(t, 0.0, 1.0);
    LinearRgb la{decodeSrgb(a.r / 255.0), decodeSrgb(a.g / 255.0), decodeSrgb(a.b / 255.0)};
    LinearRgb lb{decodeSrgb(b.r / 255.0), decodeSrgb(b.g / 255.0), decodeSrgb(b.b / 255.0)};
    return toRgb8(blendLinear(la, lb, clamped), GamutStrategy::Clip);
}

}  // namespace

std::string_view emBandName(EmBand band) {
    switch (band) {
        case EmBand::Radio:       return "radio";
        case EmBand::Microwave:   return "microwave";
        case EmBand::Infrared:    return "infrared";
        case EmBand::Visible:     return "visible";
        case EmBand::Ultraviolet: return "ultraviolet";
        case EmBand::XRay:        return "x-ray";
        case EmBand::Gamma:       return "gamma";
    }
    return "unknown";
}

EmBand classifyByWavelength(double wavelengthM) {
    if (!std::isfinite(wavelengthM) || wavelengthM <= 0.0) return EmBand::Gamma;
    if (wavelengthM > 1.0) return EmBand::Radio;                    // > 1 m
    if (wavelengthM > 1e-3) return EmBand::Microwave;               // 1 mm .. 1 m
    if (wavelengthM > kVisibleMaxWavelengthM) return EmBand::Infrared;      // 750 nm .. 1 mm
    if (wavelengthM >= kVisibleMinWavelengthM) return EmBand::Visible;      // 400 .. 750 nm
    if (wavelengthM > 10e-9) return EmBand::Ultraviolet;            // 10 .. 400 nm
    if (wavelengthM > 10e-12) return EmBand::XRay;                  // 10 pm .. 10 nm
    return EmBand::Gamma;
}

EmBand classifyByFrequency(double frequencyHz) {
    return classifyByWavelength(wavelengthFromFrequency(frequencyHz));
}

BandColour pseudoColourForBand(EmBand band) { return pseudoColourForBand(band, 0.5); }

BandColour pseudoColourForBand(EmBand band, double position) {
    BandColour result;
    result.band = band;
    result.isFalseColour = true;

    const double t = std::clamp(position, 0.0, 1.0);

    // Paleta escolhida para (a) distinguir bandas a primeira vista e (b) nao se
    // confundir com cores espectrais reais: as bandas nao visiveis usam tons
    // dessaturados ou metalicos, longe das cores saturadas do locus espectral.
    switch (band) {
        case EmBand::Radio:
            result.colour = mix(Rgb{40, 26, 20}, Rgb{102, 66, 48}, t);     // marrom escuro
            break;
        case EmBand::Microwave:
            result.colour = mix(Rgb{102, 66, 48}, Rgb{150, 98, 54}, t);    // bronze
            break;
        case EmBand::Infrared:
            result.colour = mix(Rgb{120, 40, 32}, Rgb{190, 74, 48}, t);    // terracota
            break;
        case EmBand::Visible:
            // Nao ha pseudocor legitima aqui: o visivel tem cor medida.
            result.colour = Rgb{128, 128, 128};
            result.isFalseColour = false;
            break;
        case EmBand::Ultraviolet:
            result.colour = mix(Rgb{110, 90, 190}, Rgb{170, 160, 230}, t); // lilas
            break;
        case EmBand::XRay:
            result.colour = mix(Rgb{150, 190, 205}, Rgb{200, 225, 235}, t); // ciano palido
            break;
        case EmBand::Gamma:
            result.colour = mix(Rgb{215, 230, 220}, Rgb{250, 250, 250}, t); // quase branco
            break;
    }
    return result;
}

}  // namespace soundwave
