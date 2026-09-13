#include "soundwave/render/interpretation.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace soundwave {

std::string_view interpretationModeName(InterpretationMode mode) {
    switch (mode) {
        case InterpretationMode::Dominant: return "dominant";
        case InterpretationMode::Weighted: return "weighted";
        case InterpretationMode::Spectral: return "spectral";
    }
    return "unknown";
}

bool parseInterpretationMode(std::string_view name, InterpretationMode& out) {
    if (name == "dominant") {
        out = InterpretationMode::Dominant;
    } else if (name == "weighted") {
        out = InterpretationMode::Weighted;
    } else if (name == "spectral") {
        out = InterpretationMode::Spectral;
    } else {
        return false;
    }
    return true;
}

Interpreter::Interpreter(const FrequencyMapper& mapper, const ColorEngine& engine,
                         InterpreterSettings settings)
    : mapper_(&mapper), engine_(&engine), settings_(settings) {
    if (settings_.analysisMaxHz <= settings_.analysisMinHz) {
        throw std::invalid_argument("InterpreterSettings: faixa de analise invalida");
    }
    if (!(settings_.weightExponent > 0.0)) {
        throw std::invalid_argument("InterpreterSettings: weightExponent deve ser positivo");
    }
}

BinContribution Interpreter::makeContribution(double sourceHz, double magnitude) const {
    BinContribution contribution;
    contribution.sourceHz = sourceHz;
    contribution.magnitude = magnitude;
    contribution.emHz = mapper_->map(sourceHz);
    // NaN sai da politica Discard: o bin existe no sinal mas foi deliberadamente
    // deixado fora do mapeamento. Fica preto e com peso zero, nao some.
    if (std::isnan(contribution.emHz)) return contribution;
    contribution.colour = engine_->fromEmFrequency(contribution.emHz);
    contribution.wavelengthM = contribution.colour.wavelengthM;
    return contribution;
}

FrameInterpretation Interpreter::interpret(const Spectrum& spectrum) const {
    FrameInterpretation frame;
    frame.timeOffsetSeconds = spectrum.timeOffsetSeconds;

    if (spectrum.empty()) {
        frame.silent = true;
        return frame;
    }

    frame.totalMagnitude = 0.0;
    for (const SpectralBin& bin : spectrum.bins) {
        if (bin.frequencyHz < settings_.analysisMinHz) continue;
        if (bin.frequencyHz > settings_.analysisMaxHz) break;
        frame.totalMagnitude += bin.magnitude;
    }

    if (frame.totalMagnitude < settings_.silenceThreshold) {
        frame.silent = true;
        return frame;  // cor fica preta: silencio nao tem cor
    }

    auto dominant = dominantPeak(spectrum, settings_.analysisMinHz, settings_.analysisMaxHz);
    if (!dominant) {
        frame.silent = true;
        return frame;
    }
    frame.dominantSourceHz = dominant->frequencyHz;
    frame.dominantMagnitude = dominant->magnitude;

    if (settings_.mode == InterpretationMode::Dominant) {
        frame.colour = makeContribution(dominant->frequencyHz, dominant->magnitude).colour;
        return frame;
    }

    const std::vector<SpectralPeak> peaks =
        findPeaks(spectrum, settings_.analysisMinHz, settings_.analysisMaxHz,
                  settings_.maxContributions, settings_.magnitudeFloorRatio);
    if (peaks.empty()) {
        frame.colour = makeContribution(dominant->frequencyHz, dominant->magnitude).colour;
        return frame;
    }

    frame.contributions.reserve(peaks.size());
    double weightSum = 0.0;
    for (const SpectralPeak& peak : peaks) {
        BinContribution contribution = makeContribution(peak.frequencyHz, peak.magnitude);
        contribution.weight =
            std::isnan(contribution.emHz) ? 0.0 : std::pow(peak.magnitude, settings_.weightExponent);
        weightSum += contribution.weight;
        frame.contributions.push_back(contribution);
    }

    if (weightSum > 0.0) {
        for (BinContribution& contribution : frame.contributions) contribution.weight /= weightSum;
    }

    if (settings_.mode == InterpretationMode::Weighted) {
        std::vector<ColorResult> colours;
        std::vector<double> weights;
        colours.reserve(frame.contributions.size());
        weights.reserve(frame.contributions.size());
        for (const BinContribution& contribution : frame.contributions) {
            if (contribution.weight <= 0.0) continue;
            colours.push_back(contribution.colour);
            weights.push_back(contribution.weight);
        }
        frame.colour = engine_->weightedMix(colours, weights);
    } else {
        // Spectral: nao colapsamos. A cor do quadro e a do parcial mais forte,
        // so como rotulo; o conteudo real esta em `contributions` e cabe ao
        // renderizador desenhar o conjunto.
        frame.colour = frame.contributions.front().colour;
    }

    return frame;
}

std::vector<FrameInterpretation> Interpreter::interpretAll(
    const std::vector<Spectrum>& frames) const {
    std::vector<FrameInterpretation> results;
    results.reserve(frames.size());
    for (const Spectrum& spectrum : frames) results.push_back(interpret(spectrum));
    return results;
}

}  // namespace soundwave
