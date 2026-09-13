#include "soundwave/dsp/fft.hpp"

#include <cmath>
#include <numbers>
#include <stdexcept>

namespace soundwave {

bool Fft::isPowerOfTwo(std::size_t n) { return n != 0 && (n & (n - 1)) == 0; }

std::size_t Fft::nextPowerOfTwo(std::size_t n) {
    if (n <= 1) return 1;
    std::size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

Fft::Fft(std::size_t size) : size_(size) {
    if (!isPowerOfTwo(size)) {
        throw std::invalid_argument("Fft: tamanho deve ser potencia de dois");
    }

    // Tabela de permutacao bit-reversa, construida incrementalmente:
    // rev[i] = (rev[i >> 1] >> 1) | (bit menos significativo de i no topo).
    reversal_.resize(size_);
    reversal_[0] = 0;
    std::size_t highBit = size_ >> 1;
    for (std::size_t i = 1; i < size_; ++i) {
        reversal_[i] = (reversal_[i >> 1] >> 1) | ((i & 1U) ? highBit : 0U);
    }

    // Twiddles pre-computados uma vez: evita N*log(N) chamadas a sin/cos por
    // transformada e mantem o resultado bit-a-bit identico entre execucoes.
    twiddles_.resize(size_ / 2);
    for (std::size_t k = 0; k < size_ / 2; ++k) {
        const double angle = -2.0 * std::numbers::pi * static_cast<double>(k) /
                             static_cast<double>(size_);
        twiddles_[k] = std::polar(1.0, angle);
    }
}

void Fft::transform(std::vector<std::complex<double>>& data, bool inverseTransform) const {
    if (data.size() != size_) {
        throw std::invalid_argument("Fft: tamanho do buffer nao confere com o plano");
    }

    for (std::size_t i = 0; i < size_; ++i) {
        if (i < reversal_[i]) std::swap(data[i], data[reversal_[i]]);
    }

    for (std::size_t len = 2; len <= size_; len <<= 1) {
        const std::size_t half = len >> 1;
        const std::size_t stride = size_ / len;
        for (std::size_t start = 0; start < size_; start += len) {
            for (std::size_t j = 0; j < half; ++j) {
                std::complex<double> w = twiddles_[j * stride];
                if (inverseTransform) w = std::conj(w);
                const std::complex<double> even = data[start + j];
                const std::complex<double> odd = data[start + j + half] * w;
                data[start + j] = even + odd;
                data[start + j + half] = even - odd;
            }
        }
    }

    if (inverseTransform) {
        const double scale = 1.0 / static_cast<double>(size_);
        for (auto& value : data) value *= scale;
    }
}

void Fft::forward(std::vector<std::complex<double>>& data) const { transform(data, false); }

void Fft::inverse(std::vector<std::complex<double>>& data) const { transform(data, true); }

std::vector<std::complex<double>> Fft::forwardReal(const std::vector<double>& samples) const {
    if (samples.size() != size_) {
        throw std::invalid_argument("Fft::forwardReal: tamanho incorreto");
    }
    std::vector<std::complex<double>> buffer(size_);
    for (std::size_t i = 0; i < size_; ++i) buffer[i] = std::complex<double>(samples[i], 0.0);
    forward(buffer);
    buffer.resize(size_ / 2 + 1);
    return buffer;
}

}  // namespace soundwave
