#include "test_framework.hpp"

#include <cmath>
#include <numbers>

#include "soundwave/dsp/fft.hpp"

using namespace soundwave;

TEST(fft, rejeita_tamanho_nao_potencia_de_dois) {
    bool threw = false;
    try {
        Fft bad(1000);
        (void)bad;
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

TEST(fft, potencia_de_dois) {
    CHECK(Fft::isPowerOfTwo(1));
    CHECK(Fft::isPowerOfTwo(4096));
    CHECK(!Fft::isPowerOfTwo(0));
    CHECK(!Fft::isPowerOfTwo(3));
    CHECK_EQ(Fft::nextPowerOfTwo(1000), std::size_t{1024});
    CHECK_EQ(Fft::nextPowerOfTwo(1024), std::size_t{1024});
}

TEST(fft, impulso_produz_espectro_plano) {
    // A transformada de um impulso unitario em n=0 e 1 em TODOS os bins. E o
    // teste mais direto de que a permutacao bit-reversa e os twiddles estao
    // corretos: qualquer erro de indice quebra a planicidade.
    const std::size_t n = 64;
    Fft fft(n);
    std::vector<std::complex<double>> data(n, {0.0, 0.0});
    data[0] = {1.0, 0.0};
    fft.forward(data);

    for (std::size_t k = 0; k < n; ++k) {
        CHECK_NEAR(data[k].real(), 1.0, 1e-12);
        CHECK_NEAR(data[k].imag(), 0.0, 1e-12);
    }
}

TEST(fft, continua_produz_pico_apenas_em_dc) {
    const std::size_t n = 128;
    Fft fft(n);
    std::vector<std::complex<double>> data(n, {1.0, 0.0});
    fft.forward(data);

    CHECK_NEAR(data[0].real(), static_cast<double>(n), 1e-9);
    for (std::size_t k = 1; k < n; ++k) {
        CHECK_NEAR(std::abs(data[k]), 0.0, 1e-9);
    }
}

TEST(fft, senoide_em_bin_exato_concentra_energia) {
    // Frequencia escolhida para cair exatamente no centro de um bin: sem
    // vazamento, toda a energia fica em k e no seu espelho n-k.
    const std::size_t n = 256;
    const std::size_t k = 8;
    Fft fft(n);

    std::vector<std::complex<double>> data(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double phase = 2.0 * std::numbers::pi * static_cast<double>(k * i) /
                             static_cast<double>(n);
        data[i] = {std::sin(phase), 0.0};
    }
    fft.forward(data);

    // Amplitude 1 de uma senoide real -> n/2 em cada metade do par conjugado.
    CHECK_NEAR(std::abs(data[k]), static_cast<double>(n) / 2.0, 1e-9);
    CHECK_NEAR(std::abs(data[n - k]), static_cast<double>(n) / 2.0, 1e-9);
    for (std::size_t i = 0; i < n; ++i) {
        if (i == k || i == n - k) continue;
        CHECK_NEAR(std::abs(data[i]), 0.0, 1e-9);
    }
}

TEST(fft, ida_e_volta_recupera_o_sinal) {
    const std::size_t n = 512;
    Fft fft(n);

    std::vector<std::complex<double>> original(n);
    for (std::size_t i = 0; i < n; ++i) {
        original[i] = {std::sin(0.05 * static_cast<double>(i)) +
                           0.3 * std::cos(0.31 * static_cast<double>(i)),
                       0.1 * std::sin(0.07 * static_cast<double>(i))};
    }

    std::vector<std::complex<double>> data = original;
    fft.forward(data);
    fft.inverse(data);

    for (std::size_t i = 0; i < n; ++i) {
        CHECK_NEAR(data[i].real(), original[i].real(), 1e-10);
        CHECK_NEAR(data[i].imag(), original[i].imag(), 1e-10);
    }
}

TEST(fft, teorema_de_parseval) {
    // Parseval: a energia no tempo e a energia na frequencia dividida por N.
    // Uma checagem global -- passa apenas se a escala estiver certa em toda a
    // transformada, nao so em um bin.
    const std::size_t n = 256;
    Fft fft(n);

    std::vector<std::complex<double>> data(n);
    double timeEnergy = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double value = std::sin(0.13 * static_cast<double>(i)) +
                             0.5 * std::sin(0.77 * static_cast<double>(i) + 1.1);
        data[i] = {value, 0.0};
        timeEnergy += value * value;
    }
    fft.forward(data);

    double freqEnergy = 0.0;
    for (const std::complex<double>& bin : data) freqEnergy += std::norm(bin);
    freqEnergy /= static_cast<double>(n);

    CHECK_NEAR(freqEnergy, timeEnergy, timeEnergy * 1e-10);
}

TEST(fft, forward_real_devolve_meio_espectro) {
    const std::size_t n = 64;
    Fft fft(n);
    std::vector<double> samples(n, 0.0);
    samples[0] = 1.0;

    const std::vector<std::complex<double>> spectrum = fft.forwardReal(samples);
    CHECK_EQ(spectrum.size(), n / 2 + 1);
    for (const std::complex<double>& bin : spectrum) CHECK_NEAR(std::abs(bin), 1.0, 1e-12);
}

TEST(fft, e_determinista_entre_execucoes) {
    // Reprodutibilidade byte a byte: sem isso, "mesma entrada -> mesmo resultado"
    // nao vale nem dentro da mesma maquina.
    const std::size_t n = 128;
    Fft a(n);
    Fft b(n);

    std::vector<std::complex<double>> first(n);
    for (std::size_t i = 0; i < n; ++i) first[i] = {std::sin(0.21 * static_cast<double>(i)), 0.0};
    std::vector<std::complex<double>> second = first;

    a.forward(first);
    b.forward(second);
    for (std::size_t i = 0; i < n; ++i) {
        CHECK(first[i].real() == second[i].real());
        CHECK(first[i].imag() == second[i].imag());
    }
}
