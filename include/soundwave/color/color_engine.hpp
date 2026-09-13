#pragma once

#include <string_view>
#include <vector>

#include "soundwave/color/em_band.hpp"
#include "soundwave/color/spectral.hpp"
#include "soundwave/color/srgb.hpp"

namespace soundwave {

// O que fazer quando f_EM cai fora da faixa visivel (plano, secao 7).
enum class SpectrumMode {
    VisibleOnly,      // fora do visivel -> preto. Honesto e legivel.
    VisibleWithBands, // visivel em cor medida; fora dele, pseudocor de banda.
    FullEmSpectrum,   // pseudocor em todas as bandas, inclusive no visivel.
};

[[nodiscard]] std::string_view spectrumModeName(SpectrumMode mode);
[[nodiscard]] bool parseSpectrumMode(std::string_view name, SpectrumMode& out);

struct ColorEngineSettings {
    SpectrumMode mode = SpectrumMode::VisibleWithBands;
    GamutStrategy gamut = GamutStrategy::Desaturate;

    // Reescala toda cor espectral para a mesma luminancia. Ver
    // spectral.hpp::normaliseLuminance -- e escolha de visualizacao, nao fisica.
    bool normaliseLuminance = true;
    double targetLuminance = 1.0;
};

// Resultado de uma conversao, com a procedencia junto. O tipo carrega a
// procedencia de proposito: quem consome nao consegue esquecer de checar se a
// cor e uma aproximacao de resposta visual medida ou um rotulo inventado.
struct ColorResult {
    Rgb rgb;                    // sRGB codificado, pronto para exibir
    LinearRgb linear;           // luz linear, antes da codificacao -- use para misturar
    Xyz xyz;                    // triestimulo, antes do gamut
    double emFrequencyHz = 0.0;
    double wavelengthM = 0.0;
    EmBand band = EmBand::Visible;

    // false => a cor deriva das funcoes de correspondencia CIE (resposta visual
    // humana medida). true => e uma pseudocor, um simbolo sem correspondente
    // perceptual. A distincao deve sobreviver ate a interface.
    bool isFalseColour = true;

    // A cor XYZ estava fora do gamut sRGB e precisou ser ajustada. Quase sempre
    // true para cores espectrais puras -- e esperado, nao um erro.
    bool wasOutOfGamut = false;
    double gamutExcursion = 0.0;
};

// Converte uma frequencia eletromagnetica em cor, segundo a politica configurada.
class ColorEngine {
public:
    explicit ColorEngine(ColorEngineSettings settings = {});

    [[nodiscard]] const ColorEngineSettings& settings() const { return settings_; }

    [[nodiscard]] ColorResult fromEmFrequency(double emFrequencyHz) const;
    [[nodiscard]] ColorResult fromWavelength(double wavelengthM) const;

    // Media ponderada de varias cores. Feita em XYZ (luz linear): misturar
    // valores sRGB codificados escureceria o resultado.
    //
    // Atencao ao significado: a media de duas cores espectrais NAO e a cor de um
    // som com essas duas frequencias. E a cor de uma mistura aditiva de luz.
    // Som nao se combina assim -- duas senoides somadas produzem um espectro
    // com dois picos, nao um pico intermediario. Esta operacao e uma decisao de
    // apresentacao, e docs/color.md explica por que ela e defensavel mesmo assim.
    [[nodiscard]] ColorResult weightedMix(const std::vector<ColorResult>& colours,
                                          const std::vector<double>& weights) const;

private:
    ColorEngineSettings settings_;
};

}  // namespace soundwave
