#include "soundwave/render/plots.hpp"

#include <algorithm>
#include <cmath>

namespace soundwave {
namespace {

// Posicao vertical (0 = topo) de uma frequencia no eixo configurado.
double axisPosition(double hz, const PlotSettings& settings) {
    if (settings.maxHz <= settings.minHz) return 0.0;
    double t = 0.0;
    if (settings.logFrequencyAxis) {
        if (hz <= 0.0) return 1.0;
        t = (std::log(hz) - std::log(settings.minHz)) /
            (std::log(settings.maxHz) - std::log(settings.minHz));
    } else {
        t = (hz - settings.minHz) / (settings.maxHz - settings.minHz);
    }
    return std::clamp(1.0 - t, 0.0, 1.0);  // graves embaixo
}

// Escurece uma cor em luz linear. Multiplicar os bytes sRGB direto -- erro comum
// -- nao produz "metade do brilho", porque a codificacao nao e linear.
Rgb scaleBrightness(const LinearRgb& linear, double factor, GamutStrategy gamut) {
    const double f = std::clamp(factor, 0.0, 1.0);
    return toRgb8(LinearRgb{linear.r * f, linear.g * f, linear.b * f}, gamut);
}

// Magnitude -> [0,1] pela faixa dinamica em dB, relativa a `peak`.
double brightnessFromMagnitude(double magnitude, double peak, double dynamicRangeDb) {
    if (peak <= 0.0 || magnitude <= 0.0) return 0.0;
    const double db = 20.0 * std::log10(magnitude / peak);
    if (db <= -dynamicRangeDb) return 0.0;
    return 1.0 + db / dynamicRangeDb;
}

}  // namespace

Image renderSpectrogram(const std::vector<Spectrum>& frames, const FrequencyMapper& mapper,
                        const ColorEngine& engine, const PlotSettings& settings) {
    Image image(settings.width, settings.height, settings.background);
    if (frames.empty() || image.empty()) return image;

    // Pico GLOBAL, nao por quadro: normalizar quadro a quadro faria o silencio
    // parecer tao brilhante quanto o refrao, destruindo a dinamica do sinal.
    double peak = 0.0;
    for (const Spectrum& spectrum : frames) {
        for (const SpectralBin& bin : spectrum.bins) peak = std::max(peak, bin.magnitude);
    }
    if (peak <= 0.0) return image;

    // Cache de cor por bin: o mapeamento e a conversao de cor dependem so da
    // frequencia do bin, que e a mesma em todos os quadros. Sem o cache,
    // recalculariamos as CMF milhoes de vezes.
    const std::size_t binCount = frames.front().bins.size();
    std::vector<LinearRgb> binColour(binCount);
    std::vector<bool> binVisible(binCount, false);
    for (std::size_t k = 0; k < binCount; ++k) {
        const double hz = frames.front().bins[k].frequencyHz;
        if (hz < settings.minHz || hz > settings.maxHz) continue;
        const double em = mapper.map(hz);
        if (std::isnan(em)) continue;
        binColour[k] = engine.fromEmFrequency(em).linear;
        binVisible[k] = true;
    }

    for (std::size_t x = 0; x < settings.width; ++x) {
        // Quadros por coluna: com sinais longos, varios quadros caem na mesma
        // coluna e ficamos com o maior valor de cada bin (envoltoria). Descartar
        // todos menos um esconderia transientes curtos.
        const std::size_t from = x * frames.size() / settings.width;
        const std::size_t to = std::max(from + 1, (x + 1) * frames.size() / settings.width);

        for (std::size_t k = 0; k < binCount; ++k) {
            if (!binVisible[k]) continue;

            double magnitude = 0.0;
            for (std::size_t f = from; f < to && f < frames.size(); ++f) {
                if (k < frames[f].bins.size()) {
                    magnitude = std::max(magnitude, frames[f].bins[k].magnitude);
                }
            }
            const double brightness = brightnessFromMagnitude(magnitude, peak, settings.dynamicRangeDb);
            if (brightness <= 0.0) continue;

            const double hz = frames.front().bins[k].frequencyHz;
            const double yTop = axisPosition(hz, settings) * static_cast<double>(settings.height - 1);
            // Bins vizinhos podem cair no mesmo pixel em escala log (agudos) ou
            // deixar buracos (graves). Pintamos ate o bin seguinte para cobrir.
            const double hzNext = (k + 1 < binCount) ? frames.front().bins[k + 1].frequencyHz : hz;
            const double yBottom =
                axisPosition(hzNext, settings) * static_cast<double>(settings.height - 1);

            const auto y0 = static_cast<std::size_t>(std::floor(std::min(yTop, yBottom)));
            const auto y1 = static_cast<std::size_t>(std::ceil(std::max(yTop, yBottom)));
            const Rgb colour = scaleBrightness(binColour[k], brightness, engine.settings().gamut);
            for (std::size_t y = y0; y <= y1 && y < settings.height; ++y) image.set(x, y, colour);
        }
    }
    return image;
}

Image renderColorTimeline(const std::vector<FrameInterpretation>& frames,
                          const PlotSettings& settings) {
    Image image(settings.width, settings.height, settings.background);
    if (frames.empty() || image.empty()) return image;

    for (std::size_t x = 0; x < settings.width; ++x) {
        const std::size_t index = std::min(frames.size() - 1, x * frames.size() / settings.width);
        const FrameInterpretation& frame = frames[index];
        const Rgb colour = frame.silent ? settings.background : frame.colour.rgb;
        image.drawVerticalLine(x, 0, settings.height - 1, colour);
    }
    return image;
}

Image renderSpectrumPlot(const Spectrum& spectrum, const FrequencyMapper& mapper,
                         const ColorEngine& engine, const PlotSettings& settings) {
    Image image(settings.width, settings.height, settings.background);
    if (spectrum.empty() || image.empty()) return image;

    double peak = 0.0;
    for (const SpectralBin& bin : spectrum.bins) peak = std::max(peak, bin.magnitude);
    if (peak <= 0.0) return image;

    // Aqui o eixo horizontal e a frequencia, entao reaproveitamos axisPosition
    // invertendo o sentido (ela devolve 0 no topo/agudo).
    for (std::size_t k = 1; k < spectrum.bins.size(); ++k) {
        const SpectralBin& bin = spectrum.bins[k];
        if (bin.frequencyHz < settings.minHz || bin.frequencyHz > settings.maxHz) continue;

        const double t = 1.0 - axisPosition(bin.frequencyHz, settings);
        const auto x = static_cast<std::size_t>(t * static_cast<double>(settings.width - 1));

        const double height = brightnessFromMagnitude(bin.magnitude, peak, settings.dynamicRangeDb);
        if (height <= 0.0) continue;

        const double em = mapper.map(bin.frequencyHz);
        const Rgb colour = std::isnan(em) ? Rgb{60, 60, 60} : engine.fromEmFrequency(em).rgb;

        const auto barHeight =
            static_cast<std::size_t>(height * static_cast<double>(settings.height - 1));
        image.drawVerticalLine(x, settings.height - 1 - barHeight, settings.height - 1, colour);
    }
    return image;
}

Image renderMappingRuler(const FrequencyMapper& mapper, const ColorEngine& engine,
                         const PlotSettings& settings) {
    Image image(settings.width, settings.height, settings.background);
    if (image.empty()) return image;

    const std::size_t rampHeight = settings.height * 3 / 4;

    for (std::size_t x = 0; x < settings.width; ++x) {
        const double t = static_cast<double>(x) / static_cast<double>(settings.width - 1);
        const double hz =
            settings.logFrequencyAxis
                ? settings.minHz * std::pow(settings.maxHz / settings.minHz, t)
                : settings.minHz + t * (settings.maxHz - settings.minHz);

        const double em = mapper.map(hz);
        const Rgb colour = std::isnan(em) ? Rgb{40, 40, 40} : engine.fromEmFrequency(em).rgb;
        image.drawVerticalLine(x, 0, rampHeight - 1, colour);
    }

    // Marcas nas oitavas de A. Com OctaveMapper todas caem sobre a mesma cor da
    // rampa; com LogMapper, sobre cores diferentes. E esse contraste que a
    // figura existe para mostrar.
    for (double hz = 27.5; hz <= settings.maxHz; hz *= 2.0) {
        if (hz < settings.minHz) continue;
        const double t = settings.logFrequencyAxis
                             ? std::log(hz / settings.minHz) / std::log(settings.maxHz / settings.minHz)
                             : (hz - settings.minHz) / (settings.maxHz - settings.minHz);
        const auto x = static_cast<std::size_t>(t * static_cast<double>(settings.width - 1));
        image.drawVerticalLine(x, rampHeight, settings.height - 1, Rgb{230, 230, 230});
        if (x + 1 < settings.width) {
            image.drawVerticalLine(x + 1, rampHeight, settings.height - 1, Rgb{230, 230, 230});
        }
    }
    return image;
}

}  // namespace soundwave
