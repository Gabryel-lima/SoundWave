#pragma once

#include <complex>
#include <cstddef>
#include <vector>

namespace soundwave {

// FFT Cooley-Tukey radix-2, iterativa, decimacao no tempo, in-place.
//
// Nao usamos FFTW por escolha: o nucleo matematico do projeto precisa compilar e
// ser testado sem nenhuma dependencia externa (plano, secao 13). Se FFTW estiver
// disponivel o CMake define SOUNDWAVE_WITH_FFTW e um backend alternativo pode ser
// plugado atras desta mesma interface -- os testes de equivalencia garantem que
// os dois concordam.
//
// Exige tamanho potencia de dois. Isso e restricao do algoritmo radix-2, nao da
// FFT em geral; o plano so pede 1024/2048/4096/8192, todos validos.
class Fft {
public:
    explicit Fft(std::size_t size);

    [[nodiscard]] std::size_t size() const { return size_; }

    // Transformada direta: X[k] = sum_n x[n] * exp(-i*2*pi*k*n/N). Sem escala.
    void forward(std::vector<std::complex<double>>& data) const;

    // Inversa, com escala 1/N. forward seguido de inverse recupera a entrada.
    void inverse(std::vector<std::complex<double>>& data) const;

    // Conveniencia para sinal real: retorna os N/2+1 bins de DC ate Nyquist.
    // A metade superior e o conjugado complexo da inferior e nao carrega
    // informacao nova para entrada real.
    [[nodiscard]] std::vector<std::complex<double>> forwardReal(
        const std::vector<double>& samples) const;

    [[nodiscard]] static bool isPowerOfTwo(std::size_t n);
    // Menor potencia de dois >= n.
    [[nodiscard]] static std::size_t nextPowerOfTwo(std::size_t n);

private:
    void transform(std::vector<std::complex<double>>& data, bool inverseTransform) const;

    std::size_t size_;
    std::vector<std::size_t> reversal_;              // permutacao bit-reversa
    std::vector<std::complex<double>> twiddles_;     // exp(-i*2*pi*k/N), k < N/2
};

}  // namespace soundwave
