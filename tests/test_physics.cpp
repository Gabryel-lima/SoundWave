#include "test_framework.hpp"

#include <algorithm>
#include <cmath>

#include "soundwave/mapping/physical_mappers.hpp"
#include "soundwave/physics/medium.hpp"
#include "soundwave/color/em_band.hpp"
#include "soundwave/mapping/mappers.hpp"
#include "soundwave/physics/medium_filter.hpp"
#include "soundwave/physics/survival.hpp"

using namespace soundwave;

// =============================================================================
// VALIDACAO DOS MODELOS EMPIRICOS
//
// Cada teste confere o port em C++ contra um valor de referencia obtido de uma
// implementacao publicada ou de uma tabela publicada. As referencias estao
// citadas no proprio teste, para que nenhuma constante do projeto dependa de
// memoria.
// =============================================================================

TEST(physics, mackenzie_reproduz_os_valores_de_referencia) {
    // Referencia: doctests de arlpy.uwa.soundspeed (implementacao publicada de
    // Mackenzie 1981), reproduzidos em Python com 4 casas.
    Conditions c;
    c.temperatureC = 27.0;
    c.salinityPpt = 35.0;
    c.depthM = 10.0;
    CHECK_NEAR(soundSpeedMs(media::seaWater(), c), 1539.0866, 1e-3);

    c.temperatureC = 25.0;
    c.depthM = 20.0;
    CHECK_NEAR(soundSpeedMs(media::seaWater(), c), 1534.6204, 1e-3);
}

TEST(physics, agua_doce_usa_salinidade_zero) {
    Conditions c;
    c.temperatureC = 20.0;
    c.depthM = 0.0;
    // Mackenzie com S=0: 1481.7382 m/s. Proximo do valor tabelado de ~1482 m/s
    // para agua doce a 20 C, ainda que S=0 esteja fora da faixa ajustada --
    // limitacao declarada em soundSpeedClaim().
    CHECK_NEAR(soundSpeedMs(media::freshWater(), c), 1481.7382, 1e-3);
    // E a agua do mar, nas mesmas condicoes, e mais rapida.
    CHECK(soundSpeedMs(media::seaWater(), c) > soundSpeedMs(media::freshWater(), c));
}

TEST(physics, velocidade_do_som_no_ar_bate_com_o_valor_classico) {
    Conditions c;
    c.temperatureC = 0.0;
    CHECK_NEAR(soundSpeedMs(media::air(), c), 331.3, 1e-9);
    c.temperatureC = 20.0;
    // Valor de referencia amplamente citado: 343,2 m/s a 20 C.
    CHECK_NEAR(soundSpeedMs(media::air(), c), 343.2, 0.1);
}

TEST(physics, vacuo_nao_propaga_som) {
    const Medium vacuum = media::vacuum();
    CHECK(!vacuum.carriesSound());
    CHECK_NEAR(soundSpeedMs(vacuum, Conditions{}), 0.0, 0.0);
    CHECK_NEAR(soundWavelengthM(vacuum, 440.0, Conditions{}), 0.0, 0.0);
    // E o mapeador de escala recusa esse meio em vez de devolver lixo.
    bool threw = false;
    try {
        ScaleMapper bad(media::vacuum(), media::vacuum(), Conditions{}, 1e-8);
        (void)bad;
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

TEST(physics, francois_garrison_reproduz_o_valor_de_referencia) {
    // Referencia: doctest de arlpy.uwa.absorption -- 50 kHz a (27 C, 35 ppt,
    // 10 m, pH 8,1) da 10,71 dB em 1000 m.
    Conditions c;
    c.temperatureC = 27.0;
    c.salinityPpt = 35.0;
    c.depthM = 10.0;
    c.pH = 8.1;
    const double dbPerKm = soundAbsorptionDbPerMetre(media::seaWater(), 50000.0, c) * 1000.0;
    CHECK_NEAR(dbPerKm, 10.7103, 1e-3);
}

TEST(physics, iso9613_reproduz_a_tabela_publicada) {
    // Referencia: tabela de absorcao atmosferica da ISO 9613-2, a 10 C, 70% UR,
    // 101,325 kPa, em dB/km. Tolerancia de 2% cobre o arredondamento da tabela.
    Conditions c;
    c.temperatureC = 10.0;
    c.relativeHumidity = 70.0;
    c.pressureKPa = 101.325;

    const struct { double hz; double dbPerKm; } expected[] = {
        {125.0, 0.41}, {250.0, 1.04}, {500.0, 1.92}, {1000.0, 3.66},
        {2000.0, 9.70}, {4000.0, 33.06}, {8000.0, 118.38},
    };
    for (const auto& [hz, dbPerKm] : expected) {
        const double got = soundAbsorptionDbPerMetre(media::air(), hz, c) * 1000.0;
        CHECK_NEAR(got, dbPerKm, dbPerKm * 0.02);
    }
}

TEST(physics, sellmeier_silica_reproduz_a_linha_d_publicada) {
    // Malitson (1965): n(587,6 nm) = 1,45846 para silica fundida. Este e o
    // teste-ancora mais forte da parte optica: casa em todos os digitos dados.
    CHECK_NEAR(refractiveIndexAt(media::fusedSilica(), 587.6), 1.45846, 1e-5);
}

TEST(physics, sellmeier_agua_confere_com_o_valor_publicado) {
    // Daimon & Masumura (2007) a 20 C devolve 1,333349 em 589,3 nm. O valor
    // classico IAPWS e 1,33299: divergencia REAL entre fontes, de 0,027%, e nao
    // erro de port. A tolerancia abaixo e o valor de Daimon, apertada; a
    // divergencia entre fontes esta declarada em refractiveIndexClaim().
    CHECK_NEAR(refractiveIndexAt(media::freshWater(), 589.3), 1.333349, 1e-5);
    const double relativeGap = std::abs(1.333349 - 1.33299) / 1.33299;
    CHECK(relativeGap < 3e-4);
    CHECK(refractiveIndexClaim(media::freshWater()).relativeUncertainty >= relativeGap);
}

TEST(physics, dispersao_faz_o_indice_cair_com_o_comprimento_de_onda) {
    // Dispersao normal: n decresce conforme lambda cresce. E o que faz prisma.
    for (const Medium& m : {media::freshWater(), media::fusedSilica()}) {
        double previous = 1e9;
        for (double nm = 400.0; nm <= 750.0; nm += 25.0) {
            const double n = refractiveIndexAt(m, nm);
            CHECK(n < previous);
            CHECK(n > 1.0);
            previous = n;
        }
    }
}

TEST(physics, sellmeier_e_preso_na_borda_fora_da_faixa_de_validade) {
    // Um polo de Sellmeier fora da faixa produz numeros sem sentido. Preferimos
    // prender na borda a devolver n < 1 ou negativo.
    const Medium water = media::freshWater();
    CHECK_NEAR(refractiveIndexAt(water, 100.0), refractiveIndexAt(water, 182.0), 1e-12);
    CHECK_NEAR(refractiveIndexAt(water, 5000.0), refractiveIndexAt(water, 1129.0), 1e-12);
    CHECK_NEAR(refractiveIndexAt(water, -1.0), 1.0, 0.0);
}

// =============================================================================
// A INVARIANCIA -- o resultado central
// =============================================================================

TEST(physics, razao_acustica_e_exatamente_invariante_ao_meio) {
    // O resultado que derruba a ideia de "usar o meio para evitar a compressao":
    // v cancela na razao entre os extremos, entao TODO meio da exatamente 1000.
    Conditions c;
    for (const Medium& m : media::all()) {
        if (!m.carriesSound()) continue;
        const RangeSpan span = acousticSpan(m, 20.0, 20000.0, c);
        CHECK_NEAR(span.ratio, 1000.0, 1e-9);
        CHECK_NEAR(span.octaves, std::log2(1000.0), 1e-9);
    }
}

TEST(physics, razao_optica_e_invariante_exceto_pela_dispersao) {
    // Sem dispersao (vacuo, ar, gelo com n constante) a razao e exatamente a de
    // vacuo. Com dispersao ela muda -- e o efeito e de ~1%, nao de 11x.
    for (const Medium& m : {media::vacuum(), media::air(), media::ice()}) {
        CHECK_NEAR(opticalSpan(m, 400.0, 750.0).ratio, 1.875, 1e-9);
    }

    const RangeSpan water = opticalSpan(media::freshWater(), 400.0, 750.0);
    CHECK_NEAR(water.ratio, 1.894806, 1e-5);
    const double change = std::abs(water.ratio - 1.875) / 1.875;
    CHECK(change > 0.005);   // o efeito e real...
    CHECK(change < 0.02);    // ...e e de ~1%, longe de fechar o fator 11
}

TEST(physics, o_meio_muda_cada_lambda_mas_nao_a_razao) {
    // Verificacao complementar: os comprimentos de onda individuais MUDAM
    // bastante entre meios. Nao e que nada aconteca -- e que a razao nao muda.
    Conditions c;
    const RangeSpan air = acousticSpan(media::air(), 20.0, 20000.0, c);
    const RangeSpan sea = acousticSpan(media::seaWater(), 20.0, 20000.0, c);
    CHECK(sea.atLowFrequencyM > air.atLowFrequencyM * 3.0);  // ~17 m vs ~74 m
    CHECK_NEAR(sea.ratio, air.ratio, 1e-9);
}

TEST(physics, nao_existe_meio_com_som_audivel_em_lambda_visivel) {
    // Para lambda = 550 nm a 20 kHz seria preciso v_som ~ 0,011 m/s; a 20 Hz,
    // ~1,1e-5 m/s. O meio mais lento entre os catalogados esta na casa de
    // centenas de m/s. Faltam varias ordens de grandeza.
    Conditions c;
    const double needed = 20000.0 * 550e-9;
    CHECK(needed < 0.02);
    for (const Medium& m : media::all()) {
        if (!m.carriesSound()) continue;
        CHECK(soundSpeedMs(m, c) > needed * 1000.0);
    }
}

// =============================================================================
// MAPEADORES FISICOS
// =============================================================================

TEST(physics, identidade_preserva_energia_por_quantum) {
    const IdentityMapper mapper;
    CHECK_NEAR(mapper.map(440.0), 440.0, 0.0);
    CHECK_NEAR(mapper.map(20000.0), 20000.0, 0.0);

    // E = h*f. Em 440 Hz: 6,62607015e-34 * 440 = 2,9155e-31 J.
    CHECK_NEAR(IdentityMapper::quantumEnergyJoules(440.0), 2.91547e-31, 1e-35);
    // Em eV, isso e ~1,82e-12 eV -- doze ordens de grandeza abaixo de um foton
    // visivel (~2,25 eV a 550 nm).
    CHECK(IdentityMapper::quantumEnergyElectronVolts(440.0) < 1e-11);

    const double visiblePhotonEv =
        IdentityMapper::quantumEnergyElectronVolts(kSpeedOfLight / 550e-9);
    CHECK_NEAR(visiblePhotonEv, 2.25, 0.02);
}

TEST(physics, identidade_leva_todo_o_audivel_para_radio) {
    const IdentityMapper mapper;
    for (double f : {20.0, 440.0, 20000.0}) {
        CHECK(classifyByFrequency(mapper.map(f)) == EmBand::Radio);
    }
    // E e exata: sem incerteza, sem escolha.
    CHECK(mapper.fidelity().tier == Fidelity::Exact);
    CHECK(mapper.inverse(mapper.map(440.0)).value() == 440.0);
}

TEST(physics, escala_ancorada_atinge_a_ancora) {
    const ScaleMapper mapper = ScaleMapper::anchored(media::air(), media::vacuum(), Conditions{},
                                                     20.0, 750.0);
    const double em = mapper.map(20.0);
    const double nm = kSpeedOfLight / em * 1e9;
    CHECK_NEAR(nm, 750.0, 1e-6);
}

TEST(physics, escala_so_leva_nove_porcento_do_audivel_ao_visivel) {
    // O resultado que quantifica o preco da honestidade fisica: escala pura e
    // isometria em espaco logaritmico, entao a janela visivel captura
    // exatamente a sua propria largura, 0,907 oitava de 9,966.
    const ScaleMapper mapper = ScaleMapper::anchored(media::air(), media::vacuum(), Conditions{},
                                                     20.0, 750.0);
    const ScaleMapper::VisibleWindow window = mapper.visibleWindow();
    CHECK(window.valid());
    CHECK_NEAR(window.lowHz, 20.0, 0.01);
    CHECK_NEAR(window.highHz, 37.5, 0.01);
    CHECK_NEAR(window.octaves, std::log2(1.875), 1e-6);
    CHECK_NEAR(window.fractionOfAudible, 0.091, 0.002);
}

TEST(physics, escala_manda_o_resto_para_uv_e_raios_x) {
    const ScaleMapper mapper = ScaleMapper::anchored(media::air(), media::vacuum(), Conditions{},
                                                     20.0, 750.0);
    CHECK(classifyByFrequency(mapper.map(30.0)) == EmBand::Visible);
    CHECK(classifyByFrequency(mapper.map(440.0)) == EmBand::Ultraviolet);
    CHECK(classifyByFrequency(mapper.map(20000.0)) == EmBand::XRay);
}

TEST(physics, a_janela_visivel_da_escala_nao_depende_do_meio) {
    // Corolario direto da invariancia: trocar o meio acustico muda K (via a
    // ancora) mas NAO muda quantas oitavas cabem no visivel.
    const Conditions c;
    double referenceOctaves = -1.0;
    for (const Medium& m : {media::air(), media::seaWater(), media::ice()}) {
        const ScaleMapper mapper = ScaleMapper::anchored(m, media::vacuum(), c, 20.0, 750.0);
        const ScaleMapper::VisibleWindow window = mapper.visibleWindow();
        CHECK(window.valid());
        CHECK_NEAR(window.lowHz, 20.0, 0.01);
        CHECK_NEAR(window.highHz, 37.5, 0.01);
        if (referenceOctaves < 0.0) referenceOctaves = window.octaves;
        CHECK_NEAR(window.octaves, referenceOctaves, 1e-9);
    }
}

TEST(physics, escala_e_invertivel) {
    const ScaleMapper mapper = ScaleMapper::anchored(media::seaWater(), media::freshWater(),
                                                     Conditions{}, 100.0, 550.0);
    for (double f : {25.0, 100.0, 440.0}) {
        const auto back = mapper.inverse(mapper.map(f));
        CHECK(back.has_value());
        CHECK_NEAR(*back, f, f * 1e-6);
    }
}

TEST(physics, escala_usa_a_dispersao_real_do_meio_optico) {
    // Com o mesmo K, um meio optico dispersivo produz f_EM diferente do vacuo.
    const Conditions c;
    const double scale = 4.373e-8;
    const ScaleMapper inVacuum(media::air(), media::vacuum(), c, scale);
    const ScaleMapper inWater(media::air(), media::freshWater(), c, scale);
    CHECK(std::abs(inVacuum.map(30.0) - inWater.map(30.0)) > 1e12);
}

// =============================================================================
// FILTRAGEM PELO MEIO -- o antagonismo
// =============================================================================

TEST(physics, agua_e_passa_baixa_para_som) {
    Conditions c;
    c.temperatureC = 20.0;
    c.salinityPpt = 35.0;
    const Medium sea = media::seaWater();

    double previous = -1.0;
    for (double f : {100.0, 1000.0, 10000.0, 50000.0}) {
        const double alpha = soundAbsorptionDbPerMetre(sea, f, c);
        CHECK(alpha > previous);  // absorcao cresce com a frequencia
        previous = alpha;
    }
    // Graves atravessam distancias enormes; agudos morrem perto.
    CHECK(acousticTransmittance(sea, 100.0, 100000.0, c) > 0.5);    // 100 Hz, 100 km
    CHECK(acousticTransmittance(sea, 50000.0, 10000.0, c) < 0.01);  // 50 kHz, 10 km
}

TEST(physics, agua_e_passa_alta_para_luz) {
    const Medium sea = media::seaWater();
    // Vermelho morre em metros, azul atravessa dezenas de metros.
    const double redAlpha = opticalAbsorptionDbPerMetre(sea, 700.0);
    const double blueAlpha = opticalAbsorptionDbPerMetre(sea, 475.0);
    CHECK(redAlpha > blueAlpha * 10.0);

    CHECK(opticalTransmittance(sea, 700.0, 10.0) < 0.1);  // vermelho some em 10 m
    CHECK(opticalTransmittance(sea, 475.0, 10.0) > 0.7);  // azul passa
}

TEST(physics, os_dois_filtros_sao_antagonicos) {
    // O resultado central desta secao: o mesmo meio preserva os GRAVES do som e
    // os AZUIS da luz. Como todo mapeamento monotonico do projeto leva grave em
    // vermelho, o meio destroi exatamente o que o mapeamento preserva.
    Conditions c;
    const Medium sea = media::seaWater();

    const double bestSoundHz = mostTransmittedAudibleHz(sea, 20.0, 20000.0, c);
    const double bestLightNm = mostTransmittedVisibleNm(sea);

    CHECK(bestSoundHz < 100.0);     // som: o extremo GRAVE sobrevive
    CHECK(bestLightNm < 520.0);     // luz: o extremo AZUL sobrevive
    CHECK(bestLightNm > 400.0);

    // E um mapeamento monotonico manda grave -> vermelho, ou seja, manda o que
    // o som preserva para o que a luz destroi.
    const LogMapper mapper;
    const double emOfBestSound = mapper.map(bestSoundHz);
    const double nmOfBestSound = kSpeedOfLight / emOfBestSound * 1e9;
    CHECK(nmOfBestSound > 700.0);   // vermelho profundo
    CHECK(std::abs(nmOfBestSound - bestLightNm) > 200.0);
}

TEST(physics, filtro_acustico_atenua_o_espectro_bin_a_bin) {
    Spectrum spectrum;
    spectrum.sampleRate = 44100.0;
    spectrum.fftSize = 8;
    spectrum.bins = {{100.0, 1.0, 0.0}, {1000.0, 1.0, 0.0}, {10000.0, 1.0, 0.0}};

    MediumFilterSettings settings;
    settings.acoustic = media::seaWater();
    settings.pathLengthM = 20000.0;  // 20 km

    const Spectrum filtered = applyAcousticFilter(spectrum, settings);
    CHECK_EQ(filtered.bins.size(), spectrum.bins.size());
    // Ordenacao preservada e atenuacao crescente com a frequencia.
    CHECK(filtered.bins[0].magnitude > filtered.bins[1].magnitude);
    CHECK(filtered.bins[1].magnitude > filtered.bins[2].magnitude);
    CHECK(filtered.bins[0].magnitude <= 1.0);
    // Frequencia e fase NAO mudam -- so a magnitude.
    for (std::size_t i = 0; i < spectrum.bins.size(); ++i) {
        CHECK(filtered.bins[i].frequencyHz == spectrum.bins[i].frequencyHz);
        CHECK(filtered.bins[i].phaseRad == spectrum.bins[i].phaseRad);
    }
}

TEST(physics, caminho_nulo_nao_atenua) {
    Conditions c;
    CHECK_NEAR(acousticTransmittance(media::seaWater(), 10000.0, 0.0, c), 1.0, 0.0);
    CHECK_NEAR(opticalTransmittance(media::seaWater(), 700.0, 0.0), 1.0, 0.0);
}

TEST(physics, meios_nao_modelados_se_declaram_como_tal) {
    // Devolver zero e aceitavel; fingir que zero significa "transparente" nao e.
    // A reivindicacao tem de dizer que o zero e ignorancia, nao medida.
    const FidelityClaim claim = soundAbsorptionClaim(media::ice());
    CHECK(claim.tier == Fidelity::Arbitrary);
    CHECK(claim.caveat.find("nao sabemos") != std::string::npos);
    CHECK_NEAR(soundAbsorptionDbPerMetre(media::ice(), 1000.0, Conditions{}), 0.0, 0.0);
}

// =============================================================================
// ORCAMENTO DE FIDELIDADE
// =============================================================================

TEST(fidelity, uma_etapa_arbitraria_torna_a_incerteza_indefinida) {
    // A regra central: nao se anexa barra de erro a uma escolha.
    FidelityBudget budget;
    budget.add(claims::speedOfLight());
    budget.add(soundSpeedClaim(media::seaWater()));
    CHECK(!budget.hasArbitraryStep());
    CHECK(budget.combinedRelativeUncertainty() > 0.0);
    CHECK(std::isfinite(budget.combinedRelativeUncertainty()));

    budget.add(claims::arbitraryMapping("logarithmic"));
    CHECK(budget.hasArbitraryStep());
    CHECK(std::isnan(budget.combinedRelativeUncertainty()));
    CHECK(budget.weakestTier() == Fidelity::Arbitrary);
}

TEST(fidelity, cadeia_puramente_fisica_tem_incerteza_definida) {
    FidelityBudget budget;
    budget.add(claims::speedOfLight());
    budget.add(claims::wavelengthFromFrequency());
    budget.add(soundSpeedClaim(media::seaWater()));
    budget.add(refractiveIndexClaim(media::fusedSilica()));

    CHECK(!budget.hasArbitraryStep());
    const double combined = budget.combinedRelativeUncertainty();
    CHECK(std::isfinite(combined));
    CHECK(combined > 0.0);
    CHECK(combined < 0.01);  // tudo aqui e muito bem medido
    CHECK(budget.weakestTier() == Fidelity::ModelFit);
}

TEST(fidelity, soma_quadratica_confere) {
    FidelityBudget budget;
    budget.add({"a", Fidelity::Measured, 0.03, "", "", ""});
    budget.add({"b", Fidelity::Measured, 0.04, "", "", ""});
    CHECK_NEAR(budget.combinedRelativeUncertainty(), 0.05, 1e-12);  // 3-4-5
}

TEST(fidelity, ordem_das_classes_e_a_esperada) {
    CHECK(fidelityRank(Fidelity::Exact) < fidelityRank(Fidelity::Measured));
    CHECK(fidelityRank(Fidelity::Measured) < fidelityRank(Fidelity::ModelFit));
    CHECK(fidelityRank(Fidelity::ModelFit) < fidelityRank(Fidelity::Convention));
    CHECK(fidelityRank(Fidelity::Convention) < fidelityRank(Fidelity::Arbitrary));
}

TEST(fidelity, escala_declara_exatamente_uma_etapa_arbitraria) {
    const ScaleMapper mapper = ScaleMapper::anchored(media::air(), media::vacuum(), Conditions{},
                                                     20.0, 750.0);
    const FidelityBudget budget = mapper.fidelity();
    int arbitrary = 0;
    for (const FidelityClaim& claim : budget.claims()) {
        if (claim.tier == Fidelity::Arbitrary) ++arbitrary;
    }
    // Esse e o argumento a favor do ScaleMapper: UMA escolha, nao uma funcao
    // inteira inventada.
    CHECK_EQ(arbitrary, 1);
    CHECK(std::isnan(budget.combinedRelativeUncertainty()));
}

TEST(fidelity, absorcao_da_agua_declara_incerteza_por_faixa) {
    // A limitacao de Segelstein na janela de transparencia nao pode ficar
    // escondida atras de um numero unico.
    const Medium sea = media::seaWater();
    const FidelityClaim blue = opticalAbsorptionClaim(sea, 440.0);
    const FidelityClaim green = opticalAbsorptionClaim(sea, 550.0);

    CHECK(blue.relativeUncertainty > 1.0);      // ordem de grandeza apenas
    CHECK(green.relativeUncertainty < 0.2);     // ~10% no verde
    CHECK(blue.caveat.find("TRANSPARENCIA") != std::string::npos);
    CHECK(blue.caveat.find("Pope") != std::string::npos);
}

TEST(fidelity, relatorio_menciona_o_elo_mais_fraco) {
    FidelityBudget budget;
    budget.add(claims::speedOfLight());
    budget.add(claims::arbitraryMapping("scale"));
    const std::string report = budget.report();
    CHECK(report.find("INDEFINIDA") != std::string::npos);
    CHECK(report.find("arbitrario") != std::string::npos);
    CHECK(report.find("NAO e uma medida") != std::string::npos);
}

// =============================================================================
// SOBREVIVENCIA CONJUNTA E ALINHAMENTO
//
// Esta secao documenta um RESULTADO NULO de forma verificavel. A hipotese era:
// como os dois filtros do meio sao antagonicos, existiria um mapeamento
// (provavelmente invertido) que os alinha e maximiza a informacao que sobrevive
// aos dois trajetos.
//
// A medida diz que nao. E a razao e mais forte que "o ganho e pequeno".
// =============================================================================

namespace {

MediumFilterSettings seaAt(double pathM) {
    MediumFilterSettings settings;
    settings.acoustic = media::seaWater();
    settings.optical = media::seaWater();
    settings.conditions.temperatureC = 20.0;
    settings.conditions.salinityPpt = 35.0;
    settings.pathLengthM = pathM;
    return settings;
}

}  // namespace

TEST(survival, pontua_qualquer_mapeador) {
    const MediumFilterSettings settings = seaAt(10.0);
    const MappingDomain domain;
    const LogMapper mapper(domain);
    const SurvivalScore score = jointSurvival(mapper, settings, domain);

    CHECK(score.joint > 0.0);
    CHECK(score.joint <= score.ceiling);          // nunca supera o teto
    CHECK(score.efficiency > 0.0);
    CHECK(score.efficiency <= 1.0);
    CHECK_NEAR(score.visibleFraction, 1.0, 1e-9); // log mapeia todo o audivel no visivel
    CHECK_NEAR(score.opticalBestNm, 485.0, 15.0); // o "azul" da agua
}

TEST(survival, linear_pontua_muito_pior_que_log) {
    // Sanidade do criterio: ele tem de penalizar o contra-exemplo conhecido.
    // O linear empilha quase toda a banda audivel no extremo vermelho, que e
    // exatamente onde a agua absorve.
    const MediumFilterSettings settings = seaAt(10.0);
    const MappingDomain domain;
    const double logScore = jointSurvival(LogMapper(domain), settings, domain).joint;
    const double linearScore = jointSurvival(LinearMapper(domain), settings, domain).joint;
    CHECK(linearScore < logScore * 0.5);
}

TEST(survival, mapeamento_fora_do_visivel_pontua_zero) {
    // Um mapeamento que joga tudo fora do visivel nao pode ganhar por omissao:
    // a fracao visivel cai a zero, e o objetivo tambem.
    const MediumFilterSettings settings = seaAt(10.0);
    const MappingDomain domain;
    const IdentityMapper identity(domain);
    const SurvivalScore score = jointSurvival(identity, settings, domain);
    CHECK_NEAR(score.visibleFraction, 0.0, 1e-9);
    CHECK_NEAR(score.joint, 0.0, 1e-12);
}

TEST(survival, alinhamento_nao_traz_ganho_em_agua) {
    // O RESULTADO NULO, medido. Os quatro candidatos empatam dentro de 1%.
    const AlignedMapper aligned(seaAt(10.0), MappingDomain{});
    const auto& scores = aligned.allScores();

    double lowest = 1e9;
    double highest = 0.0;
    for (const SurvivalScore& score : scores) {
        lowest = std::min(lowest, score.joint);
        highest = std::max(highest, score.joint);
    }
    CHECK(lowest > 0.0);
    CHECK(highest / lowest < 1.01);  // empate tecnico
}

TEST(survival, a_razao_do_empate_e_a_simetria_da_medida) {
    // Por que empatam: nas distancias em que a luz discrimina, T_som e quase
    // constante em toda a banda. Entao J vira a media de T_luz sobre a imagem --
    // e os quatro candidatos induzem a MESMA medida no visivel, so percorrida em
    // ordens diferentes. O empate e por simetria, nao coincidencia.
    const MediumFilterSettings settings = seaAt(10.0);
    Conditions c = settings.conditions;

    const double low = acousticTransmittance(settings.acoustic, 20.0, settings.pathLengthM, c);
    const double high = acousticTransmittance(settings.acoustic, 20000.0, settings.pathLengthM, c);
    CHECK(low / high < 1.01);   // T_som praticamente constante na banda
    CHECK(high > 0.99);
}

TEST(survival, janelas_de_transicao_nao_se_sobrepoem) {
    // O resultado central: nao ha distancia em que os DOIS filtros discriminem.
    const ScaleSeparation separation = scaleSeparation(seaAt(10.0), MappingDomain{});

    CHECK(separation.acoustic.found);
    CHECK(separation.optical.found);
    CHECK(!separation.overlaps);

    // Luz discrimina em torno de ~0,7 m; som em torno de ~6 km.
    CHECK(separation.optical.centreM() < 5.0);
    CHECK(separation.acoustic.centreM() > 1000.0);
    CHECK(separation.separationDecades > 3.0);
}

TEST(survival, agua_doce_separa_ainda_mais) {
    // Agua doce nao tem o termo de sulfato de magnesio, entao o som viaja ainda
    // mais longe -- e a separacao PIORA. A conclusao e robusta ao meio.
    MediumFilterSettings fresh = seaAt(10.0);
    fresh.acoustic = media::freshWater();
    fresh.optical = media::freshWater();

    const ScaleSeparation sea = scaleSeparation(seaAt(10.0), MappingDomain{});
    const ScaleSeparation lake = scaleSeparation(fresh, MappingDomain{});
    CHECK(!lake.overlaps);
    CHECK(lake.separationDecades > sea.separationDecades);
}

TEST(survival, meio_sem_absorcao_modelada_nao_inventa_janela) {
    // Ar nao tem absorcao optica modelada. O correto e nao encontrar janela,
    // em vez de reportar uma janela infinita.
    MediumFilterSettings air;
    air.acoustic = media::air();
    air.optical = media::air();
    const ScaleSeparation separation = scaleSeparation(air, MappingDomain{});
    CHECK(separation.acoustic.found);   // ISO 9613-1 esta modelada
    CHECK(!separation.optical.found);   // absorcao optica do ar, nao
    CHECK(!separation.overlaps);
}

TEST(survival, alinhado_e_determinista) {
    // Com quatro candidatos quase empatados, o desempate precisa ser estavel --
    // caso contrario duas execucoes escolheriam mapeamentos diferentes.
    const AlignedMapper a(seaAt(10.0), MappingDomain{});
    const AlignedMapper b(seaAt(10.0), MappingDomain{});
    CHECK(a.chosen() == b.chosen());
    CHECK(a.map(440.0) == b.map(440.0));
}

TEST(survival, alinhado_declara_o_resultado_nulo) {
    const AlignedMapper aligned(seaAt(10.0), MappingDomain{});
    const std::string text = aligned.describe();
    CHECK(text.find("NAO e um mapeamento fisico") != std::string::npos);
    CHECK(text.find("por medida") != std::string::npos);

    // E continua Arbitrary -- otimizar contra um criterio nao torna fisico.
    const FidelityBudget budget = aligned.fidelity();
    CHECK(budget.hasArbitraryStep());
    CHECK(std::isnan(budget.combinedRelativeUncertainty()));
}

TEST(survival, alinhado_respeita_o_candidato_escolhido) {
    const AlignedMapper aligned(seaAt(10.0), MappingDomain{});
    // map() tem de concordar com evaluate() do candidato vencedor.
    for (double f : {25.0, 440.0, 8000.0}) {
        CHECK(aligned.map(f) == aligned.evaluate(aligned.chosen(), f));
    }
    // E os candidatos descendentes de fato invertem a orientacao.
    const double lowAsc = aligned.evaluate(AlignedMapper::Candidate::LogAscending, 20.0);
    const double highAsc = aligned.evaluate(AlignedMapper::Candidate::LogAscending, 20000.0);
    const double lowDesc = aligned.evaluate(AlignedMapper::Candidate::LogDescending, 20.0);
    CHECK(lowAsc < highAsc);
    CHECK_NEAR(lowDesc, highAsc, highAsc * 1e-9);
}
