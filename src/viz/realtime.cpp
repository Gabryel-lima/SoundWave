#include "soundwave/viz/realtime.hpp"

#include <iostream>

#ifdef SOUNDWAVE_WITH_SDL2

#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <memory>
#include <vector>

#include "soundwave/dsp/analyzer.hpp"
#include "soundwave/render/interpretation.hpp"

namespace soundwave::viz {
namespace {

// Converte o buffer double para PCM 16 bits, que e o que a SDL enfileira.
std::vector<Sint16> toPcm16(const std::vector<double>& samples) {
    std::vector<Sint16> pcm(samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i) {
        pcm[i] = static_cast<Sint16>(std::lround(std::clamp(samples[i], -1.0, 1.0) * 32767.0));
    }
    return pcm;
}

struct SdlContext {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_AudioDeviceID audio = 0;

    ~SdlContext() {
        if (audio != 0) SDL_CloseAudioDevice(audio);
        if (renderer != nullptr) SDL_DestroyRenderer(renderer);
        if (window != nullptr) SDL_DestroyWindow(window);
        SDL_Quit();
    }
};

void setColour(SDL_Renderer* renderer, Rgb colour, Uint8 alpha = 255) {
    SDL_SetRenderDrawColor(renderer, colour.r, colour.g, colour.b, alpha);
}

}  // namespace

bool isAvailable() { return true; }

int run(const PcmBuffer& buffer, const AnalysisConfig& config, const RealtimeOptions& options) {
    if (buffer.empty()) {
        std::cerr << "erro: entrada sem amostras\n";
        return 1;
    }

    const Uint32 subsystems = SDL_INIT_VIDEO | (options.playAudio ? SDL_INIT_AUDIO : 0U);
    if (SDL_Init(subsystems) != 0) {
        std::cerr << "erro: SDL_Init falhou: " << SDL_GetError() << "\n";
        return 1;
    }

    SdlContext context;
    context.window = SDL_CreateWindow(
        options.title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        static_cast<int>(options.windowWidth), static_cast<int>(options.windowHeight),
        SDL_WINDOW_SHOWN);
    if (context.window == nullptr) {
        std::cerr << "erro: SDL_CreateWindow falhou: " << SDL_GetError() << "\n";
        return 1;
    }

    context.renderer = SDL_CreateRenderer(
        context.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (context.renderer == nullptr) {
        std::cerr << "erro: SDL_CreateRenderer falhou: " << SDL_GetError() << "\n";
        return 1;
    }
    SDL_SetRenderDrawBlendMode(context.renderer, SDL_BLENDMODE_BLEND);

    std::vector<Sint16> pcm;
    if (options.playAudio) {
        SDL_AudioSpec desired{};
        desired.freq = static_cast<int>(buffer.sampleRate);
        desired.format = AUDIO_S16SYS;
        desired.channels = 1;
        desired.samples = 1024;
        desired.callback = nullptr;  // modo de fila: SDL_QueueAudio

        SDL_AudioSpec obtained{};
        context.audio = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
        if (context.audio == 0) {
            std::cerr << "aviso: audio indisponivel (" << SDL_GetError()
                      << "); seguindo apenas com a visualizacao\n";
        } else {
            pcm = toPcm16(buffer.samples);
            SDL_PauseAudioDevice(context.audio, 0);
        }
    }

    const AnalyzerSettings analyzerSettings = makeAnalyzerSettings(config);
    const SpectrumAnalyzer analyzer(analyzerSettings, buffer.sampleRate);
    const std::unique_ptr<FrequencyMapper> mapper = makeMapper(config);
    const ColorEngine engine(makeColorEngineSettings(config));
    const Interpreter interpreter(*mapper, engine, makeInterpreterSettings(config));

    const std::size_t width = options.windowWidth;
    const std::size_t height = options.windowHeight;
    const std::size_t barCount = std::min<std::size_t>(128, width / 6);

    // Rastro do espectrograma: uma coluna de cor por quadro, rolando a esquerda.
    std::deque<Rgb> trail;

    std::size_t playhead = 0;
    std::size_t queuedTotal = 0;
    bool running = true;
    Uint64 lastTicks = SDL_GetPerformanceCounter();

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN) {
                const SDL_Keycode key = event.key.keysym.sym;
                if (key == SDLK_ESCAPE || key == SDLK_q) running = false;
            }
        }
        if (!running) break;

        // Mantem ~300 ms enfileirados. Menos que isso engasga; muito mais
        // aumenta a latencia entre o que se ouve e o que se ve.
        if (context.audio != 0 && !pcm.empty()) {
            const std::size_t targetQueue = static_cast<std::size_t>(buffer.sampleRate * 0.3);
            while (queuedTotal < pcm.size() &&
                   SDL_GetQueuedAudioSize(context.audio) / sizeof(Sint16) < targetQueue) {
                const std::size_t chunk = std::min<std::size_t>(4096, pcm.size() - queuedTotal);
                SDL_QueueAudio(context.audio, pcm.data() + queuedTotal,
                               static_cast<Uint32>(chunk * sizeof(Sint16)));
                queuedTotal += chunk;
            }
            // A posicao de leitura segue o que JA SAIU do dispositivo, nao o que
            // foi enfileirado. Usar o total enfileirado adiantaria a imagem em
            // ~300 ms em relacao ao som.
            const std::size_t pending = SDL_GetQueuedAudioSize(context.audio) / sizeof(Sint16);
            playhead = queuedTotal > pending ? queuedTotal - pending : 0;
        } else {
            const Uint64 now = SDL_GetPerformanceCounter();
            const double delta =
                static_cast<double>(now - lastTicks) / static_cast<double>(SDL_GetPerformanceFrequency());
            lastTicks = now;
            playhead += static_cast<std::size_t>(delta * buffer.sampleRate);
        }

        if (playhead >= buffer.samples.size()) {
            if (!options.loop) break;
            playhead = 0;
            queuedTotal = 0;
            trail.clear();
            if (context.audio != 0) SDL_ClearQueuedAudio(context.audio);
            continue;
        }

        const Spectrum spectrum = analyzer.analyzeBlock(buffer.samples, playhead);
        const FrameInterpretation frame = interpreter.interpret(spectrum);

        trail.push_back(frame.silent ? Rgb{10, 10, 14} : frame.colour.rgb);
        while (trail.size() > width) trail.pop_front();

        SDL_SetRenderDrawColor(context.renderer, 10, 10, 14, 255);
        SDL_RenderClear(context.renderer);

        // 1) Rastro de cor dominante ao longo do tempo, no topo.
        const int trailHeight = static_cast<int>(height / 10);
        for (std::size_t i = 0; i < trail.size(); ++i) {
            setColour(context.renderer, trail[i]);
            SDL_Rect column{static_cast<int>(i), 0, 1, trailHeight};
            SDL_RenderFillRect(context.renderer, &column);
        }

        // 2) Barras de frequencia, agrupadas em bandas logaritmicas.
        const int barsTop = trailHeight + 10;
        const int barsHeight = static_cast<int>(height / 3);
        const double minHz = config.interpretation.analysisMinHz;
        const double maxHz = config.interpretation.analysisMaxHz;

        double peak = 1e-9;
        for (const SpectralBin& bin : spectrum.bins) peak = std::max(peak, bin.magnitude);

        for (std::size_t b = 0; b < barCount; ++b) {
            const double t0 = static_cast<double>(b) / static_cast<double>(barCount);
            const double t1 = static_cast<double>(b + 1) / static_cast<double>(barCount);
            const double lowHz = minHz * std::pow(maxHz / minHz, t0);
            const double highHz = minHz * std::pow(maxHz / minHz, t1);

            double magnitude = 0.0;
            for (const SpectralBin& bin : spectrum.bins) {
                if (bin.frequencyHz >= lowHz && bin.frequencyHz < highHz) {
                    magnitude = std::max(magnitude, bin.magnitude);
                }
            }
            if (magnitude <= 0.0) continue;

            // Altura em dB: em escala linear quase todas as barras ficariam
            // rentes ao chao, porque o espectro musical decai muito rapido.
            const double db = 20.0 * std::log10(magnitude / peak);
            const double level = std::clamp(1.0 + db / 60.0, 0.0, 1.0);

            const double centreHz = std::sqrt(lowHz * highHz);  // media geometrica
            const double em = mapper->map(centreHz);
            const Rgb colour = std::isnan(em) ? Rgb{50, 50, 50} : engine.fromEmFrequency(em).rgb;

            const int barWidth = std::max(1, static_cast<int>(width / barCount) - 2);
            const int barHeight = static_cast<int>(level * barsHeight);
            setColour(context.renderer, colour, 235);
            SDL_Rect bar{static_cast<int>(b * width / barCount), barsTop + barsHeight - barHeight,
                         barWidth, barHeight};
            SDL_RenderFillRect(context.renderer, &bar);
        }

        // 3) Forma de onda, tingida pela cor do quadro.
        const int waveCentre = static_cast<int>(height * 3 / 4);
        const int waveAmplitude = static_cast<int>(height / 5);
        const Rgb waveColour = frame.silent ? Rgb{60, 60, 70} : frame.colour.rgb;
        setColour(context.renderer, waveColour, 220);

        const std::size_t span = std::min<std::size_t>(analyzerSettings.fftSize,
                                                       buffer.samples.size() - playhead);
        int previousX = 0;
        int previousY = waveCentre;
        for (std::size_t x = 0; x < width; ++x) {
            const std::size_t index = playhead + x * span / width;
            if (index >= buffer.samples.size()) break;
            const int y = waveCentre - static_cast<int>(buffer.samples[index] * waveAmplitude);
            if (x > 0) {
                SDL_RenderDrawLine(context.renderer, previousX, previousY, static_cast<int>(x), y);
            }
            previousX = static_cast<int>(x);
            previousY = y;
        }

        SDL_RenderPresent(context.renderer);
    }
    return 0;
}

}  // namespace soundwave::viz

#else  // SOUNDWAVE_WITH_SDL2

namespace soundwave::viz {

bool isAvailable() { return false; }

int run(const PcmBuffer&, const AnalysisConfig&, const RealtimeOptions&) {
    std::cerr
        << "erro: este binario foi compilado sem SDL2, entao `live` esta indisponivel.\n\n"
           "  Instale o SDL2 e recompile:\n"
           "    Debian/Ubuntu : sudo apt-get install libsdl2-dev\n"
           "    Fedora        : sudo dnf install SDL2-devel\n"
           "    macOS         : brew install sdl2\n"
           "    depois        : cmake -S . -B build && cmake --build build\n\n"
           "  Enquanto isso, `soundwave analyze` faz o pipeline completo sem SDL2 --\n"
           "  e e o caminho recomendado pelo plano, que so coloca tempo real depois\n"
           "  da validacao matematica.\n";
    return 1;
}

}  // namespace soundwave::viz

#endif  // SOUNDWAVE_WITH_SDL2
