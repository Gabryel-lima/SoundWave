// O pipeline inteiro em uma pagina, sem CLI e sem configuracao -- o caminho mais
// curto de um sinal ate uma cor, com cada camada explicita.
//
// Compile: cmake -S . -B build && cmake --build build
// Rode:    ./build/example_minimal_pipeline

#include <iomanip>
#include <iostream>

#include "soundwave/audio/signal_generator.hpp"
#include "soundwave/color/color_engine.hpp"
#include "soundwave/dsp/analyzer.hpp"
#include "soundwave/mapping/mappers.hpp"

int main() {
    using namespace soundwave;

    // ---- Camada 0: um sinal conhecido -------------------------------------
    const double sampleRate = 44100.0;
    const PcmBuffer signal = signals::sine(440.0, 1.0, sampleRate, 0.5);

    // ---- Camada 1: ANALISE -- o que existe no sinal? -----------------------
    AnalyzerSettings analyzerSettings;
    analyzerSettings.fftSize = 4096;
    analyzerSettings.window = WindowType::Hann;

    const SpectrumAnalyzer analyzer(analyzerSettings, sampleRate);
    const Spectrum spectrum = analyzer.analyzeBlock(signal.samples, 0);
    const auto peak = dominantPeak(spectrum, 20.0, 20000.0);
    if (!peak) {
        std::cerr << "nenhum pico encontrado\n";
        return 1;
    }

    // ---- Camada 2: TRANSFORMACAO -- a escolha arbitraria --------------------
    const LogMapper mapper;
    const double emFrequency = mapper.map(peak->frequencyHz);

    // ---- Camada 3: CONVERSAO -- lambda e resposta visual --------------------
    const ColorEngine engine;
    const ColorResult colour = engine.fromEmFrequency(emFrequency);

    // ---- Camada 4: RENDERIZACAO -- aqui, apenas texto ----------------------
    std::cout << std::fixed
              << "CAMADA 1  analise\n"
              << "  frequencia dominante : " << std::setprecision(2) << peak->frequencyHz
              << " Hz\n"
              << "  magnitude            : " << std::setprecision(4) << peak->magnitude << "\n"
              << "  resolucao da FFT     : " << std::setprecision(3) << spectrum.resolutionHz()
              << " Hz por bin\n\n"
              << "CAMADA 2  transformacao (ESCOLHA, nao fisica)\n"
              << "  mapeamento           : " << mapper.name() << "\n"
              << "  f_EM                 : " << std::setprecision(2) << (emFrequency / 1e12)
              << " THz\n\n"
              << "CAMADA 3  conversao\n"
              << "  lambda = c / f_EM    : " << std::setprecision(1)
              << (colour.wavelengthM * 1e9) << " nm   <- unico passo fisico\n"
              << "  banda                : " << emBandName(colour.band) << "\n"
              << "  XYZ                  : " << std::setprecision(4) << colour.xyz.x << ", "
              << colour.xyz.y << ", " << colour.xyz.z << "\n"
              << "  sRGB                 : (" << static_cast<int>(colour.rgb.r) << ", "
              << static_cast<int>(colour.rgb.g) << ", " << static_cast<int>(colour.rgb.b) << ")\n"
              << "  fora do gamut        : " << (colour.wasOutOfGamut ? "sim" : "nao")
              << "   <- esperado: cores espectrais puras nao cabem no sRGB\n"
              << "  pseudocor            : " << (colour.isFalseColour ? "sim" : "nao") << "\n\n"
              << "A cor acima NAO e 'a cor de 440 Hz'. E o que a funcao "
              << mapper.name() << " produz\n"
              << "para 440 Hz. Trocar a funcao troca a cor, e nada no som mudou.\n";
    return 0;
}
