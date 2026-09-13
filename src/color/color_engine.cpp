#include "soundwave/color/color_engine.hpp"

#include <algorithm>
#include <cmath>

namespace soundwave {

std::string_view spectrumModeName(SpectrumMode mode) {
    switch (mode) {
        case SpectrumMode::VisibleOnly:      return "visible-only";
        case SpectrumMode::VisibleWithBands: return "visible-with-bands";
        case SpectrumMode::FullEmSpectrum:   return "full-em-spectrum";
    }
    return "unknown";
}

bool parseSpectrumMode(std::string_view name, SpectrumMode& out) {
    if (name == "visible-only" || name == "visible_only") {
        out = SpectrumMode::VisibleOnly;
    } else if (name == "visible-with-bands" || name == "visible_with_bands") {
        out = SpectrumMode::VisibleWithBands;
    } else if (name == "full-em-spectrum" || name == "full_em_spectrum" || name == "full") {
        out = SpectrumMode::FullEmSpectrum;
    } else {
        return false;
    }
    return true;
}

ColorEngine::ColorEngine(ColorEngineSettings settings) : settings_(settings) {}

ColorResult ColorEngine::fromEmFrequency(double emFrequencyHz) const {
    return fromWavelength(wavelengthFromFrequency(emFrequencyHz));
}

ColorResult ColorEngine::fromWavelength(double wavelengthM) const {
    ColorResult result;
    result.wavelengthM = wavelengthM;
    result.emFrequencyHz = frequencyFromWavelength(wavelengthM);

    if (!std::isfinite(wavelengthM) || wavelengthM <= 0.0) {
        result.band = EmBand::Gamma;
        result.isFalseColour = true;
        return result;  // preto: entrada invalida nao ganha cor
    }

    result.band = classifyByWavelength(wavelengthM);
    const bool visible = result.band == EmBand::Visible;

    // Caminho da cor medida: so vale dentro do visivel e so quando o modo pede.
    if (visible && settings_.mode != SpectrumMode::FullEmSpectrum) {
        Xyz xyz = spectralXyz(wavelengthM);
        if (settings_.normaliseLuminance) {
            xyz = normaliseLuminance(xyz, settings_.targetLuminance);
        }
        const LinearRgb raw = xyzToLinearRgb(xyz);

        result.xyz = xyz;
        result.wasOutOfGamut = !isInGamut(raw);
        result.gamutExcursion = gamutExcursion(raw);
        result.linear = mapIntoGamut(raw, settings_.gamut);
        result.rgb = toRgb8(raw, settings_.gamut);
        result.isFalseColour = false;  // deriva das CMF: resposta visual medida
        return result;
    }

    if (settings_.mode == SpectrumMode::VisibleOnly) {
        // Fora do visivel nao existe cor. Preto e a representacao honesta:
        // qualquer outra coisa sugeriria uma resposta visual que nao existe.
        result.isFalseColour = true;
        return result;
    }

    // Pseudocor, com posicao dentro da banda para nao virar um bloco chapado.
    const double position = [&] {
        // Fracao logaritmica da banda: bandas EM cobrem varias ordens de
        // grandeza, entao posicao linear deixaria quase tudo em um extremo.
        switch (result.band) {
            case EmBand::Radio:       return std::log10(1e3 / std::min(wavelengthM, 1e3)) / 3.0;
            case EmBand::Microwave:   return std::log10(1.0 / wavelengthM) / 3.0;
            case EmBand::Infrared:    return std::log10(1e-3 / wavelengthM) / std::log10(1e-3 / 750e-9);
            case EmBand::Ultraviolet: return std::log10(400e-9 / wavelengthM) / std::log10(400e-9 / 10e-9);
            case EmBand::XRay:        return std::log10(10e-9 / wavelengthM) / 3.0;
            case EmBand::Gamma:       return std::log10(10e-12 / std::max(wavelengthM, 1e-20)) / 8.0;
            case EmBand::Visible:     return (750e-9 - wavelengthM) / (750e-9 - 400e-9);
        }
        return 0.5;
    }();

    const BandColour band = pseudoColourForBand(result.band, position);
    result.rgb = band.colour;
    // No modo FullEmSpectrum o visivel tambem recebe um rotulo de banda em vez
    // da sua cor medida -- e portanto e pseudocor, mesmo sendo visivel.
    // pseudoColourForBand nao sabe disso: ela so ve a banda, nao o modo.
    result.isFalseColour =
        band.isFalseColour || settings_.mode == SpectrumMode::FullEmSpectrum;
    result.linear = LinearRgb{decodeSrgb(band.colour.r / 255.0), decodeSrgb(band.colour.g / 255.0),
                              decodeSrgb(band.colour.b / 255.0)};
    result.xyz = linearRgbToXyz(result.linear);
    return result;
}

ColorResult ColorEngine::weightedMix(const std::vector<ColorResult>& colours,
                                     const std::vector<double>& weights) const {
    ColorResult result;
    if (colours.empty() || colours.size() != weights.size()) return result;

    double totalWeight = 0.0;
    Xyz sum;
    double emSum = 0.0;
    bool anyFalseColour = false;

    for (std::size_t i = 0; i < colours.size(); ++i) {
        const double w = weights[i];
        if (!std::isfinite(w) || w <= 0.0) continue;
        sum.x += colours[i].xyz.x * w;
        sum.y += colours[i].xyz.y * w;
        sum.z += colours[i].xyz.z * w;
        emSum += colours[i].emFrequencyHz * w;
        if (colours[i].isFalseColour) anyFalseColour = true;
        totalWeight += w;
    }

    if (totalWeight <= 0.0) return result;

    sum.x /= totalWeight;
    sum.y /= totalWeight;
    sum.z /= totalWeight;

    const LinearRgb raw = xyzToLinearRgb(sum);
    result.xyz = sum;
    result.linear = mapIntoGamut(raw, settings_.gamut);
    result.rgb = toRgb8(raw, settings_.gamut);
    result.wasOutOfGamut = !isInGamut(raw);
    result.gamutExcursion = gamutExcursion(raw);

    // A frequencia EM media ponderada e so um rotulo: a mistura em geral NAO e
    // monocromatica, entao nao existe um lambda unico que a produza.
    result.emFrequencyHz = emSum / totalWeight;
    result.wavelengthM = wavelengthFromFrequency(result.emFrequencyHz);
    result.band = classifyByWavelength(result.wavelengthM);
    // Uma mistura que inclua qualquer pseudocor e, como um todo, pseudocor.
    result.isFalseColour = anyFalseColour;
    return result;
}

}  // namespace soundwave
