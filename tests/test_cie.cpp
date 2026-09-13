#include "test_framework.hpp"

#include <algorithm>
#include <cmath>

#include "soundwave/color/cie_tables.hpp"
#include "soundwave/color/color_engine.hpp"

using namespace soundwave;

// -----------------------------------------------------------------------------
// Esta suite existe por causa de um bug real que passou pelos testes originais.
//
// A primeira versao usava um ajuste analitico das CMF justificado por ter "erro
// abaixo de ~1% do pico". Os testes de entao verificavam os PICOS das funcoes e
// algumas cores no centro do visivel -- e todos passavam. Mesmo assim, 736 nm
// era renderizado como VERDE, porque cor depende das RAZOES entre x_barra,
// y_barra e z_barra, e nas bordas do visivel essas funcoes valem ~1e-5, onde um
// erro absoluto de 1% do pico e um erro relativo de varias centenas por cento.
//
// Os testes abaixo cobrem exatamente o que faltava: razoes, bordas, e a
// progressao de matiz ao longo de TODO o visivel -- nao so onde e facil.
// -----------------------------------------------------------------------------

TEST(cie, tabela_bate_com_as_ancoras_oficiais) {
    // Tres valores independentes e amplamente publicados da tabela CIE 1931 2
    // graus. Se a tabela embutida tiver sido corrompida ou trocada, cai aqui.
    CHECK_NEAR(cieY(555.0), 1.00000, 1e-5);  // pico da eficiencia luminosa fotopica
    CHECK_NEAR(cieX(600.0), 1.06220, 1e-5);
    CHECK_NEAR(cieZ(445.0), 1.78260, 1e-5);
}

TEST(cie, tabela_cobre_a_faixa_esperada) {
    CHECK_NEAR(cie::firstWavelengthNm(), 380.0, 0.0);
    CHECK_NEAR(cie::lastWavelengthNm(), 780.0, 0.0);
    CHECK_EQ(cie::sampleCount(), std::size_t{81});  // 380..780 em passos de 5
}

TEST(cie, fora_da_tabela_devolve_zero) {
    for (double nm : {0.0, 100.0, 379.0, 781.0, 2000.0, std::nan("")}) {
        const Xyz value = cie::lookup(nm);
        CHECK_NEAR(value.x, 0.0, 0.0);
        CHECK_NEAR(value.y, 0.0, 0.0);
        CHECK_NEAR(value.z, 0.0, 0.0);
    }
}

TEST(cie, interpolacao_atinge_os_pontos_tabelados) {
    // Em um multiplo exato de 5 nm o resultado tem de ser o valor tabelado, sem
    // contaminacao da interpolacao.
    CHECK_NEAR(cieY(550.0), cie::lookup(550.0).y, 0.0);
    // E entre duas amostras, ficar entre elas.
    const double middle = cieY(552.5);
    CHECK(middle >= std::min(cieY(550.0), cieY(555.0)));
    CHECK(middle <= std::max(cieY(550.0), cieY(555.0)));
}

TEST(cie, razao_x_sobre_y_e_estavel_no_vermelho_profundo) {
    // ESTE e o teste que teria pego o bug. A razao x_barra/y_barra e ~2,77 e
    // praticamente constante acima de 700 nm -- e ela que mantem o vermelho
    // vermelho. O ajuste analitico dava 0,23 em 750 nm.
    for (double nm : {700.0, 710.0, 720.0, 730.0, 740.0, 750.0}) {
        const Xyz value = cie::lookup(nm);
        CHECK(value.y > 0.0);
        CHECK_NEAR(value.x / value.y, 2.77, 0.06);
    }
}

TEST(cie, o_ajuste_analitico_diverge_nas_bordas) {
    // Documenta o contra-exemplo de forma executavel, para que ninguem repita a
    // troca "por ser mais barato".
    //
    // No centro do visivel o ajuste e bom.
    CHECK_NEAR(cieXAnalyticFit(550.0), cieX(550.0), 0.05);
    CHECK_NEAR(cieYAnalyticFit(550.0), cieY(550.0), 0.05);

    // Nas bordas o erro ABSOLUTO continua minusculo...
    CHECK_NEAR(cieXAnalyticFit(750.0), cieX(750.0), 0.001);
    // ...mas a RAZAO, que e o que define a cor, esta completamente errada.
    const double fitRatio = cieXAnalyticFit(750.0) / cieYAnalyticFit(750.0);
    const double trueRatio = cieX(750.0) / cieY(750.0);
    CHECK_NEAR(trueRatio, 2.77, 0.06);
    CHECK(fitRatio < 1.0);                     // o ajuste inverte a relacao
    CHECK(std::abs(fitRatio - trueRatio) > 2.0);
}

TEST(cie, todo_o_visivel_produz_o_matiz_correto) {
    // Varredura completa, e nao apenas tres comprimentos de onda escolhidos a
    // dedo. A ausencia deste teste foi o que permitiu o bug do vermelho.
    const ColorEngine engine;

    // Vermelho profundo: R tem de dominar ate a borda de 750 nm.
    for (double nm = 610.0; nm <= 750.0; nm += 2.0) {
        const Rgb rgb = engine.fromWavelength(nanometresToMetres(nm)).rgb;
        CHECK(rgb.r > rgb.g);
        CHECK(rgb.r > rgb.b);
    }
    // Verde.
    for (double nm = 510.0; nm <= 555.0; nm += 2.0) {
        const Rgb rgb = engine.fromWavelength(nanometresToMetres(nm)).rgb;
        CHECK(rgb.g > rgb.r);
        CHECK(rgb.g > rgb.b);
    }
    // Azul / violeta, ate a borda de 400 nm.
    for (double nm = 400.0; nm <= 470.0; nm += 2.0) {
        const Rgb rgb = engine.fromWavelength(nanometresToMetres(nm)).rgb;
        CHECK(rgb.b > rgb.r);
        CHECK(rgb.b > rgb.g);
    }
}

TEST(cie, canal_dominante_avanca_de_azul_para_verde_para_vermelho) {
    // Invariante estrutural do locus espectral, independente de matiz exato:
    // percorrendo 400 -> 750 nm, o canal dominante deve seguir B, depois G,
    // depois R, e NUNCA voltar atras.
    //
    // Nota sobre um teste que tentei antes e estava errado: a razao R/B NAO e
    // monotonica. Acima de ~620 nm o azul volta a subir, porque o vermelho
    // profundo esta mais fora do gamut e a dessaturacao soma mais branco. Isso e
    // comportamento correto, nao regressao. Ja a ORDEM do canal dominante e um
    // invariante genuino -- e e exatamente o que quebrava quando 736 nm saia
    // verde, pois G reaparecia depois de R.
    const ColorEngine engine;

    auto rank = [](Rgb c) {
        if (c.b >= c.r && c.b >= c.g) return 0;  // azul
        if (c.g >= c.r) return 1;                // verde
        return 2;                                // vermelho
    };

    int previous = 0;
    for (double nm = 400.0; nm <= 750.0; nm += 1.0) {
        const int current = rank(engine.fromWavelength(nanometresToMetres(nm)).rgb);
        CHECK(current >= previous);
        previous = current;
    }
    CHECK_EQ(previous, 2);  // termina no vermelho

    // E as tres fases realmente ocorrem, em vez de o teste passar por vacuidade.
    CHECK_EQ(rank(engine.fromWavelength(410e-9).rgb), 0);
    CHECK_EQ(rank(engine.fromWavelength(530e-9).rgb), 1);
    CHECK_EQ(rank(engine.fromWavelength(700e-9).rgb), 2);
}

TEST(cie, bordas_do_visivel_nao_ficam_pretas_nem_brancas) {
    // Com normalizacao de luminancia, as bordas precisam continuar coloridas e
    // saturadas. Se algo colapsar (gamut ceifando demais, luminancia estourando),
    // elas viram branco -- que foi outro sintoma do mesmo bug de gamut.
    const ColorEngine engine;
    for (double nm : {400.0, 410.0, 740.0, 750.0}) {
        const Rgb rgb = engine.fromWavelength(nanometresToMetres(nm)).rgb;
        const int total = rgb.r + rgb.g + rgb.b;
        const int maximum = std::max({rgb.r, rgb.g, rgb.b});
        const int minimum = std::min({rgb.r, rgb.g, rgb.b});
        CHECK(total > 0);                 // nao preto
        CHECK(maximum - minimum > 100);   // nao cinza nem branco: e saturado
    }
}

TEST(cie, gamut_preserva_o_matiz_em_vez_de_ceifar_canal_a_canal) {
    // Regressao do segundo bug encontrado: dessaturar e depois CEIFAR o canal
    // que estourou muda as razoes entre canais, ou seja, muda o matiz. Em 650 nm
    // isso transformava vermelho em magenta (255,0,198).
    const ColorEngine engine;
    const Rgb red = engine.fromWavelength(650e-9).rgb;
    CHECK(red.r > 200);
    CHECK(red.b < red.r / 2);  // um vermelho, nao um magenta
}
