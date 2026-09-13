#include "test_framework.hpp"

#include <algorithm>
#include <cmath>

#include "soundwave/color/color_engine.hpp"

using namespace soundwave;

TEST(color, lambda_igual_c_sobre_f) {
    // Unico passo genuinamente fisico do pipeline. Vale checar contra valores
    // calculados a mao.
    CHECK_NEAR(wavelengthFromFrequency(kSpeedOfLight), 1.0, 1e-15);
    CHECK_NEAR(metresToNanometres(wavelengthFromFrequency(5.0e14)), 599.585, 0.01);
    CHECK_NEAR(frequencyFromWavelength(500e-9), 5.99584916e14, 1e7);
}

TEST(color, conversao_frequencia_lambda_faz_ida_e_volta) {
    for (double nm : {380.0, 400.0, 450.0, 500.0, 550.0, 600.0, 700.0, 750.0, 800.0}) {
        const double metres = nanometresToMetres(nm);
        const double back = wavelengthFromFrequency(frequencyFromWavelength(metres));
        CHECK_NEAR(metresToNanometres(back), nm, 1e-9);
    }
}

TEST(color, entradas_invalidas_nao_produzem_lixo) {
    CHECK_NEAR(wavelengthFromFrequency(0.0), 0.0, 0.0);
    CHECK_NEAR(wavelengthFromFrequency(-1.0), 0.0, 0.0);
    CHECK_NEAR(wavelengthFromFrequency(std::nan("")), 0.0, 0.0);
    CHECK_NEAR(frequencyFromWavelength(0.0), 0.0, 0.0);
}

TEST(color, limites_do_visivel_sao_consistentes) {
    CHECK(isVisibleWavelength(400e-9));
    CHECK(isVisibleWavelength(550e-9));
    CHECK(isVisibleWavelength(750e-9));
    CHECK(!isVisibleWavelength(399e-9));
    CHECK(!isVisibleWavelength(751e-9));

    // As constantes em Hz tem de bater com as em metros.
    CHECK_NEAR(frequencyFromWavelength(kVisibleMinWavelengthM), kVisibleMaxHz, 1e6);
    CHECK_NEAR(frequencyFromWavelength(kVisibleMaxWavelengthM), kVisibleMinHz, 1e6);
}

TEST(color, y_barra_tem_pico_unitario_em_555nm) {
    // Verificacao do ajuste de Wyman/Sloan/Shirley contra a propriedade mais
    // conhecida das CMF: y_barra e a funcao de eficiencia luminosa fotopica,
    // normalizada para 1,0 no seu pico, em 555 nm.
    CHECK_NEAR(cieY(555.0), 1.0, 0.005);

    double peak = 0.0;
    double peakNm = 0.0;
    for (double nm = 380.0; nm <= 780.0; nm += 0.1) {
        if (cieY(nm) > peak) {
            peak = cieY(nm);
            peakNm = nm;
        }
    }
    CHECK_NEAR(peakNm, 555.0, 3.0);
    CHECK_NEAR(peak, 1.0, 0.005);
}

TEST(color, x_e_z_barra_tem_os_picos_esperados) {
    // x_barra: pico principal ~1,056 perto de 600 nm (CIE tabelada: ~1,062).
    // z_barra: pico ~1,78 perto de 445 nm (CIE tabelada: ~1,78).
    double peakX = 0.0;
    double peakXnm = 0.0;
    double peakZ = 0.0;
    double peakZnm = 0.0;
    for (double nm = 380.0; nm <= 780.0; nm += 0.1) {
        if (cieX(nm) > peakX) { peakX = cieX(nm); peakXnm = nm; }
        if (cieZ(nm) > peakZ) { peakZ = cieZ(nm); peakZnm = nm; }
    }
    CHECK_NEAR(peakX, 1.056, 0.02);
    CHECK_NEAR(peakXnm, 600.0, 5.0);
    CHECK_NEAR(peakZ, 1.78, 0.05);
    CHECK_NEAR(peakZnm, 445.0, 8.0);
}

TEST(color, cmf_sao_praticamente_nao_negativas) {
    // x_barra tem um lobo negativo pequeno no ajuste (coeficiente -0,065), que
    // e artefato do ajuste e nao das CMF reais. Verificamos que ele e pequeno.
    for (double nm = 390.0; nm <= 760.0; nm += 1.0) {
        CHECK(cieX(nm) > -0.02);
        CHECK(cieY(nm) >= 0.0);
        CHECK(cieZ(nm) >= 0.0);
    }
}

TEST(color, fora_do_visivel_o_triestimulo_e_zero) {
    for (double nm : {300.0, 390.0, 760.0, 1000.0}) {
        const Xyz colour = spectralXyz(nanometresToMetres(nm));
        CHECK_NEAR(colour.x, 0.0, 0.0);
        CHECK_NEAR(colour.y, 0.0, 0.0);
        CHECK_NEAR(colour.z, 0.0, 0.0);
    }
}

TEST(color, branco_d65_vira_rgb_neutro) {
    // Teste-ancora da matriz XYZ->sRGB: o ponto branco do espaco tem de virar
    // exatamente (1,1,1). Qualquer erro de digitacao na matriz falha aqui.
    const LinearRgb white = xyzToLinearRgb(whitePointD65());
    CHECK_NEAR(white.r, 1.0, 1e-4);
    CHECK_NEAR(white.g, 1.0, 1e-4);
    CHECK_NEAR(white.b, 1.0, 1e-4);
    CHECK(isInGamut(white, 1e-3));
}

TEST(color, matrizes_xyz_e_rgb_sao_inversas) {
    const LinearRgb original{0.3, 0.6, 0.8};
    const LinearRgb back = xyzToLinearRgb(linearRgbToXyz(original));
    CHECK_NEAR(back.r, original.r, 1e-6);
    CHECK_NEAR(back.g, original.g, 1e-6);
    CHECK_NEAR(back.b, original.b, 1e-6);
}

TEST(color, transferencia_srgb_faz_ida_e_volta) {
    for (double v : {0.0, 0.001, 0.0031308, 0.05, 0.2, 0.5, 0.9, 1.0}) {
        CHECK_NEAR(decodeSrgb(encodeSrgb(v)), v, 1e-9);
    }
    // Continuidade na emenda entre o trecho linear e o de potencia.
    CHECK_NEAR(encodeSrgb(0.0031308), 12.92 * 0.0031308, 1e-9);
    CHECK_NEAR(encodeSrgb(0.0), 0.0, 0.0);
    CHECK_NEAR(encodeSrgb(1.0), 1.0, 1e-12);
}

TEST(color, cores_espectrais_puras_ficam_fora_do_gamut_srgb) {
    // Limitacao central documentada em docs/color.md, verificada e nao apenas
    // afirmada: quase nenhuma cor monocromatica cabe no triangulo sRGB.
    int outside = 0;
    int total = 0;
    for (double nm = 400.0; nm <= 750.0; nm += 5.0) {
        const Xyz xyz = spectralXyz(nanometresToMetres(nm));
        if (!isInGamut(xyzToLinearRgb(xyz))) ++outside;
        ++total;
    }
    CHECK(total > 60);
    CHECK(outside > total * 9 / 10);  // mais de 90% fora
}

TEST(color, tratamento_de_gamut_sempre_produz_cor_valida) {
    for (GamutStrategy strategy :
         {GamutStrategy::Clip, GamutStrategy::Desaturate, GamutStrategy::DesaturateAndScale}) {
        for (double nm = 400.0; nm <= 750.0; nm += 2.5) {
            const LinearRgb mapped =
                mapIntoGamut(xyzToLinearRgb(spectralXyz(nanometresToMetres(nm))), strategy);
            CHECK(isInGamut(mapped, 1e-9));
        }
    }
}

TEST(color, dessaturar_preserva_o_matiz_melhor_que_ceifar) {
    // Justificativa do padrao do projeto. Comparamos qual estrategia mantem as
    // RAZOES entre canais mais proximas das do sinal original fora do gamut.
    const Xyz xyz = spectralXyz(nanometresToMetres(480.0));  // azul-ciano, bem fora
    const LinearRgb raw = xyzToLinearRgb(xyz);
    CHECK(!isInGamut(raw));

    const LinearRgb clipped = mapIntoGamut(raw, GamutStrategy::Clip);
    const LinearRgb desaturated = mapIntoGamut(raw, GamutStrategy::Desaturate);

    // Ceifar zera completamente o canal negativo; dessaturar o preserva em zero
    // mas desloca os outros dois junto, mantendo a diferenca relativa entre eles.
    const double rawSpan = std::max({raw.r, raw.g, raw.b}) - std::min({raw.r, raw.g, raw.b});
    const double clippedSpan =
        std::max({clipped.r, clipped.g, clipped.b}) - std::min({clipped.r, clipped.g, clipped.b});
    const double desaturatedSpan = std::max({desaturated.r, desaturated.g, desaturated.b}) -
                                   std::min({desaturated.r, desaturated.g, desaturated.b});
    CHECK(std::abs(desaturatedSpan - rawSpan) < std::abs(clippedSpan - rawSpan));
}

TEST(color, bandas_em_sao_classificadas_corretamente) {
    CHECK(classifyByWavelength(10.0) == EmBand::Radio);
    CHECK(classifyByWavelength(1e-2) == EmBand::Microwave);
    CHECK(classifyByWavelength(1e-5) == EmBand::Infrared);
    CHECK(classifyByWavelength(550e-9) == EmBand::Visible);
    CHECK(classifyByWavelength(100e-9) == EmBand::Ultraviolet);
    CHECK(classifyByWavelength(1e-9) == EmBand::XRay);
    CHECK(classifyByWavelength(1e-13) == EmBand::Gamma);

    // Bordas exatas do visivel.
    CHECK(classifyByWavelength(400e-9) == EmBand::Visible);
    CHECK(classifyByWavelength(750e-9) == EmBand::Visible);
    CHECK(classifyByWavelength(750.001e-9) == EmBand::Infrared);
    CHECK(classifyByWavelength(399.999e-9) == EmBand::Ultraviolet);
}

TEST(color, classificacao_por_frequencia_bate_com_a_por_lambda) {
    for (double nm : {1e9, 1e6, 1e4, 550.0, 100.0, 1.0, 1e-4}) {
        const double metres = nanometresToMetres(nm);
        CHECK(classifyByFrequency(frequencyFromWavelength(metres)) == classifyByWavelength(metres));
    }
}

TEST(color, pseudocor_se_declara_como_falsa) {
    // A garantia mais importante da camada 3: nada fora do visivel pode se passar
    // por cor medida.
    for (EmBand band : {EmBand::Radio, EmBand::Microwave, EmBand::Infrared, EmBand::Ultraviolet,
                        EmBand::XRay, EmBand::Gamma}) {
        CHECK(pseudoColourForBand(band).isFalseColour);
    }
    CHECK(!pseudoColourForBand(EmBand::Visible).isFalseColour);
}

TEST(color_engine, visivel_produz_cor_medida_e_nao_falsa) {
    const ColorEngine engine;
    const ColorResult result = engine.fromWavelength(550e-9);
    CHECK(result.band == EmBand::Visible);
    CHECK(!result.isFalseColour);
    CHECK(result.wasOutOfGamut);  // esperado para uma cor espectral pura
    CHECK(result.rgb.g > result.rgb.b);  // 550 nm e verde
}

TEST(color_engine, comprimentos_de_onda_conhecidos_dao_a_cor_certa) {
    const ColorEngine engine;
    const ColorResult red = engine.fromWavelength(650e-9);
    const ColorResult green = engine.fromWavelength(530e-9);
    const ColorResult blue = engine.fromWavelength(460e-9);

    CHECK(red.rgb.r > red.rgb.g && red.rgb.r > red.rgb.b);
    CHECK(green.rgb.g > green.rgb.r && green.rgb.g > green.rgb.b);
    CHECK(blue.rgb.b > blue.rgb.r && blue.rgb.b > blue.rgb.g);
}

TEST(color_engine, modo_visible_only_devolve_preto_fora_do_visivel) {
    ColorEngineSettings settings;
    settings.mode = SpectrumMode::VisibleOnly;
    const ColorEngine engine(settings);

    const ColorResult infrared = engine.fromWavelength(1e-5);
    CHECK(infrared.band == EmBand::Infrared);
    CHECK_EQ(static_cast<int>(infrared.rgb.r), 0);
    CHECK_EQ(static_cast<int>(infrared.rgb.g), 0);
    CHECK_EQ(static_cast<int>(infrared.rgb.b), 0);
    CHECK(infrared.isFalseColour);
}

TEST(color_engine, modo_visible_with_bands_usa_pseudocor_fora_do_visivel) {
    ColorEngineSettings settings;
    settings.mode = SpectrumMode::VisibleWithBands;
    const ColorEngine engine(settings);

    const ColorResult infrared = engine.fromWavelength(1e-5);
    CHECK(infrared.isFalseColour);
    CHECK(infrared.rgb.r + infrared.rgb.g + infrared.rgb.b > 0);

    const ColorResult visible = engine.fromWavelength(550e-9);
    CHECK(!visible.isFalseColour);
}

TEST(color_engine, modo_full_em_usa_pseudocor_ate_no_visivel) {
    ColorEngineSettings settings;
    settings.mode = SpectrumMode::FullEmSpectrum;
    const ColorEngine engine(settings);
    CHECK(engine.fromWavelength(550e-9).isFalseColour);
}

TEST(color_engine, mistura_ponderada_fica_entre_as_cores) {
    const ColorEngine engine;
    const ColorResult red = engine.fromWavelength(650e-9);
    const ColorResult blue = engine.fromWavelength(460e-9);

    const ColorResult even = engine.weightedMix({red, blue}, {1.0, 1.0});
    const ColorResult mostlyRed = engine.weightedMix({red, blue}, {9.0, 1.0});

    CHECK(mostlyRed.rgb.r > even.rgb.r);
    CHECK(mostlyRed.rgb.b < even.rgb.b);
}

TEST(color_engine, mistura_com_peso_zero_ignora_a_cor) {
    const ColorEngine engine;
    const ColorResult red = engine.fromWavelength(650e-9);
    const ColorResult blue = engine.fromWavelength(460e-9);

    const ColorResult mixed = engine.weightedMix({red, blue}, {1.0, 0.0});
    CHECK_EQ(static_cast<int>(mixed.rgb.r), static_cast<int>(red.rgb.r));
    CHECK_EQ(static_cast<int>(mixed.rgb.g), static_cast<int>(red.rgb.g));
    CHECK_EQ(static_cast<int>(mixed.rgb.b), static_cast<int>(red.rgb.b));
}

TEST(color_engine, mistura_contaminada_por_pseudocor_se_declara_falsa) {
    ColorEngineSettings settings;
    settings.mode = SpectrumMode::VisibleWithBands;
    const ColorEngine engine(settings);

    const ColorResult visible = engine.fromWavelength(550e-9);
    const ColorResult infrared = engine.fromWavelength(1e-5);
    CHECK(engine.weightedMix({visible, infrared}, {1.0, 1.0}).isFalseColour);
    CHECK(!engine.weightedMix({visible, visible}, {1.0, 1.0}).isFalseColour);
}

TEST(color_engine, mistura_degenerada_nao_quebra) {
    const ColorEngine engine;
    CHECK_EQ(static_cast<int>(engine.weightedMix({}, {}).rgb.r), 0);

    const ColorResult red = engine.fromWavelength(650e-9);
    CHECK_EQ(static_cast<int>(engine.weightedMix({red}, {1.0, 2.0}).rgb.r), 0);  // tamanhos diferentes
    CHECK_EQ(static_cast<int>(engine.weightedMix({red}, {0.0}).rgb.r), 0);       // peso total zero
}

TEST(color_engine, normalizar_luminancia_iguala_o_brilho_no_espectro) {
    ColorEngineSettings on;
    on.normaliseLuminance = true;
    ColorEngineSettings off;
    off.normaliseLuminance = false;

    const ColorEngine withNorm(on);
    const ColorEngine withoutNorm(off);

    // Sem normalizar, a borda do visivel fica quase preta (baixa eficiencia
    // luminosa). Com normalizacao, fica comparavel ao verde central.
    const ColorResult edgeOff = withoutNorm.fromWavelength(700e-9);
    const ColorResult edgeOn = withNorm.fromWavelength(700e-9);
    const int sumOff = edgeOff.rgb.r + edgeOff.rgb.g + edgeOff.rgb.b;
    const int sumOn = edgeOn.rgb.r + edgeOn.rgb.g + edgeOn.rgb.b;
    CHECK(sumOn > sumOff);
}

TEST(color_engine, frequencia_invalida_produz_preto_sem_quebrar) {
    const ColorEngine engine;
    for (double f : {0.0, -1.0, std::nan("")}) {
        const ColorResult result = engine.fromEmFrequency(f);
        CHECK_EQ(static_cast<int>(result.rgb.r), 0);
        CHECK_EQ(static_cast<int>(result.rgb.g), 0);
        CHECK_EQ(static_cast<int>(result.rgb.b), 0);
    }
}

TEST(color_engine, nomes_de_modo_e_estrategia_fazem_ida_e_volta) {
    for (SpectrumMode mode : {SpectrumMode::VisibleOnly, SpectrumMode::VisibleWithBands,
                              SpectrumMode::FullEmSpectrum}) {
        SpectrumMode parsed{};
        CHECK(parseSpectrumMode(spectrumModeName(mode), parsed));
        CHECK(parsed == mode);
    }
    for (GamutStrategy strategy : {GamutStrategy::Clip, GamutStrategy::Desaturate,
                                   GamutStrategy::DesaturateAndScale}) {
        GamutStrategy parsed{};
        CHECK(parseGamutStrategy(gamutStrategyName(strategy), parsed));
        CHECK(parsed == strategy);
    }
    SpectrumMode ignored{};
    CHECK(!parseSpectrumMode("modo-inexistente", ignored));
}

TEST(color_engine, o_espectro_visivel_varre_cores_distintas) {
    // Sanidade global: percorrer o visivel tem de produzir muitas cores
    // diferentes. Se algo colapsasse (matriz errada, gamut agressivo demais),
    // isto cairia drasticamente.
    const ColorEngine engine;
    std::vector<int> seen;
    for (double nm = 400.0; nm <= 750.0; nm += 1.0) {
        const Rgb rgb = engine.fromWavelength(nanometresToMetres(nm)).rgb;
        const int packed = (rgb.r << 16) | (rgb.g << 8) | rgb.b;
        if (std::find(seen.begin(), seen.end(), packed) == seen.end()) seen.push_back(packed);
    }
    CHECK(seen.size() > 150);
}
