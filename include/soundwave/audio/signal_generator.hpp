#pragma once

#include <cstdint>
#include <vector>

#include "soundwave/audio/pcm_buffer.hpp"

namespace soundwave {

// Sinais sinteticos deterministas para validacao (plano, secao 10).
//
// Todo gerador e determinista -- inclusive o ruido, que usa um PRNG com semente
// explicita. Sem isso nao existe "mesma configuracao -> mesmo resultado", e o
// criterio de reprodutibilidade do plano viraria letra morta.
namespace signals {

// Senoide pura. `phaseRad` permite testar se a fase estimada pela FFT confere.
[[nodiscard]] PcmBuffer sine(double frequencyHz, double durationSeconds, double sampleRate,
                             double amplitude = 0.5, double phaseRad = 0.0);

// Soma de senoides de mesma amplitude. Amplitude total e dividida pelo numero de
// componentes para evitar ceifamento.
[[nodiscard]] PcmBuffer chord(const std::vector<double>& frequenciesHz, double durationSeconds,
                              double sampleRate, double amplitude = 0.5);

// Fundamental + harmonicos com queda 1/n. Serve para estudar a pergunta 3 do
// plano: como harmonicos alteram a representacao visual.
[[nodiscard]] PcmBuffer harmonicSeries(double fundamentalHz, std::size_t harmonics,
                                       double durationSeconds, double sampleRate,
                                       double amplitude = 0.5);

// Varredura exponencial de startHz a endHz. Exponencial (e nao linear) porque a
// percepcao de altura e logaritmica: uma varredura linear passa quase todo o
// tempo nos agudos.
[[nodiscard]] PcmBuffer logSweep(double startHz, double endHz, double durationSeconds,
                                 double sampleRate, double amplitude = 0.5);

// Ruido branco uniforme com semente fixa.
[[nodiscard]] PcmBuffer whiteNoise(double durationSeconds, double sampleRate,
                                   double amplitude = 0.25, std::uint64_t seed = 0x5EED1234U);

[[nodiscard]] PcmBuffer silence(double durationSeconds, double sampleRate);

}  // namespace signals
}  // namespace soundwave
