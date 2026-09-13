#include "soundwave/core/config.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>

#include "soundwave/core/version.hpp"
#include "soundwave/dsp/fft.hpp"
#include "soundwave/mapping/mappers.hpp"
#include "soundwave/mapping/physical_mappers.hpp"
#include "soundwave/physics/survival.hpp"

namespace soundwave {
namespace {

std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::string stripQuotes(const std::string& text) {
    if (text.size() >= 2 && ((text.front() == '"' && text.back() == '"') ||
                             (text.front() == '\'' && text.back() == '\''))) {
        return text.substr(1, text.size() - 2);
    }
    return text;
}

bool toBool(const std::string& text, bool& out) {
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower == "true" || lower == "yes" || lower == "on" || lower == "1") { out = true; return true; }
    if (lower == "false" || lower == "no" || lower == "off" || lower == "0") { out = false; return true; }
    return false;
}

bool toDouble(const std::string& text, double& out) {
    try {
        std::size_t consumed = 0;
        const double value = std::stod(text, &consumed);
        if (consumed != text.size()) return false;
        out = value;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool toSize(const std::string& text, std::size_t& out) {
    double value = 0.0;
    if (!toDouble(text, value) || value < 0.0 || value != std::floor(value)) return false;
    out = static_cast<std::size_t>(value);
    return true;
}

// Lista de pares em linha: [[20, 4.0e14], [20000, 7.5e14]]
bool toControlPoints(const std::string& text, std::vector<std::pair<double, double>>& out) {
    std::vector<double> numbers;
    std::string token;
    for (char c : text) {
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '+' ||
            c == 'e' || c == 'E') {
            token.push_back(c);
        } else {
            if (!token.empty()) {
                double value = 0.0;
                if (!toDouble(token, value)) return false;
                numbers.push_back(value);
                token.clear();
            }
        }
    }
    if (!token.empty()) {
        double value = 0.0;
        if (!toDouble(token, value)) return false;
        numbers.push_back(value);
    }
    if (numbers.empty() || numbers.size() % 2 != 0) return false;

    out.clear();
    for (std::size_t i = 0; i + 1 < numbers.size(); i += 2) {
        out.emplace_back(numbers[i], numbers[i + 1]);
    }
    return true;
}

std::string formatDouble(double value) {
    std::ostringstream out;
    // Precisao alta o bastante para uma ida e volta exata em double. Sem isso,
    // gravar e reler a configuracao alteraria o resultado -- reprodutibilidade
    // perdida por arredondamento de impressao.
    out << std::setprecision(17) << value;
    return out.str();
}

}  // namespace

ConfigLoadResult parseConfig(const std::string& text) {
    ConfigLoadResult result;

    // Achatamos "secao.chave" -> valor. Suficiente para dois niveis, que e todo
    // o formato que o projeto aceita.
    std::map<std::string, std::string> entries;
    std::istringstream stream(text);
    std::string line;
    std::string section;
    int lineNumber = 0;

    while (std::getline(stream, line)) {
        ++lineNumber;
        const auto comment = line.find('#');
        if (comment != std::string::npos) line = line.substr(0, comment);
        if (trim(line).empty()) continue;

        const bool indented = !line.empty() && (line[0] == ' ' || line[0] == '\t');
        const auto colon = line.find(':');
        if (colon == std::string::npos) {
            result.error = "linha " + std::to_string(lineNumber) + ": esperado 'chave: valor'";
            return result;
        }

        const std::string key = trim(line.substr(0, colon));
        const std::string value = stripQuotes(trim(line.substr(colon + 1)));

        if (!indented) {
            if (value.empty()) { section = key; continue; }
            section.clear();
            entries[key] = value;
        } else {
            if (section.empty()) {
                result.error = "linha " + std::to_string(lineNumber) + ": chave indentada sem secao";
                return result;
            }
            entries[section + "." + key] = value;
        }
    }

    AnalysisConfig config;
    std::vector<std::string> consumed;

    auto readDouble = [&](const char* key, double& target) {
        auto it = entries.find(key);
        if (it == entries.end()) return;
        consumed.emplace_back(key);
        if (!toDouble(it->second, target)) {
            result.warnings.push_back(std::string(key) + ": nao e um numero ('" + it->second +
                                      "'), mantendo o padrao");
        }
    };
    auto readSize = [&](const char* key, std::size_t& target) {
        auto it = entries.find(key);
        if (it == entries.end()) return;
        consumed.emplace_back(key);
        if (!toSize(it->second, target)) {
            result.warnings.push_back(std::string(key) + ": nao e um inteiro nao negativo ('" +
                                      it->second + "'), mantendo o padrao");
        }
    };
    auto readBool = [&](const char* key, bool& target) {
        auto it = entries.find(key);
        if (it == entries.end()) return;
        consumed.emplace_back(key);
        if (!toBool(it->second, target)) {
            result.warnings.push_back(std::string(key) + ": nao e booleano ('" + it->second +
                                      "'), mantendo o padrao");
        }
    };
    auto readString = [&](const char* key, std::string& target) {
        auto it = entries.find(key);
        if (it == entries.end()) return;
        consumed.emplace_back(key);
        target = it->second;
    };

    readDouble("analysis.sample_rate", config.analysis.sampleRate);
    readSize("analysis.fft_size", config.analysis.fftSize);
    readSize("analysis.hop_size", config.analysis.hopSize);
    readString("analysis.window", config.analysis.window);
    readBool("analysis.remove_dc", config.analysis.removeDc);
    readBool("analysis.normalize_blocks", config.analysis.normalizeBlocks);

    readString("mapping.type", config.mapping.type);
    readDouble("mapping.source_min_hz", config.mapping.sourceMinHz);
    readDouble("mapping.source_max_hz", config.mapping.sourceMaxHz);
    readDouble("mapping.target_min_hz", config.mapping.targetMinHz);
    readDouble("mapping.target_max_hz", config.mapping.targetMaxHz);
    readDouble("mapping.reference_hz", config.mapping.referenceHz);
    readString("mapping.out_of_range", config.mapping.outOfRange);
    readString("mapping.acoustic_medium", config.mapping.acousticMedium);
    readString("mapping.optical_medium", config.mapping.opticalMedium);
    readDouble("mapping.anchor_hz", config.mapping.anchorHz);
    readDouble("mapping.anchor_nm", config.mapping.anchorNm);

    readDouble("environment.temperature_c", config.environment.temperatureC);
    readDouble("environment.pressure_kpa", config.environment.pressureKPa);
    readDouble("environment.relative_humidity", config.environment.relativeHumidity);
    readDouble("environment.salinity_ppt", config.environment.salinityPpt);
    readDouble("environment.depth_m", config.environment.depthM);
    readDouble("environment.ph", config.environment.pH);
    readBool("environment.apply_medium_filter", config.environment.applyMediumFilter);
    readString("environment.filter_medium", config.environment.filterMedium);
    readDouble("environment.path_length_m", config.environment.pathLengthM);

    if (auto it = entries.find("mapping.control_points"); it != entries.end()) {
        consumed.emplace_back("mapping.control_points");
        if (!toControlPoints(it->second, config.mapping.controlPoints)) {
            result.warnings.emplace_back(
                "mapping.control_points: formato invalido, esperado [[f1, em1], [f2, em2]]");
        }
    }

    readString("color.mode", config.color.mode);
    readString("color.gamut", config.color.gamut);
    readBool("color.normalise_luminance", config.color.normaliseLuminance);

    readString("interpretation.mode", config.interpretation.mode);
    readDouble("interpretation.analysis_min_hz", config.interpretation.analysisMinHz);
    readDouble("interpretation.analysis_max_hz", config.interpretation.analysisMaxHz);
    readSize("interpretation.max_contributions", config.interpretation.maxContributions);
    readDouble("interpretation.magnitude_floor_ratio", config.interpretation.magnitudeFloorRatio);
    readDouble("interpretation.weight_exponent", config.interpretation.weightExponent);
    readDouble("interpretation.silence_threshold", config.interpretation.silenceThreshold);

    readSize("render.width", config.render.width);
    readSize("render.height", config.render.height);
    readBool("render.log_frequency_axis", config.render.logFrequencyAxis);

    for (const auto& [key, value] : entries) {
        (void)value;
        if (std::find(consumed.begin(), consumed.end(), key) == consumed.end()) {
            result.warnings.push_back("chave desconhecida ignorada: " + key);
        }
    }

    result.ok = true;
    result.config = config;
    return result;
}

ConfigLoadResult loadConfig(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        ConfigLoadResult result;
        result.error = "nao foi possivel abrir a configuracao: " + path;
        return result;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return parseConfig(buffer.str());
}

std::vector<std::string> validateConfig(const AnalysisConfig& config) {
    std::vector<std::string> problems;

    if (!Fft::isPowerOfTwo(config.analysis.fftSize)) {
        problems.push_back("analysis.fft_size deve ser potencia de dois (recebido " +
                           std::to_string(config.analysis.fftSize) + ")");
    }
    if (config.analysis.hopSize == 0) {
        problems.emplace_back("analysis.hop_size deve ser maior que zero");
    }
    if (config.analysis.hopSize > config.analysis.fftSize) {
        problems.emplace_back(
            "analysis.hop_size maior que fft_size: parte do sinal nunca sera analisada");
    }

    WindowType window{};
    if (!parseWindowType(config.analysis.window, window)) {
        problems.push_back("analysis.window desconhecida: " + config.analysis.window);
    }

    const std::string& type = config.mapping.type;
    if (type != "linear" && type != "logarithmic" && type != "octave" && type != "custom" &&
        type != "identity" && type != "scale" && type != "aligned") {
        problems.push_back("mapping.type desconhecido: " + type);
    }
    if (type == "scale") {
        const auto acoustic = media::byName(config.mapping.acousticMedium);
        if (!acoustic) {
            problems.push_back("mapping.acoustic_medium desconhecido: " +
                               config.mapping.acousticMedium);
        } else if (!acoustic->carriesSound()) {
            problems.push_back("mapping.acoustic_medium '" + config.mapping.acousticMedium +
                               "' nao propaga som: onda mecanica exige meio material");
        }
        if (!media::byName(config.mapping.opticalMedium)) {
            problems.push_back("mapping.optical_medium desconhecido: " +
                               config.mapping.opticalMedium);
        }
        if (!(config.mapping.anchorHz > 0.0) || !(config.mapping.anchorNm > 0.0)) {
            problems.emplace_back("mapping.anchor_hz e mapping.anchor_nm devem ser positivos");
        }
    }
    if (config.environment.applyMediumFilter && !media::byName(config.environment.filterMedium)) {
        problems.push_back("environment.filter_medium desconhecido: " +
                           config.environment.filterMedium);
    }
    if (config.environment.pathLengthM < 0.0) {
        problems.emplace_back("environment.path_length_m nao pode ser negativo");
    }
    if (type == "custom" && config.mapping.controlPoints.size() < 2) {
        problems.emplace_back("mapping.type=custom exige ao menos 2 mapping.control_points");
    }
    if (!(config.mapping.sourceMinHz > 0.0) ||
        config.mapping.sourceMaxHz <= config.mapping.sourceMinHz) {
        problems.emplace_back("mapping: exige 0 < source_min_hz < source_max_hz");
    }
    if (!(config.mapping.targetMinHz > 0.0) ||
        config.mapping.targetMaxHz <= config.mapping.targetMinHz) {
        problems.emplace_back("mapping: exige 0 < target_min_hz < target_max_hz");
    }

    OutOfRangePolicy policy{};
    if (!parseOutOfRangePolicy(config.mapping.outOfRange, policy)) {
        problems.push_back("mapping.out_of_range desconhecida: " + config.mapping.outOfRange);
    }

    SpectrumMode spectrumMode{};
    if (!parseSpectrumMode(config.color.mode, spectrumMode)) {
        problems.push_back("color.mode desconhecido: " + config.color.mode);
    }
    GamutStrategy gamut{};
    if (!parseGamutStrategy(config.color.gamut, gamut)) {
        problems.push_back("color.gamut desconhecido: " + config.color.gamut);
    }
    InterpretationMode interpretation{};
    if (!parseInterpretationMode(config.interpretation.mode, interpretation)) {
        problems.push_back("interpretation.mode desconhecido: " + config.interpretation.mode);
    }
    if (!(config.interpretation.weightExponent > 0.0)) {
        problems.emplace_back("interpretation.weight_exponent deve ser positivo");
    }
    if (config.interpretation.analysisMaxHz <= config.interpretation.analysisMinHz) {
        problems.emplace_back("interpretation: exige analysis_min_hz < analysis_max_hz");
    }
    if (config.render.width == 0 || config.render.height == 0) {
        problems.emplace_back("render.width e render.height devem ser maiores que zero");
    }
    return problems;
}

std::unique_ptr<FrequencyMapper> makeMapper(const AnalysisConfig& config) {
    MappingDomain domain;
    domain.sourceMinHz = config.mapping.sourceMinHz;
    domain.sourceMaxHz = config.mapping.sourceMaxHz;
    domain.targetMinHz = config.mapping.targetMinHz;
    domain.targetMaxHz = config.mapping.targetMaxHz;

    OutOfRangePolicy policy = OutOfRangePolicy::Clamp;
    // Um valor invalido ja foi reportado por validateConfig; aqui o padrao e o
    // recuo deliberado, nao um erro engolido.
    (void)parseOutOfRangePolicy(config.mapping.outOfRange, policy);

    const std::string& type = config.mapping.type;
    if (type == "identity") return std::make_unique<IdentityMapper>(domain);
    if (type == "aligned") {
        return std::make_unique<AlignedMapper>(makeMediumFilterSettings(config), domain,
                                               config.mapping.referenceHz);
    }
    if (type == "scale") {
        const Medium acoustic =
            media::byName(config.mapping.acousticMedium).value_or(media::air());
        const Medium optical =
            media::byName(config.mapping.opticalMedium).value_or(media::vacuum());
        return std::make_unique<ScaleMapper>(ScaleMapper::anchored(
            acoustic, optical, makeConditions(config), config.mapping.anchorHz,
            config.mapping.anchorNm, domain));
    }
    if (type == "linear") return std::make_unique<LinearMapper>(domain, policy);
    if (type == "octave") {
        return std::make_unique<OctaveMapper>(domain, config.mapping.referenceHz, policy);
    }
    if (type == "custom") {
        return std::make_unique<CustomMapper>(config.mapping.controlPoints, domain, policy);
    }
    return std::make_unique<LogMapper>(domain, policy);
}

AnalyzerSettings makeAnalyzerSettings(const AnalysisConfig& config) {
    AnalyzerSettings settings;
    settings.fftSize = config.analysis.fftSize;
    settings.hopSize = config.analysis.hopSize;
    (void)parseWindowType(config.analysis.window, settings.window);
    settings.removeDc = config.analysis.removeDc;
    settings.normalizeBlocks = config.analysis.normalizeBlocks;
    return settings;
}

ColorEngineSettings makeColorEngineSettings(const AnalysisConfig& config) {
    ColorEngineSettings settings;
    (void)parseSpectrumMode(config.color.mode, settings.mode);
    (void)parseGamutStrategy(config.color.gamut, settings.gamut);
    settings.normaliseLuminance = config.color.normaliseLuminance;
    return settings;
}

InterpreterSettings makeInterpreterSettings(const AnalysisConfig& config) {
    InterpreterSettings settings;
    (void)parseInterpretationMode(config.interpretation.mode, settings.mode);
    settings.analysisMinHz = config.interpretation.analysisMinHz;
    settings.analysisMaxHz = config.interpretation.analysisMaxHz;
    settings.maxContributions = config.interpretation.maxContributions;
    settings.magnitudeFloorRatio = config.interpretation.magnitudeFloorRatio;
    settings.weightExponent = config.interpretation.weightExponent;
    settings.silenceThreshold = config.interpretation.silenceThreshold;
    return settings;
}

Conditions makeConditions(const AnalysisConfig& config) {
    Conditions conditions;
    conditions.temperatureC = config.environment.temperatureC;
    conditions.pressureKPa = config.environment.pressureKPa;
    conditions.relativeHumidity = config.environment.relativeHumidity;
    conditions.salinityPpt = config.environment.salinityPpt;
    conditions.depthM = config.environment.depthM;
    conditions.pH = config.environment.pH;
    return conditions;
}

MediumFilterSettings makeMediumFilterSettings(const AnalysisConfig& config) {
    MediumFilterSettings settings;
    const Medium medium =
        media::byName(config.environment.filterMedium).value_or(media::seaWater());
    settings.acoustic = medium;
    settings.optical = medium;
    settings.conditions = makeConditions(config);
    settings.pathLengthM = config.environment.pathLengthM;
    return settings;
}

FidelityBudget pipelineFidelity(const AnalysisConfig& config, const FrequencyMapper& mapper,
                                double resolutionHz, double representativeHz) {
    FidelityBudget budget;

    // Camada 1 -- analise. E medida, e tem incerteza mensuravel.
    budget.add(claims::fftResolution(resolutionHz, representativeHz));

    // Camada 2 -- transformacao. Aqui mora a escolha, salvo nos dois mapeadores
    // fisicos, que declaram o proprio orcamento.
    if (const auto* aligned = dynamic_cast<const AlignedMapper*>(&mapper)) {
        budget.merge(aligned->fidelity());
    } else if (const auto* scale = dynamic_cast<const ScaleMapper*>(&mapper)) {
        budget.merge(scale->fidelity());
    } else if (const auto* identity = dynamic_cast<const IdentityMapper*>(&mapper)) {
        budget.add(identity->fidelity());
    } else {
        budget.add(claims::arbitraryMapping(mapper.name()));
    }

    // Camada 3 -- conversao.
    budget.add(claims::speedOfLight());
    budget.add(claims::wavelengthFromFrequency());
    budget.add(claims::visibleRangeConvention());
    budget.add(claims::cie1931Observer());
    budget.add(claims::srgbEncoding());
    budget.add(claims::gamutMapping());
    if (config.color.normaliseLuminance) budget.add(claims::luminanceNormalisation());

    // Filtro de meio, quando ativo.
    if (config.environment.applyMediumFilter) {
        const MediumFilterSettings filter = makeMediumFilterSettings(config);
        budget.merge(filterFidelity(filter, 550.0));
    }
    return budget;
}

std::string configToYaml(const AnalysisConfig& config) {
    std::ostringstream out;
    out << "analysis:\n"
        << "  sample_rate: " << formatDouble(config.analysis.sampleRate)
        << "        # 0 = usar a taxa do arquivo de entrada\n"
        << "  fft_size: " << config.analysis.fftSize << "\n"
        << "  hop_size: " << config.analysis.hopSize << "\n"
        << "  window: " << config.analysis.window << "\n"
        << "  remove_dc: " << (config.analysis.removeDc ? "true" : "false") << "\n"
        << "  normalize_blocks: " << (config.analysis.normalizeBlocks ? "true" : "false") << "\n"
        << "\nmapping:\n"
        << "  type: " << config.mapping.type << "\n"
        << "  source_min_hz: " << formatDouble(config.mapping.sourceMinHz) << "\n"
        << "  source_max_hz: " << formatDouble(config.mapping.sourceMaxHz) << "\n"
        << "  target_min_hz: " << formatDouble(config.mapping.targetMinHz) << "\n"
        << "  target_max_hz: " << formatDouble(config.mapping.targetMaxHz) << "\n"
        << "  reference_hz: " << formatDouble(config.mapping.referenceHz) << "\n"
        << "  out_of_range: " << config.mapping.outOfRange << "\n"
        << "  acoustic_medium: " << config.mapping.acousticMedium
        << "   # so para type: scale\n"
        << "  optical_medium: " << config.mapping.opticalMedium << "\n"
        << "  anchor_hz: " << formatDouble(config.mapping.anchorHz) << "\n"
        << "  anchor_nm: " << formatDouble(config.mapping.anchorNm) << "\n";
    if (!config.mapping.controlPoints.empty()) {
        out << "  control_points: [";
        for (std::size_t i = 0; i < config.mapping.controlPoints.size(); ++i) {
            if (i != 0) out << ", ";
            out << "[" << formatDouble(config.mapping.controlPoints[i].first) << ", "
                << formatDouble(config.mapping.controlPoints[i].second) << "]";
        }
        out << "]\n";
    }
    out << "\nenvironment:\n"
        << "  temperature_c: " << formatDouble(config.environment.temperatureC) << "\n"
        << "  pressure_kpa: " << formatDouble(config.environment.pressureKPa) << "\n"
        << "  relative_humidity: " << formatDouble(config.environment.relativeHumidity) << "\n"
        << "  salinity_ppt: " << formatDouble(config.environment.salinityPpt) << "\n"
        << "  depth_m: " << formatDouble(config.environment.depthM) << "\n"
        << "  ph: " << formatDouble(config.environment.pH) << "\n"
        << "  apply_medium_filter: " << (config.environment.applyMediumFilter ? "true" : "false")
        << "\n"
        << "  filter_medium: " << config.environment.filterMedium << "\n"
        << "  path_length_m: " << formatDouble(config.environment.pathLengthM) << "\n";

    out << "\ncolor:\n"
        << "  mode: " << config.color.mode << "\n"
        << "  gamut: " << config.color.gamut << "\n"
        << "  normalise_luminance: " << (config.color.normaliseLuminance ? "true" : "false") << "\n"
        << "\ninterpretation:\n"
        << "  mode: " << config.interpretation.mode << "\n"
        << "  analysis_min_hz: " << formatDouble(config.interpretation.analysisMinHz) << "\n"
        << "  analysis_max_hz: " << formatDouble(config.interpretation.analysisMaxHz) << "\n"
        << "  max_contributions: " << config.interpretation.maxContributions << "\n"
        << "  magnitude_floor_ratio: " << formatDouble(config.interpretation.magnitudeFloorRatio)
        << "\n"
        << "  weight_exponent: " << formatDouble(config.interpretation.weightExponent) << "\n"
        << "  silence_threshold: " << formatDouble(config.interpretation.silenceThreshold) << "\n"
        << "\nrender:\n"
        << "  width: " << config.render.width << "\n"
        << "  height: " << config.render.height << "\n"
        << "  log_frequency_axis: " << (config.render.logFrequencyAxis ? "true" : "false") << "\n";
    return out.str();
}

bool saveConfig(const std::string& path, const AnalysisConfig& config, std::string* error) {
    std::ofstream file(path);
    if (!file) {
        if (error) *error = "nao foi possivel gravar em: " + path;
        return false;
    }
    file << "# Configuracao do SoundWave -- gerada automaticamente.\n"
         << "# Todo parametro aqui pode alterar o resultado. Guarde este arquivo\n"
         << "# junto com a imagem produzida, ou o resultado nao e reproduzivel.\n\n"
         << configToYaml(config);
    if (!file.good()) {
        if (error) *error = "falha de escrita em: " + path;
        return false;
    }
    return true;
}

std::string fingerprintSamples(const std::vector<double>& samples) {
    // FNV-1a de 64 bits sobre a representacao binaria das amostras. Usamos os
    // bits do double e nao o valor impresso: a impressao perderia precisao e
    // dois sinais diferentes poderiam colidir por arredondamento.
    std::uint64_t hash = 1469598103934665603ULL;
    for (double sample : samples) {
        std::uint64_t bits = 0;
        static_assert(sizeof(bits) == sizeof(sample));
        std::memcpy(&bits, &sample, sizeof(bits));
        for (int byte = 0; byte < 8; ++byte) {
            hash ^= (bits >> (byte * 8)) & 0xFFULL;
            hash *= 1099511628211ULL;
        }
    }
    std::ostringstream out;
    out << "fnv1a64:" << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

std::string manifestToYaml(const RunManifest& manifest) {
    std::ostringstream out;
    out << "# Manifesto de execucao do SoundWave.\n"
        << "# Este arquivo e o que torna o resultado verificavel: ele registra\n"
        << "# TODAS as escolhas arbitrarias que produziram a imagem.\n\n"
        << "run:\n"
        << "  soundwave_version: " << manifest.soundwaveVersion << "\n"
        << "  algorithm_version: " << manifest.algorithmVersion << "\n"
        << "  input_path: \"" << manifest.inputPath << "\"\n"
        << "  input_fingerprint: " << manifest.inputFingerprint << "\n"
        << "  input_sample_rate: " << formatDouble(manifest.inputSampleRate) << "\n"
        << "  input_samples: " << manifest.inputSamples << "\n"
        << "  mapping_description: \"" << manifest.mapperDescription << "\"\n\n"
        << configToYaml(manifest.config);

    if (!manifest.fidelity.empty()) {
        // Em bloco de comentario: e um relatorio para humanos, e mantem o
        // manifesto relegivel pelo proprio parser do projeto.
        out << "\n# ";
        const std::string report = manifest.fidelity.report();
        for (char ch : report) {
            out << ch;
            if (ch == '\n') out << "# ";
        }
        out << "\n";
    }
    return out.str();
}

bool saveManifest(const std::string& path, const RunManifest& manifest, std::string* error) {
    std::ofstream file(path);
    if (!file) {
        if (error) *error = "nao foi possivel gravar em: " + path;
        return false;
    }
    file << manifestToYaml(manifest);
    if (!file.good()) {
        if (error) *error = "falha de escrita em: " + path;
        return false;
    }
    return true;
}

}  // namespace soundwave
