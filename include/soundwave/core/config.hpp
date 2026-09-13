#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "soundwave/color/color_engine.hpp"
#include "soundwave/core/version.hpp"
#include "soundwave/dsp/analyzer.hpp"
#include "soundwave/mapping/frequency_mapper.hpp"
#include "soundwave/physics/medium_filter.hpp"
#include "soundwave/render/interpretation.hpp"

namespace soundwave {

// ---------------------------------------------------------------------------
// REPRODUTIBILIDADE.
//
// Um resultado do SoundWave e uma imagem produzida por uma cadeia de escolhas
// arbitrarias. Sem registrar essas escolhas, a imagem nao e um dado -- e uma
// ilustracao. Todo parametro que possa mudar a saida mora aqui, e todo resultado
// e acompanhado do manifesto correspondente.
//
// O formato e um subconjunto de YAML deliberadamente pequeno: mapas aninhados
// de dois niveis, escalares e listas em linha. Nao usamos uma biblioteca YAML
// completa porque nao queremos que o formato do experimento aceite construcoes
// que o codigo nao saiba reproduzir.
// ---------------------------------------------------------------------------
struct AnalysisConfig {
    struct Analysis {
        double sampleRate = 0.0;  // 0 = usar a do arquivo de entrada
        std::size_t fftSize = 4096;
        std::size_t hopSize = 1024;
        std::string window = "hann";
        bool removeDc = true;
        bool normalizeBlocks = false;
    } analysis;

    struct Mapping {
        // linear | logarithmic | octave | custom | identity | scale
        //
        // identity e scale sao as HIPOTESES NULAS fisicamente fundamentadas.
        // Ver docs/physics.md: elas mostram o que acontece quando voce se recusa
        // a inventar a funcao -- e a resposta e que quase nada fica visivel.
        std::string type = "logarithmic";
        double sourceMinHz = kAudibleMinHz;
        double sourceMaxHz = kAudibleMaxHz;
        double targetMinHz = kVisibleMinHz;
        double targetMaxHz = kVisibleMaxHz;
        double referenceHz = 440.0;        // so para o tipo octave
        std::string outOfRange = "clamp";
        std::vector<std::pair<double, double>> controlPoints;  // so para custom

        // Apenas para type: scale.
        std::string acousticMedium = "air";   // meio onde o som se propaga
        std::string opticalMedium = "vacuum"; // meio onde a luz se propaga
        double anchorHz = 20.0;               // f_som ancorada...
        double anchorNm = 750.0;              // ...neste comprimento de onda
    } mapping;

    // Condicoes ambientais dos modelos fisicos. So tem efeito com type: scale
    // ou com o filtro de meio ativo.
    struct Environment {
        double temperatureC = 20.0;
        double pressureKPa = 101.325;
        double relativeHumidity = 70.0;
        double salinityPpt = 35.0;
        double depthM = 0.0;
        double pH = 8.1;

        // Filtro de meio: atenua o espectro pela absorcao real do meio.
        bool applyMediumFilter = false;
        std::string filterMedium = "sea-water";
        double pathLengthM = 10.0;
    } environment;

    struct Color {
        std::string mode = "visible-with-bands";
        std::string gamut = "desaturate";
        bool normaliseLuminance = true;
    } color;

    struct Interpretation {
        std::string mode = "dominant";
        double analysisMinHz = kAudibleMinHz;
        double analysisMaxHz = kAudibleMaxHz;
        std::size_t maxContributions = 32;
        double magnitudeFloorRatio = 0.02;
        double weightExponent = 1.0;
        double silenceThreshold = 1e-5;
    } interpretation;

    struct Render {
        std::size_t width = 1200;
        std::size_t height = 500;
        bool logFrequencyAxis = true;
    } render;
};

struct ConfigLoadResult {
    bool ok = false;
    std::string error;
    std::vector<std::string> warnings;  // chaves desconhecidas, valores ajustados
    AnalysisConfig config;
};

// Le um arquivo de configuracao. Chaves desconhecidas viram AVISO, nao erro
// silencioso: um parametro digitado errado que fosse ignorado sem aviso tornaria
// o manifesto uma mentira.
[[nodiscard]] ConfigLoadResult loadConfig(const std::string& path);
[[nodiscard]] ConfigLoadResult parseConfig(const std::string& text);

[[nodiscard]] std::string configToYaml(const AnalysisConfig& config);
[[nodiscard]] bool saveConfig(const std::string& path, const AnalysisConfig& config,
                              std::string* error = nullptr);

// Validacao: devolve a lista de problemas. Vazia = configuracao utilizavel.
[[nodiscard]] std::vector<std::string> validateConfig(const AnalysisConfig& config);

// Construcao dos objetos do pipeline a partir da configuracao.
[[nodiscard]] std::unique_ptr<FrequencyMapper> makeMapper(const AnalysisConfig& config);
[[nodiscard]] AnalyzerSettings makeAnalyzerSettings(const AnalysisConfig& config);
[[nodiscard]] ColorEngineSettings makeColorEngineSettings(const AnalysisConfig& config);
[[nodiscard]] InterpreterSettings makeInterpreterSettings(const AnalysisConfig& config);
[[nodiscard]] Conditions makeConditions(const AnalysisConfig& config);
[[nodiscard]] MediumFilterSettings makeMediumFilterSettings(const AnalysisConfig& config);

// Orcamento de fidelidade da configuracao inteira: toda etapa do pipeline
// declara sua natureza e incerteza. Ver docs/fidelity.md.
[[nodiscard]] FidelityBudget pipelineFidelity(const AnalysisConfig& config,
                                              const FrequencyMapper& mapper,
                                              double resolutionHz, double representativeHz);

// Manifesto do experimento: a configuracao efetiva mais o que a identifica.
// Gravado junto de toda saida.
struct RunManifest {
    std::string soundwaveVersion;
    int algorithmVersion = kAlgorithmVersion;
    std::string inputPath;
    std::string inputFingerprint;  // hash do conteudo, nao do caminho
    double inputSampleRate = 0.0;
    std::size_t inputSamples = 0;
    std::string mapperDescription;  // describe() do mapeador de fato usado
    AnalysisConfig config;
    FidelityBudget fidelity;        // natureza e incerteza de cada etapa
};

[[nodiscard]] std::string manifestToYaml(const RunManifest& manifest);
[[nodiscard]] bool saveManifest(const std::string& path, const RunManifest& manifest,
                                std::string* error = nullptr);

// Impressao digital FNV-1a de 64 bits sobre as amostras decodificadas.
//
// Sobre as AMOSTRAS e nao sobre os bytes do arquivo: dois arquivos com
// metadados diferentes mas o mesmo audio devem produzir a mesma impressao, ja
// que produzem o mesmo resultado. Nao e criptografica -- serve para detectar
// "a entrada mudou", nao para resistir a adversario.
[[nodiscard]] std::string fingerprintSamples(const std::vector<double>& samples);

}  // namespace soundwave
