#pragma once

#include <string>

#include "soundwave/audio/pcm_buffer.hpp"
#include "soundwave/core/config.hpp"

namespace soundwave::viz {

// Visualizacao em tempo real com reproducao sincronizada (Milestone 5 do plano).
//
// Herda o carater visual do SoundWave original -- forma de onda, barras de
// frequencia, rastro do espectrograma -- mas as cores agora vem do pipeline
// completo (mapeamento -> lambda -> XYZ -> sRGB) em vez de uma rampa de matiz
// HSV arbitraria.
//
// O plano coloca isto DEPOIS da validacao matematica, e por bom motivo: em
// tempo real nao da para inspecionar numeros, so impressao visual -- que e
// exatamente o tipo de evidencia que engana. Use `analyze` para verificar,
// `live` para explorar.
struct RealtimeOptions {
    std::size_t windowWidth = 1280;
    std::size_t windowHeight = 720;
    std::string title = "SoundWave";
    bool playAudio = true;
    bool loop = true;
};

// true se o binario foi compilado com suporte a SDL2.
[[nodiscard]] bool isAvailable();

// Executa ate o usuario fechar a janela. Devolve codigo de saida no estilo main.
// Sem SDL2 compilado, devolve um erro explicativo sem travar.
[[nodiscard]] int run(const PcmBuffer& buffer, const AnalysisConfig& config,
                      const RealtimeOptions& options);

}  // namespace soundwave::viz
