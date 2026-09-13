#include "commands.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>

#include "soundwave/audio/signal_generator.hpp"
#include "soundwave/audio/wav_reader.hpp"
#include "soundwave/core/config.hpp"
#include "soundwave/core/version.hpp"
#include "soundwave/dsp/analyzer.hpp"
#include "soundwave/mapping/mappers.hpp"
#include "soundwave/render/interpretation.hpp"
#include "soundwave/render/plots.hpp"
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

std::string formatThz(double hz) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << (hz / 1e12) << " THz";
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
    const std::vector<Spectrum> frames = analyzer.analyzeAll(buffer);

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
    manifest.config.analysis.sampleRate = buffer.sampleRate;  // taxa efetiva, nao a pedida

    std::string manifestError;
    const fs::path manifestPath = outputDir / "manifest.yaml";
    if (saveManifest(manifestPath.string(), manifest, &manifestError)) {
        std::cout << "  gravado: " << manifestPath.string() << "\n";
    } else {
        std::cerr << "erro: " << manifestError << "\n";
        allSaved = false;
    }

    std::cout << "\n" << kDisclaimer << "\n";
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
              << std::left << std::setw(14) << "mapeamento" << std::setw(14) << "f_EM"
              << std::setw(12) << "lambda" << std::setw(14) << "banda" << std::setw(18) << "sRGB"
              << "fora do gamut\n"
              << std::string(78, '-') << "\n";

    for (const char* type : {"linear", "logarithmic", "octave"}) {
        AnalysisConfig variant = config;
        variant.mapping.type = type;
        const std::unique_ptr<FrequencyMapper> mapper = makeMapper(variant);
        const double em = mapper->map(frequency);
        const ColorResult colour = engine.fromEmFrequency(em);

        std::ostringstream rgb;
        rgb << "(" << static_cast<int>(colour.rgb.r) << "," << static_cast<int>(colour.rgb.g)
            << "," << static_cast<int>(colour.rgb.b) << ")";

        std::cout << std::left << std::setw(14) << type << std::setw(14) << formatThz(em)
                  << std::setw(12) << formatNm(colour.wavelengthM) << std::setw(14)
                  << emBandName(colour.band) << std::setw(18) << rgb.str()
                  << (colour.wasOutOfGamut ? "sim" : "nao") << "\n";
    }

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
