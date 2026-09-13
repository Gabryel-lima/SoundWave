#pragma once

#include <vector>

#include "soundwave/color/color_engine.hpp"
#include "soundwave/core/spectrum.hpp"
#include "soundwave/mapping/frequency_mapper.hpp"
#include "soundwave/render/image.hpp"
#include "soundwave/render/interpretation.hpp"

namespace soundwave {

struct PlotSettings {
    std::size_t width = 1200;
    std::size_t height = 500;

    // Eixo de frequencia em escala logaritmica. Praticamente obrigatorio: em
    // escala linear, as sete oitavas graves onde mora quase toda a informacao
    // musical ficam espremidas nos primeiros 5% do eixo.
    bool logFrequencyAxis = true;

    double minHz = kAudibleMinHz;
    double maxHz = kAudibleMaxHz;

    // Faixa dinamica exibida, em dB abaixo do pico. Tudo abaixo vira fundo.
    // 60 dB e um compromisso: mostra estrutura sem encher a imagem de ruido.
    double dynamicRangeDb = 60.0;

    Rgb background = Rgb{12, 12, 16};
};

// Espectrograma colorido: eixo X = tempo, eixo Y = frequencia sonora, MATIZ dado
// pelo mapeamento f_som -> f_EM -> cor, BRILHO dado pela magnitude em dB.
//
// Matiz e brilho carregam informacoes independentes de proposito: o matiz e a
// transformacao arbitraria em estudo, o brilho e o dado medido. Misturar os dois
// no mesmo canal visual tornaria impossivel dizer qual dos dois esta variando.
[[nodiscard]] Image renderSpectrogram(const std::vector<Spectrum>& frames,
                                      const FrequencyMapper& mapper, const ColorEngine& engine,
                                      const PlotSettings& settings);

// Faixa horizontal com a cor representativa de cada quadro ao longo do tempo.
// E a saida mais direta do pipeline: "esta musica, com este mapeamento, tem
// estas cores nesta ordem".
[[nodiscard]] Image renderColorTimeline(const std::vector<FrameInterpretation>& frames,
                                        const PlotSettings& settings);

// Espectro de um unico quadro: magnitude em dB, barras coloridas pelo mapeamento.
[[nodiscard]] Image renderSpectrumPlot(const Spectrum& spectrum, const FrequencyMapper& mapper,
                                       const ColorEngine& engine, const PlotSettings& settings);

// A funcao de mapeamento em si, como uma regua de cor sobre o eixo audivel, com
// marcas nas oitavas de A (55, 110, 220, 440, 880, ... Hz).
//
// E o diagnostico mais util do projeto: as marcas de oitava mostram de imediato
// se o mapeador preserva ou nao a equivalencia de oitava. Com LogMapper, oitavas
// vizinhas tem cores quase iguais mas nunca repetem; com OctaveMapper, TODAS as
// marcas caem exatamente sobre a mesma cor. Isso torna visivel a limitacao
// discutida em docs/mapping.md.
[[nodiscard]] Image renderMappingRuler(const FrequencyMapper& mapper, const ColorEngine& engine,
                                       const PlotSettings& settings);

}  // namespace soundwave
