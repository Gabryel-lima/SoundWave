#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "soundwave/color/color_engine.hpp"
#include "soundwave/core/spectrum.hpp"
#include "soundwave/mapping/frequency_mapper.hpp"

namespace soundwave {

// Como reduzir um espectro inteiro (milhares de bins) a uma representacao
// visual (plano, secao 7).
//
// Este e um problema de compressao com perda e nao tem resposta certa. Cada modo
// descarta informacao diferente, e o modo escolhido e parte do resultado.
enum class InterpretationMode {
    // So a maior magnitude importa. Estavel e legivel para sinais quase
    // monofonicos; descarta tudo o mais e pisca de forma erratica quando dois
    // parciais disputam o topo.
    Dominant,
    // Media ponderada pela magnitude, em luz linear. Estavel, mas tende ao
    // centro do gamut: sinais de banda larga convergem todos para um cinza
    // esverdeado parecido -- e por isso que ruido branco e um teste severo.
    Weighted,
    // Guarda as contribuicoes individuais sem colapsa-las. Nao produz "uma cor";
    // produz um conjunto, que o renderizador desenha lado a lado.
    Spectral,
};

[[nodiscard]] std::string_view interpretationModeName(InterpretationMode mode);
[[nodiscard]] bool parseInterpretationMode(std::string_view name, InterpretationMode& out);

struct BinContribution {
    double sourceHz = 0.0;
    double emHz = 0.0;
    double wavelengthM = 0.0;
    double magnitude = 0.0;
    double weight = 0.0;  // normalizado, soma 1 no conjunto
    ColorResult colour;
};

struct FrameInterpretation {
    double timeOffsetSeconds = 0.0;
    ColorResult colour;                  // cor representativa do quadro
    double dominantSourceHz = 0.0;
    double dominantMagnitude = 0.0;
    double totalMagnitude = 0.0;
    std::vector<BinContribution> contributions;  // vazio no modo Dominant
    bool silent = false;                 // abaixo do limiar: preto, sem cor
};

struct InterpreterSettings {
    InterpretationMode mode = InterpretationMode::Dominant;

    // Faixa de analise. Independente do dominio do mapeador: aqui decidimos
    // quais bins sequer entram no calculo.
    double analysisMinHz = kAudibleMinHz;
    double analysisMaxHz = kAudibleMaxHz;

    // Quantos picos alimentam Weighted/Spectral.
    std::size_t maxContributions = 32;
    // Picos abaixo desta fracao do maior sao descartados.
    double magnitudeFloorRatio = 0.02;

    // Peso = magnitude^weightExponent. 1 pondera por amplitude, 2 por energia.
    // Expoentes maiores concentram a cor no parcial mais forte, aproximando o
    // resultado do modo Dominant de forma continua.
    double weightExponent = 1.0;

    // Abaixo desta magnitude total o quadro e silencio. Sem isso, trechos mudos
    // ganham a cor do ruido de fundo amplificado, o que e ruido puro na saida.
    double silenceThreshold = 1e-5;
};

// Junta as camadas 2 e 3: espectro -> mapeamento -> cor.
//
// Nao possui nem o mapeador nem o motor de cor; recebe referencias. Isso torna
// barato rodar o MESMO espectro por varios mapeadores diferentes, que e o
// experimento central proposto no plano (perguntas 1, 4 e 8).
class Interpreter {
public:
    Interpreter(const FrequencyMapper& mapper, const ColorEngine& engine,
                InterpreterSettings settings = {});

    [[nodiscard]] FrameInterpretation interpret(const Spectrum& spectrum) const;
    [[nodiscard]] std::vector<FrameInterpretation> interpretAll(
        const std::vector<Spectrum>& frames) const;

    [[nodiscard]] const InterpreterSettings& settings() const { return settings_; }
    [[nodiscard]] const FrequencyMapper& mapper() const { return *mapper_; }
    [[nodiscard]] const ColorEngine& engine() const { return *engine_; }

private:
    [[nodiscard]] BinContribution makeContribution(double sourceHz, double magnitude) const;

    const FrequencyMapper* mapper_;
    const ColorEngine* engine_;
    InterpreterSettings settings_;
};

}  // namespace soundwave
