#include "soundwave/audio/pcm_buffer.hpp"

#include <cmath>

namespace soundwave {

double PcmBuffer::peakAmplitude() const {
    double peak = 0.0;
    for (double s : samples) peak = std::max(peak, std::abs(s));
    return peak;
}

double PcmBuffer::rmsAmplitude() const {
    if (samples.empty()) return 0.0;
    double sum = 0.0;
    for (double s : samples) sum += s * s;
    return std::sqrt(sum / static_cast<double>(samples.size()));
}

std::vector<double> downmixToMono(const std::vector<double>& interleaved, std::size_t channels) {
    if (channels <= 1) return interleaved;

    const std::size_t frames = interleaved.size() / channels;
    std::vector<double> mono(frames, 0.0);
    for (std::size_t f = 0; f < frames; ++f) {
        double sum = 0.0;
        for (std::size_t c = 0; c < channels; ++c) sum += interleaved[f * channels + c];
        mono[f] = sum / static_cast<double>(channels);
    }
    return mono;
}

}  // namespace soundwave
