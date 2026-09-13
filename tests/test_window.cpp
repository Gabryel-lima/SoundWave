#include "test_framework.hpp"

#include <cmath>

#include "soundwave/dsp/window.hpp"

using namespace soundwave;

TEST(window, retangular_e_toda_unitaria) {
    const std::vector<double> w = makeWindow(WindowType::Rectangular, 16);
    CHECK_EQ(w.size(), std::size_t{16});
    for (double value : w) CHECK_NEAR(value, 1.0, 0.0);
    CHECK_NEAR(coherentGain(w), 1.0, 1e-15);
    CHECK_NEAR(equivalentNoiseBandwidth(w), 1.0, 1e-15);
}

TEST(window, hann_comeca_em_zero_e_tem_ganho_meio) {
    const std::vector<double> w = makeWindow(WindowType::Hann, 1024);
    // Na forma periodica, w[0] = 0 mas w[N-1] != 0 (o zero final pertence ao
    // periodo seguinte). Confundir periodica com simetrica e um erro classico.
    CHECK_NEAR(w.front(), 0.0, 1e-15);
    CHECK(w.back() > 0.0);
    CHECK_NEAR(w[512], 1.0, 1e-12);           // pico no centro
    CHECK_NEAR(coherentGain(w), 0.5, 1e-12);  // valor analitico conhecido
}

TEST(window, ganhos_coerentes_conferem_com_os_valores_analiticos) {
    // Sao os coeficientes a0 de cada janela. Se o gerador quebrar, quebra aqui.
    CHECK_NEAR(coherentGain(makeWindow(WindowType::Hann, 2048)), 0.5, 1e-12);
    CHECK_NEAR(coherentGain(makeWindow(WindowType::Hamming, 2048)), 0.54, 1e-12);
    CHECK_NEAR(coherentGain(makeWindow(WindowType::Blackman, 2048)), 0.42, 1e-12);
    CHECK_NEAR(coherentGain(makeWindow(WindowType::BlackmanHarris, 2048)), 0.35875, 1e-12);
}

TEST(window, enbw_confere_com_a_literatura) {
    // Largura de banda equivalente de ruido, em bins. Valores tabelados classicos
    // (Harris, 1978). Confirmam que ha o compromisso esperado: mais supressao de
    // lobo lateral custa lobo principal mais largo.
    CHECK_NEAR(equivalentNoiseBandwidth(makeWindow(WindowType::Hann, 4096)), 1.5, 1e-3);
    CHECK_NEAR(equivalentNoiseBandwidth(makeWindow(WindowType::Hamming, 4096)), 1.3628, 1e-3);
    CHECK_NEAR(equivalentNoiseBandwidth(makeWindow(WindowType::Blackman, 4096)), 1.7269, 1e-3);
    CHECK_NEAR(equivalentNoiseBandwidth(makeWindow(WindowType::BlackmanHarris, 4096)), 2.0044,
               1e-3);
}

TEST(window, todas_sao_nao_negativas_e_limitadas_por_um) {
    for (WindowType type : {WindowType::Hann, WindowType::Hamming, WindowType::Blackman,
                            WindowType::BlackmanHarris}) {
        const std::vector<double> w = makeWindow(type, 512);
        for (double value : w) {
            CHECK(value >= -1e-12);
            CHECK(value <= 1.0 + 1e-12);
        }
    }
}

TEST(window, nomes_fazem_ida_e_volta) {
    for (WindowType type : {WindowType::Rectangular, WindowType::Hann, WindowType::Hamming,
                            WindowType::Blackman, WindowType::BlackmanHarris}) {
        WindowType parsed{};
        CHECK(parseWindowType(windowTypeName(type), parsed));
        CHECK(parsed == type);
    }
    WindowType ignored{};
    CHECK(!parseWindowType("gaussiana-que-nao-existe", ignored));
}

TEST(window, tamanho_zero_nao_quebra) {
    const std::vector<double> w = makeWindow(WindowType::Hann, 0);
    CHECK(w.empty());
    CHECK_NEAR(coherentGain(w), 0.0, 0.0);
}
