#include "commands.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <memory>

#include "soundwave/audio/signal_generator.hpp"
#include "soundwave/audio/wav_reader.hpp"
#include "soundwave/core/config.hpp"
#include "soundwave/core/version.hpp"
#include "soundwave/dsp/analyzer.hpp"
#include "soundwave/mapping/mappers.hpp"
#include "soundwave/render/interpretation.hpp"
#include "soundwave/render/plots.hpp"
#include "soundwave/mapping/physical_mappers.hpp"
#include "soundwave/physics/medium_filter.hpp"
#include "soundwave/viz/realtime.hpp"

namespace soundwave::cli {
namespace {

namespace fs = std::filesystem;

constexpr const char* kBanner =
    "SoundWave -- laboratorio de mapeamento entre o espectro sonoro e o eletromagnetico";

// Aviso repetido de proposito em toda saida do programa. O projeto inteiro
// depende de o usuario nao esquecer disto.
constexpr const char* kDisclaimer =
    "  Nota: o SoundWave NAO converte som em luz. Ele aplica uma funcao\n"
    "  matematica escolhida por voce entre dois dominios de frequencia sem\n"
    "  relacao fisica entre si. Trocar a funcao troca o resultado.";

// Cobre de ELF (dezenas de Hz) a raios gama.
std::string formatSpectralHz(double hz) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2);
    if (hz >= 1e15) out << (hz / 1e15) << " PHz";
    else if (hz >= 1e12) out << (hz / 1e12) << " THz";
    else if (hz >= 1e9) out << (hz / 1e9) << " GHz";
    else if (hz >= 1e6) out << (hz / 1e6) << " MHz";
    else if (hz >= 1e3) out << (hz / 1e3) << " kHz";
    else out << hz << " Hz";
    return out.str();
}

std::string formatWavelength(double metres) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(1);
    if (metres >= 1e3) out << (metres / 1e3) << " km";
    else if (metres >= 1.0) out << metres << " m";
    else if (metres >= 1e-3) out << (metres * 1e3) << " mm";
    else if (metres >= 1e-6) out << (metres * 1e6) << " um";
    else if (metres >= 1e-9) out << (metres * 1e9) << " nm";
    else out << std::scientific << std::setprecision(2) << metres << " m";
    return out.str();
}

std::string formatThz(double hz) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << (hz / 1e12) << " THz";
    return out.str();
}

// Distancias uteis aqui vao de centimetros a dezenas de milhares de km.
std::string formatDistance(double metres) {
    std::ostringstream out;
    if (!std::isfinite(metres)) return "infinita";
    if (metres >= 1e6) out << std::fixed << std::setprecision(0) << (metres / 1000.0) << " km";
    else if (metres >= 1000.0) out << std::fixed << std::setprecision(1) << (metres / 1000.0) << " km";
    else if (metres >= 1.0) out << std::fixed << std::setprecision(1) << metres << " m";
    else out << std::fixed << std::setprecision(2) << (metres * 100.0) << " cm";
    return out.str();
}

std::string formatLambda(double metres) {
    std::ostringstream out;
    if (metres >= 1.0) out << std::fixed << std::setprecision(2) << metres << " m";
    else if (metres >= 0.01) out << std::fixed << std::setprecision(2) << (metres * 100.0) << " cm";
    else out << std::fixed << std::setprecision(1) << (metres * 1e9) << " nm";
    return out.str();
}

std::string formatNm(double metres) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << (metres * 1e9) << " nm";
    return out.str();
}

// Carrega WAV ou gera um sinal sintetico a partir de uma pseudo-URI
// "gen:sine:440" -- assim os exemplos e testes rodam sem nenhum arquivo.
bool loadInput(const std::string& path, double sampleRate, PcmBuffer& out, std::string& error) {
    if (path.rfind("gen:", 0) == 0) {
        std::vector<std::string> parts;
        std::string token;
        std::istringstream stream(path.substr(4));
        while (std::getline(stream, token, ':')) parts.push_back(token);

        const std::string kind = parts.empty() ? "sine" : parts[0];
        const double rate = sampleRate > 0.0 ? sampleRate : 44100.0;
        const double freq = parts.size() > 1 ? std::stod(parts[1]) : 440.0;
        const double duration = parts.size() > 2 ? std::stod(parts[2]) : 2.0;

        if (kind == "sine") {
            out = signals::sine(freq, duration, rate);
        } else if (kind == "chord") {
            out = signals::chord({freq, freq * 1.25, freq * 1.5}, duration, rate);
        } else if (kind == "harmonics") {
            out = signals::harmonicSeries(freq, 8, duration, rate);
        } else if (kind == "sweep") {
            out = signals::logSweep(20.0, 20000.0, duration, rate);
        } else if (kind == "noise") {
            out = signals::whiteNoise(duration, rate);
        } else if (kind == "silence") {
            out = signals::silence(duration, rate);
        } else {
            error = "gerador desconhecido: " + kind;
            return false;
        }
        out.sourceName = path;
        return true;
    }

    WavLoadResult loaded = loadWavFile(path);
    if (!loaded.ok) {
        error = loaded.error;
        return false;
    }
    out = std::move(loaded.buffer);
    return true;
}

// Resolve a configuracao: arquivo se dado, padroes caso contrario, com as
// sobreposicoes de linha de comando aplicadas por cima. Reporta tudo.
bool resolveConfig(const Args& args, AnalysisConfig& config) {
    if (args.has("config")) {
        ConfigLoadResult loaded = loadConfig(args.get("config"));
        if (!loaded.ok) {
            std::cerr << "erro: " << loaded.error << "\n";
            return false;
        }
        for (const std::string& warning : loaded.warnings) {
            std::cerr << "aviso: " << warning << "\n";
        }
        config = loaded.config;
    }

    if (args.has("mapping")) config.mapping.type = args.get("mapping");
    if (args.has("window")) config.analysis.window = args.get("window");
    if (args.has("mode")) config.interpretation.mode = args.get("mode");
    if (args.has("color-mode")) config.color.mode = args.get("color-mode");
    if (args.has("gamut")) config.color.gamut = args.get("gamut");
    config.analysis.fftSize = args.getSize("fft-size", config.analysis.fftSize);
    config.analysis.hopSize = args.getSize("hop-size", config.analysis.hopSize);
    config.render.width = args.getSize("width", config.render.width);
    config.render.height = args.getSize("height", config.render.height);

    const std::vector<std::string> problems = validateConfig(config);
    if (!problems.empty()) {
        std::cerr << "configuracao invalida:\n";
        for (const std::string& problem : problems) std::cerr << "  - " << problem << "\n";
        return false;
    }
    return true;
}

PlotSettings makePlotSettings(const AnalysisConfig& config) {
    PlotSettings settings;
    settings.width = config.render.width;
    settings.height = config.render.height;
    settings.logFrequencyAxis = config.render.logFrequencyAxis;
    settings.minHz = config.interpretation.analysisMinHz;
    settings.maxHz = config.interpretation.analysisMaxHz;
    return settings;
}

bool savePng(const fs::path& path, const Image& image) {
    std::string error;
    if (!writePng(path.string(), image, &error)) {
        std::cerr << "erro ao gravar " << path << ": " << error << "\n";
        return false;
    }
    std::cout << "  gravado: " << path.string() << " (" << image.width() << "x" << image.height()
              << ")\n";
    return true;
}

}  // namespace

// ---------------------------------------------------------------------------

bool Args::has(const std::string& key) const {
    for (const auto& [name, value] : options) {
        (void)value;
        if (name == key) return true;
    }
    return false;
}

std::string Args::get(const std::string& key, const std::string& fallback) const {
    for (const auto& [name, value] : options) {
        if (name == key) return value;
    }
    return fallback;
}

double Args::getDouble(const std::string& key, double fallback) const {
    if (!has(key)) return fallback;
    try {
        return std::stod(get(key));
    } catch (const std::exception&) {
        std::cerr << "aviso: --" << key << " nao e numero, usando " << fallback << "\n";
        return fallback;
    }
}

std::size_t Args::getSize(const std::string& key, std::size_t fallback) const {
    if (!has(key)) return fallback;
    try {
        return static_cast<std::size_t>(std::stoull(get(key)));
    } catch (const std::exception&) {
        std::cerr << "aviso: --" << key << " nao e inteiro, usando " << fallback << "\n";
        return fallback;
    }
}

Args parseArgs(int argc, char** argv) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        std::string token = argv[i];
        if (token.rfind("--", 0) != 0) {
            args.positional.push_back(token);
            continue;
        }
        token = token.substr(2);
        const auto equals = token.find('=');
        if (equals != std::string::npos) {
            args.options.emplace_back(token.substr(0, equals), token.substr(equals + 1));
        } else if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) {
            args.options.emplace_back(token, argv[++i]);
        } else {
            args.options.emplace_back(token, "true");  // opcao booleana
        }
    }
    return args;
}

// ---------------------------------------------------------------------------

int commandAnalyze(const Args& args) {
    if (args.positional.size() < 2) {
        std::cerr << "uso: soundwave analyze <arquivo.wav | gen:tipo:freq:dur> [opcoes]\n";
        return 2;
    }

    AnalysisConfig config;
    if (!resolveConfig(args, config)) return 2;

    const std::string input = args.positional[1];
    PcmBuffer buffer;
    std::string error;
    if (!loadInput(input, config.analysis.sampleRate, buffer, error)) {
        std::cerr << "erro: " << error << "\n";
        return 1;
    }
    if (buffer.empty()) {
        std::cerr << "erro: entrada sem amostras\n";
        return 1;
    }

    const fs::path outputDir = args.get("out", "out");
    std::error_code ec;
    fs::create_directories(outputDir, ec);
    if (ec) {
        std::cerr << "erro: nao foi possivel criar " << outputDir << ": " << ec.message() << "\n";
        return 1;
    }

    const AnalyzerSettings analyzerSettings = makeAnalyzerSettings(config);
    SpectrumAnalyzer analyzer(analyzerSettings, buffer.sampleRate);
    std::vector<Spectrum> frames = analyzer.analyzeAll(buffer);

    // Filtro de meio: atenua o espectro pela absorcao REAL do meio ao longo de
    // um caminho. Nao desloca frequencia -- so magnitude. Ver docs/physics.md.
    if (config.environment.applyMediumFilter) {
        const MediumFilterSettings filter = makeMediumFilterSettings(config);
        for (Spectrum& spectrum : frames) spectrum = applyAcousticFilter(spectrum, filter);
        std::cout << "filtro de meio : " << filter.acoustic.name << ", "
                  << filter.pathLengthM << " m de caminho acustico\n";
    }

    const std::unique_ptr<FrequencyMapper> mapper = makeMapper(config);
    const ColorEngine engine(makeColorEngineSettings(config));
    const Interpreter interpreter(*mapper, engine, makeInterpreterSettings(config));
    const std::vector<FrameInterpretation> interpretations = interpreter.interpretAll(frames);

    std::cout << kBanner << "\n\n"
              << "entrada        : " << input << "\n"
              << "taxa           : " << buffer.sampleRate << " Hz\n"
              << "duracao        : " << std::fixed << std::setprecision(3)
              << buffer.durationSeconds() << " s (" << buffer.frameCount() << " amostras)\n"
              << "pico / RMS     : " << buffer.peakAmplitude() << " / " << buffer.rmsAmplitude()
              << "\n"
              << "FFT            : " << analyzerSettings.fftSize << " pontos, salto "
              << analyzerSettings.hopSize << ", janela "
              << windowTypeName(analyzerSettings.window) << "\n"
              << "resolucao      : " << analyzer.resolutionHz() << " Hz por bin\n"
              << "quadros        : " << frames.size() << "\n"
              << "interpretacao  : " << config.interpretation.mode << "\n\n"
              << "mapeamento     : " << mapper->name() << "\n"
              << "  " << mapper->describe() << "\n\n";

    const PlotSettings plot = makePlotSettings(config);
    bool allSaved = true;
    allSaved &= savePng(outputDir / "spectrogram.png",
                        renderSpectrogram(frames, *mapper, engine, plot));
    allSaved &= savePng(outputDir / "timeline.png", renderColorTimeline(interpretations, plot));
    allSaved &= savePng(outputDir / "mapping_ruler.png",
                        renderMappingRuler(*mapper, engine, plot));
    if (!frames.empty()) {
        // Quadro do meio: mais representativo que o primeiro, que costuma cair
        // no ataque ou no silencio inicial.
        allSaved &= savePng(outputDir / "spectrum.png",
                            renderSpectrumPlot(frames[frames.size() / 2], *mapper, engine, plot));
    }

    // CSV por quadro: o dado bruto por tras das imagens. Sem ele o resultado nao
    // e analisavel, so observavel.
    const fs::path csvPath = outputDir / "frames.csv";
    std::ofstream csv(csvPath);
    if (csv) {
        csv << "time_s,dominant_hz,dominant_magnitude,total_magnitude,em_hz,wavelength_nm,"
               "band,is_false_colour,out_of_gamut,r,g,b,silent\n";
        for (const FrameInterpretation& frame : interpretations) {
            csv << std::setprecision(9) << frame.timeOffsetSeconds << ','
                << frame.dominantSourceHz << ',' << frame.dominantMagnitude << ','
                << frame.totalMagnitude << ',' << frame.colour.emFrequencyHz << ','
                << (frame.colour.wavelengthM * 1e9) << ',' << emBandName(frame.colour.band) << ','
                << (frame.colour.isFalseColour ? 1 : 0) << ','
                << (frame.colour.wasOutOfGamut ? 1 : 0) << ','
                << static_cast<int>(frame.colour.rgb.r) << ','
                << static_cast<int>(frame.colour.rgb.g) << ','
                << static_cast<int>(frame.colour.rgb.b) << ',' << (frame.silent ? 1 : 0) << '\n';
        }
        std::cout << "  gravado: " << csvPath.string() << "\n";
    } else {
        std::cerr << "erro ao gravar " << csvPath << "\n";
        allSaved = false;
    }

    RunManifest manifest;
    manifest.soundwaveVersion = versionString();
    manifest.inputPath = input;
    manifest.inputFingerprint = fingerprintSamples(buffer.samples);
    manifest.inputSampleRate = buffer.sampleRate;
    manifest.inputSamples = buffer.frameCount();
    manifest.mapperDescription = mapper->describe();
    manifest.config = config;
    // Frequencia de referencia do orcamento: a dominante do quadro do meio, que
    // e mais representativa do sinal do que um valor fixo.
    const double representativeHz =
        interpretations.empty() ? 440.0
                                : std::max(20.0, interpretations[interpretations.size() / 2]
                                                     .dominantSourceHz);
    manifest.fidelity =
        pipelineFidelity(config, *mapper, analyzer.resolutionHz(), representativeHz);
    manifest.config.analysis.sampleRate = buffer.sampleRate;  // taxa efetiva, nao a pedida

    std::string manifestError;
    const fs::path manifestPath = outputDir / "manifest.yaml";
    if (saveManifest(manifestPath.string(), manifest, &manifestError)) {
        std::cout << "  gravado: " << manifestPath.string() << "\n";
    } else {
        std::cerr << "erro: " << manifestError << "\n";
        allSaved = false;
    }

    std::cout << "\n" << manifest.fidelity.report() << "\n" << kDisclaimer << "\n";
    return allSaved ? 0 : 1;
}

int commandCompare(const Args& args) {
    if (args.positional.size() < 2) {
        std::cerr << "uso: soundwave compare <arquivo.wav | gen:tipo:freq:dur> [--out dir]\n";
        return 2;
    }

    AnalysisConfig config;
    if (!resolveConfig(args, config)) return 2;

    PcmBuffer buffer;
    std::string error;
    if (!loadInput(args.positional[1], config.analysis.sampleRate, buffer, error)) {
        std::cerr << "erro: " << error << "\n";
        return 1;
    }

    const fs::path outputDir = args.get("out", "out/compare");
    std::error_code ec;
    fs::create_directories(outputDir, ec);

    SpectrumAnalyzer analyzer(makeAnalyzerSettings(config), buffer.sampleRate);
    const std::vector<Spectrum> frames = analyzer.analyzeAll(buffer);
    const ColorEngine engine(makeColorEngineSettings(config));
    const PlotSettings plot = makePlotSettings(config);

    std::cout << kBanner << "\n\n"
              << "Mesmo sinal, mesma FFT, mesma conversao de cor -- so o mapeamento muda.\n"
              << "Se as quatro saidas forem muito diferentes entre si, isso e a prova\n"
              << "visual de que a cor vem da funcao escolhida, nao do som.\n\n";

    bool ok = true;
    for (const char* type : {"linear", "logarithmic", "octave"}) {
        AnalysisConfig variant = config;
        variant.mapping.type = type;
        const std::unique_ptr<FrequencyMapper> mapper = makeMapper(variant);
        const Interpreter interpreter(*mapper, engine, makeInterpreterSettings(variant));

        std::cout << type << ":\n  " << mapper->describe() << "\n";
        ok &= savePng(outputDir / (std::string(type) + "_ruler.png"),
                      renderMappingRuler(*mapper, engine, plot));
        ok &= savePng(outputDir / (std::string(type) + "_timeline.png"),
                      renderColorTimeline(interpreter.interpretAll(frames), plot));
        ok &= savePng(outputDir / (std::string(type) + "_spectrogram.png"),
                      renderSpectrogram(frames, *mapper, engine, plot));
        std::cout << "\n";
    }

    std::cout << kDisclaimer << "\n";
    return ok ? 0 : 1;
}

int commandMap(const Args& args) {
    if (args.positional.size() < 2) {
        std::cerr << "uso: soundwave map <frequencia_hz> [--config arquivo]\n";
        return 2;
    }

    double frequency = 0.0;
    try {
        frequency = std::stod(args.positional[1]);
    } catch (const std::exception&) {
        std::cerr << "erro: frequencia invalida: " << args.positional[1] << "\n";
        return 2;
    }

    AnalysisConfig config;
    if (!resolveConfig(args, config)) return 2;
    const ColorEngine engine(makeColorEngineSettings(config));

    std::cout << kBanner << "\n\n"
              << "f_som = " << frequency << " Hz, sob cada mapeamento:\n\n"
              << std::left << std::setw(14) << "mapeamento" << std::setw(16) << "f_EM"
              << std::setw(14) << "lambda" << std::setw(14) << "banda" << std::setw(18) << "sRGB"
              << "fora do gamut\n"
              << std::string(92, '-') << "\n";

    // Inclui os dois mapeadores fisicos. Eles precisam de extrapolate: prender
    // nas bordas do visivel esconderia justamente o resultado deles, que e cair
    // fora do visivel.
    for (const char* type : {"linear", "logarithmic", "octave", "identity", "scale"}) {
        AnalysisConfig variant = config;
        variant.mapping.type = type;
        if (std::string(type) == "identity" || std::string(type) == "scale") {
            variant.mapping.outOfRange = "extrapolate";
        }
        const std::unique_ptr<FrequencyMapper> mapper = makeMapper(variant);
        const double em = mapper->map(frequency);
        const ColorResult colour = engine.fromEmFrequency(em);

        std::ostringstream rgb;
        rgb << "(" << static_cast<int>(colour.rgb.r) << "," << static_cast<int>(colour.rgb.g)
            << "," << static_cast<int>(colour.rgb.b) << ")";

        std::cout << std::left << std::setw(14) << type << std::setw(16) << formatSpectralHz(em)
                  << std::setw(14) << formatWavelength(colour.wavelengthM) << std::setw(14)
                  << emBandName(colour.band) << std::setw(18) << rgb.str()
                  << (colour.wasOutOfGamut ? "sim" : "nao") << "\n";
    }
    std::cout << "\n  identity e scale sao as hipoteses NULAS: a primeira preserva energia\n"
              << "  por quantum (E = h*f) e joga tudo em radio; a segunda fixa a forma da\n"
              << "  funcao pela fisica e deixa so a escala livre -- e manda quase tudo para\n"
              << "  fora do visivel. Ver docs/physics.md.\n";

    // A demonstracao mais direta da limitacao discutida em docs/mapping.md.
    std::cout << "\nA mesma nota uma oitava acima (" << frequency * 2.0 << " Hz):\n";
    for (const char* type : {"logarithmic", "octave"}) {
        AnalysisConfig variant = config;
        variant.mapping.type = type;
        const std::unique_ptr<FrequencyMapper> mapper = makeMapper(variant);
        const ColorResult low = engine.fromEmFrequency(mapper->map(frequency));
        const ColorResult high = engine.fromEmFrequency(mapper->map(frequency * 2.0));
        const bool same = low.rgb.r == high.rgb.r && low.rgb.g == high.rgb.g &&
                          low.rgb.b == high.rgb.b;
        std::cout << "  " << std::left << std::setw(14) << type << "(" << static_cast<int>(low.rgb.r)
                  << "," << static_cast<int>(low.rgb.g) << "," << static_cast<int>(low.rgb.b)
                  << ") -> (" << static_cast<int>(high.rgb.r) << ","
                  << static_cast<int>(high.rgb.g) << "," << static_cast<int>(high.rgb.b) << ")"
                  << (same ? "   [mesma cor: equivalencia de oitava preservada]"
                           : "   [cor diferente: equivalencia de oitava perdida]")
                  << "\n";
    }

    std::cout << "\n" << kDisclaimer << "\n";
    return 0;
}

int commandGenerate(const Args& args) {
    if (args.positional.size() < 3) {
        std::cerr << "uso: soundwave gen <sine|chord|harmonics|sweep|noise|silence> <saida.wav> "
                     "[--freq 440] [--duration 2] [--rate 44100]\n";
        return 2;
    }

    const std::string kind = args.positional[1];
    const std::string output = args.positional[2];
    const double frequency = args.getDouble("freq", 440.0);
    const double duration = args.getDouble("duration", 2.0);
    const double rate = args.getDouble("rate", 44100.0);

    PcmBuffer buffer;
    if (kind == "sine") {
        buffer = signals::sine(frequency, duration, rate);
    } else if (kind == "chord") {
        buffer = signals::chord({frequency, frequency * 1.25, frequency * 1.5}, duration, rate);
    } else if (kind == "harmonics") {
        buffer = signals::harmonicSeries(frequency, args.getSize("harmonics", 8), duration, rate);
    } else if (kind == "sweep") {
        buffer = signals::logSweep(args.getDouble("start", 20.0), args.getDouble("end", 20000.0),
                                   duration, rate);
    } else if (kind == "noise") {
        buffer = signals::whiteNoise(duration, rate);
    } else if (kind == "silence") {
        buffer = signals::silence(duration, rate);
    } else {
        std::cerr << "erro: gerador desconhecido: " << kind << "\n";
        return 2;
    }

    const WavWriteResult result = writeWav16(output, buffer);
    if (!result.ok) {
        std::cerr << "erro: " << result.error << "\n";
        return 1;
    }
    if (result.clippedSamples > 0) {
        std::cerr << "aviso: " << result.clippedSamples << " amostras ceifadas\n";
    }
    std::cout << "gravado " << output << ": " << kind << ", " << buffer.frameCount()
              << " amostras a " << rate << " Hz\n";
    return 0;
}

int commandConfig(const Args& args) {
    const AnalysisConfig config;
    if (args.positional.size() < 2) {
        std::cout << configToYaml(config);
        return 0;
    }
    std::string error;
    if (!saveConfig(args.positional[1], config, &error)) {
        std::cerr << "erro: " << error << "\n";
        return 1;
    }
    std::cout << "configuracao padrao gravada em " << args.positional[1] << "\n";
    return 0;
}

int commandInfo(const Args&) {
    const MappingDomain domain;
    std::cout << kBanner << "\n\n"
              << "versao         : " << versionString() << " (algoritmo v" << kAlgorithmVersion
              << ")\n\n"
              << "Dominios:\n"
              << "  audivel      : " << domain.sourceMinHz << " a " << domain.sourceMaxHz
              << " Hz  = " << std::fixed << std::setprecision(2) << domain.sourceOctaves()
              << " oitavas\n"
              << "  visivel      : " << formatThz(domain.targetMinHz) << " a "
              << formatThz(domain.targetMaxHz) << " = " << domain.targetOctaves() << " oitava\n"
              << "  compressao   : " << domain.compressionRatio() << "x\n\n"
              << "  Consequencia: qualquer mapeamento monotonico espreme ~"
              << domain.sourceOctaves() << " oitavas sonoras em menos de uma oitava de luz.\n"
              << "  Uma oitava musical vira um deslocamento de "
              << ((std::pow(2.0, 1.0 / domain.compressionRatio()) - 1.0) * 100.0)
              << "% em f_EM -- quase imperceptivel.\n"
              << "  Por isso o mapeamento logaritmico NAO preserva equivalencia de oitava,\n"
              << "  e o mapeamento por classe de altura (octave) preserva ao custo da\n"
              << "  monotonicidade. Ver docs/mapping.md.\n\n"
              << kDisclaimer << "\n";
    return 0;
}

int commandMedium(const Args& args) {
    AnalysisConfig config;
    if (!resolveConfig(args, config)) return 2;
    const Conditions conditions = makeConditions(config);

    const std::string name = args.positional.size() > 1 ? args.positional[1] : "sea-water";
    const auto medium = media::byName(name);
    if (!medium) {
        std::cerr << "erro: meio desconhecido: " << name << "\nmeios: ";
        for (const Medium& m : media::all()) std::cerr << m.name << " ";
        std::cerr << "\n";
        return 2;
    }
    const double pathM = args.getDouble("path", 10.0);  // usado no orcamento de fidelidade

    std::cout << kBanner << "\n\n"
              << "MEIO: " << medium->name << "\n"
              << "  " << medium->description << "\n"
              << "  " << std::fixed << std::setprecision(1) << conditions.temperatureC
              << " C, " << conditions.salinityPpt << " ppt, " << conditions.depthM << " m, pH "
              << conditions.pH << ", " << conditions.relativeHumidity << "% UR\n\n";

    // ---- 1. A frequencia NAO muda; o comprimento de onda muda ----------------
    std::cout << "1. O MEIO NAO MUDA A FREQUENCIA -- MUDA O COMPRIMENTO DE ONDA\n"
              << std::string(74, '=') << "\n"
              << "   A continuidade de fase na interface forca a frequencia transmitida a\n"
              << "   ser igual a incidente. Por isso um objeto vermelho continua vermelho\n"
              << "   debaixo d'agua, embora lambda encolha. Cor segue FREQUENCIA.\n\n"
              << "   " << std::left << std::setw(14) << "meio" << std::setw(13) << "v_som (m/s)"
              << std::setw(15) << "lambda@20Hz" << std::setw(15) << "lambda@20kHz" << "razao\n"
              << "   " << std::string(70, '-') << "\n";
    for (const Medium& m : media::all()) {
        if (!m.carriesSound()) continue;
        const RangeSpan span = acousticSpan(m, 20.0, 20000.0, conditions);
        std::cout << "   " << std::left << std::setw(14) << m.name << std::setw(13)
                  << std::setprecision(0) << soundSpeedMs(m, conditions) << std::setw(15)
                  << formatLambda(span.atLowFrequencyM) << std::setw(15)
                  << formatLambda(span.atHighFrequencyM) << std::setprecision(1) << span.ratio
                  << "\n";
    }
    std::cout << "\n   " << std::left << std::setw(14) << "meio" << std::setw(13) << "n(550nm)"
              << std::setw(15) << "lambda@400nm" << std::setw(15) << "lambda@750nm" << "razao\n"
              << "   " << std::string(70, '-') << "\n";
    for (const Medium& m : media::all()) {
        const RangeSpan span = opticalSpan(m, 400.0, 750.0);
        std::cout << "   " << std::left << std::setw(14) << m.name << std::setw(13)
                  << std::setprecision(4) << refractiveIndexAt(m, 550.0) << std::setw(15)
                  << formatLambda(span.atHighFrequencyM) << std::setw(15)
                  << formatLambda(span.atLowFrequencyM) << std::setprecision(4) << span.ratio
                  << "\n";
    }
    std::cout << "\n   >>> A razao NAO muda com o meio: v e n cancelam. A compressao de\n"
              << "       ~11x entre audivel e visivel e INVARIANTE. So a dispersao a\n"
              << "       altera, e apenas em ~1%. Ver docs/physics.md.\n\n";

    // ---- 2. Os dois filtros -------------------------------------------------
    MediumFilterSettings filter;
    filter.acoustic = *medium;
    filter.optical = *medium;
    filter.conditions = conditions;
    filter.pathLengthM = pathM;

    std::cout << "2. O MEIO FILTRA -- E OS DOIS FILTROS SAO ANTAGONICOS\n"
              << std::string(74, '=') << "\n"
              << "   A grandeza comparavel aqui NAO e a transmitancia a uma distancia fixa:\n"
              << "   som e luz tem escalas caracteristicas separadas por ordens de grandeza.\n"
              << "   Usamos a DISTANCIA DE MEIA POTENCIA (queda de 3 dB), que e livre de escala.\n\n"
              << "   SOM                                      LUZ\n"
              << "   " << std::left << std::setw(9) << "f (Hz)" << std::setw(12) << "dB/m"
              << std::setw(16) << "meia potencia" << "  " << std::setw(7) << "nm"
              << std::setw(12) << "dB/m" << "meia potencia\n"
              << "   " << std::string(72, '-') << "\n";

    const std::vector<double> freqs = {20, 100, 440, 2000, 8000, 20000};
    const std::vector<double> nms = {400, 450, 500, 550, 650, 750};
    const std::vector<FilterPoint> acoustic = acousticProfile(filter, freqs);
    const std::vector<FilterPoint> optical = opticalProfile(filter, nms);
    for (std::size_t i = 0; i < acoustic.size(); ++i) {
        std::cout << "   " << std::left << std::setw(9) << std::setprecision(0) << std::fixed
                  << acoustic[i].frequencyHz << std::setw(12) << std::scientific
                  << std::setprecision(2) << acoustic[i].attenuationDbPerM << std::setw(16)
                  << formatDistance(acoustic[i].halfDistanceM) << "  " << std::setw(7)
                  << std::fixed << std::setprecision(0) << optical[i].wavelengthNm
                  << std::setw(12) << std::scientific << std::setprecision(2)
                  << optical[i].attenuationDbPerM << formatDistance(optical[i].halfDistanceM)
                  << "\n";
    }

    const double bestHz = mostTransmittedAudibleHz(*medium, 20.0, 20000.0, conditions);
    const double bestNm = mostTransmittedVisibleNm(*medium);
    std::cout << "\n   sobrevive melhor:  som " << std::fixed << std::setprecision(1) << bestHz
              << " Hz (GRAVE)   |   luz " << std::setprecision(0) << bestNm << " nm (AZUL)\n\n";

    if (medium->opticalAbsorption != OpticalAbsorptionModel::NotModelled &&
        medium->soundAbsorption != SoundAbsorptionModel::NotModelled) {
        const LogMapper reference;
        const double nmOfBestSound = kSpeedOfLight / reference.map(bestHz) * 1e9;
        std::cout << "   >>> ANTAGONISMO: o meio preserva os GRAVES do som e os AZUIS da luz.\n"
                  << "       Mas todo mapeamento monotonico leva grave em VERMELHO -- o\n"
                  << "       mapeamento logaritmico poe " << std::fixed << std::setprecision(1)
                  << bestHz << " Hz em " << std::setprecision(0) << nmOfBestSound << " nm.\n"
                  << "       O meio destroi exatamente o que o mapeamento preservou.\n"
                  << "       'O mesmo ambiente para os dois' nao da equivalencia: da conflito.\n\n";
    }

    // ---- 3. Fidelidade ------------------------------------------------------
    FidelityBudget budget;
    budget.add(soundSpeedClaim(*medium));
    budget.add(soundAbsorptionClaim(*medium));
    budget.add(refractiveIndexClaim(*medium));
    budget.add(opticalAbsorptionClaim(*medium, 550.0));
    std::cout << "3. " << budget.report() << "\n" << kDisclaimer << "\n";
    return 0;
}

int commandFidelity(const Args& args) {
    AnalysisConfig config;
    if (!resolveConfig(args, config)) return 2;

    const std::unique_ptr<FrequencyMapper> mapper = makeMapper(config);
    const double sampleRate =
        config.analysis.sampleRate > 0.0 ? config.analysis.sampleRate : 44100.0;
    const double resolution = sampleRate / static_cast<double>(config.analysis.fftSize);
    const double representative = args.getDouble("hz", 440.0);

    std::cout << kBanner << "\n\n"
              << "Configuracao: mapeamento " << config.mapping.type << ", FFT "
              << config.analysis.fftSize << " a " << sampleRate << " Hz\n"
              << "Grandeza de referencia: " << representative << " Hz\n\n"
              << pipelineFidelity(config, *mapper, resolution, representative).report() << "\n"
              << kDisclaimer << "\n";
    return 0;
}

int commandLive(const Args& args) {
    if (args.positional.size() < 2) {
        std::cerr << "uso: soundwave live <arquivo.wav | gen:tipo:freq:dur> [--width] [--height]\n";
        return 2;
    }

    AnalysisConfig config;
    if (!resolveConfig(args, config)) return 2;

    PcmBuffer buffer;
    std::string error;
    if (!loadInput(args.positional[1], config.analysis.sampleRate, buffer, error)) {
        std::cerr << "erro: " << error << "\n";
        return 1;
    }

    viz::RealtimeOptions options;
    options.windowWidth = args.getSize("width", options.windowWidth);
    options.windowHeight = args.getSize("height", options.windowHeight);
    options.playAudio = !args.has("no-audio");
    options.loop = !args.has("no-loop");

    if (viz::isAvailable()) {
        std::cout << kBanner << "\n\n"
                  << "mapeamento: " << config.mapping.type << "\n"
                  << "ESC ou Q para sair.\n\n"
                  << kDisclaimer << "\n\n";
    }
    return viz::run(buffer, config, options);
}

void printUsage() {
    std::cout
        << kBanner << "\n\n"
        << "uso: soundwave <comando> [argumentos]\n\n"
        << "comandos:\n"
        << "  analyze <entrada> [--out dir] [--config f.yaml] [--mapping tipo] [--mode m]\n"
        << "      Pipeline completo: FFT -> mapeamento -> cor -> imagens + CSV + manifesto.\n"
        << "  compare <entrada> [--out dir]\n"
        << "      Mesmo sinal sob linear, logaritmico e oitava, lado a lado.\n"
        << "  map <frequencia_hz>\n"
        << "      Mostra a cadeia f_som -> f_EM -> lambda -> banda -> sRGB para cada mapeamento.\n"
        << "  gen <tipo> <saida.wav> [--freq] [--duration] [--rate]\n"
        << "      Gera sinais de teste deterministas.\n"
        << "  config [saida.yaml]\n"
        << "      Imprime ou grava a configuracao padrao comentada.\n"
        << "  medium [meio] [--path metros]\n"
        << "      Propriedades reais do meio, a invariancia e os dois filtros antagonicos.\n"
        << "      meios: vacuum air fresh-water sea-water ice fused-silica\n"
        << "  fidelity [--hz 440] [--config f.yaml]\n"
        << "      Orcamento de fidelidade: natureza e incerteza de CADA etapa do pipeline.\n"
        << "  info\n"
        << "      Constantes, dominios e a assimetria audivel/visivel.\n"
        << "  live <entrada>\n"
        << "      Visualizacao em tempo real (exige SDL2 na compilacao).\n\n"
        << "entrada pode ser um arquivo .wav ou um sinal sintetico:\n"
        << "  gen:sine:440:2   gen:chord:220:3   gen:harmonics:110:2\n"
        << "  gen:sweep:0:5    gen:noise:0:2     gen:silence:0:1\n\n"
        << kDisclaimer << "\n";
}

}  // namespace soundwave::cli
