#include "test_framework.hpp"

#include <cmath>

#include "soundwave/mapping/mappers.hpp"

using namespace soundwave;

TEST(mapping, dominio_padrao_expoe_a_assimetria_audivel_visivel) {
    // O numero central do projeto. Se ele mudar, docs/mapping.md esta errado.
    const MappingDomain domain;
    CHECK(domain.valid());
    CHECK_NEAR(domain.sourceOctaves(), std::log2(1000.0), 1e-9);  // ~9,966
    CHECK_NEAR(domain.targetOctaves(), std::log2(1.875), 1e-6);   // ~0,907
    CHECK_NEAR(domain.compressionRatio(), 10.99, 0.02);
}

TEST(mapping, dominio_invalido_e_rejeitado) {
    MappingDomain domain;
    domain.sourceMaxHz = domain.sourceMinHz;
    CHECK(!domain.valid());

    bool threw = false;
    try {
        LogMapper bad(domain);
        (void)bad;
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

TEST(mapping, log_atinge_exatamente_as_bordas) {
    const MappingDomain domain;
    const LogMapper mapper(domain);
    CHECK_NEAR(mapper.map(domain.sourceMinHz), domain.targetMinHz, domain.targetMinHz * 1e-12);
    CHECK_NEAR(mapper.map(domain.sourceMaxHz), domain.targetMaxHz, domain.targetMaxHz * 1e-12);
}

TEST(mapping, linear_atinge_exatamente_as_bordas) {
    const MappingDomain domain;
    const LinearMapper mapper(domain);
    CHECK_NEAR(mapper.map(domain.sourceMinHz), domain.targetMinHz, domain.targetMinHz * 1e-12);
    CHECK_NEAR(mapper.map(domain.sourceMaxHz), domain.targetMaxHz, domain.targetMaxHz * 1e-12);
}

TEST(mapping, log_e_linear_sao_estritamente_crescentes) {
    const LogMapper logMapper;
    const LinearMapper linearMapper;
    double previousLog = 0.0;
    double previousLinear = 0.0;

    for (double f = 20.0; f <= 20000.0; f *= 1.05) {
        const double logValue = logMapper.map(f);
        const double linearValue = linearMapper.map(f);
        CHECK(logValue > previousLog);
        CHECK(linearValue > previousLinear);
        previousLog = logValue;
        previousLinear = linearValue;
    }
    CHECK(logMapper.isMonotonic());
    CHECK(linearMapper.isMonotonic());
}

TEST(mapping, log_leva_razoes_iguais_a_razoes_iguais) {
    // Propriedade que define o mapeamento logaritmico: intervalos musicais
    // iguais (mesma razao de frequencia) produzem a mesma razao em f_EM.
    const LogMapper mapper;
    const double a = mapper.map(200.0) / mapper.map(100.0);
    const double b = mapper.map(800.0) / mapper.map(400.0);
    const double c = mapper.map(4000.0) / mapper.map(2000.0);
    CHECK_NEAR(a, b, 1e-12);
    CHECK_NEAR(b, c, 1e-12);
}

TEST(mapping, log_NAO_preserva_equivalencia_de_oitava) {
    // Verificacao explicita da correcao apontada em docs/mapping.md: a oitava,
    // a relacao musical mais forte que existe, vira ~6,5% em f_EM.
    const MappingDomain domain;
    const LogMapper mapper(domain);

    const double ratio = mapper.map(880.0) / mapper.map(440.0);
    const double expected = std::pow(2.0, 1.0 / domain.compressionRatio());
    CHECK_NEAR(ratio, expected, 1e-9);
    CHECK_NEAR(ratio, 1.065, 0.002);

    CHECK(!mapper.preservesOctaveEquivalence());
    CHECK(std::abs(mapper.map(880.0) - mapper.map(440.0)) > 1e6);  // nao sao a mesma cor
}

TEST(mapping, octave_preserva_equivalencia_de_oitava) {
    const OctaveMapper mapper({}, 440.0);
    const double reference = mapper.map(440.0);

    for (double f : {27.5, 55.0, 110.0, 220.0, 440.0, 880.0, 1760.0, 3520.0, 7040.0, 14080.0}) {
        CHECK_NEAR(mapper.map(f), reference, reference * 1e-9);
    }
    CHECK(mapper.preservesOctaveEquivalence());
}

TEST(mapping, octave_nao_e_monotonico_e_nao_tem_inversa) {
    const OctaveMapper mapper;
    CHECK(!mapper.isMonotonic());
    CHECK(!mapper.inverse(5.0e14).has_value());

    // Uma varredura ascendente tem de DESCER em algum ponto: e a descontinuidade
    // ao virar a oitava. Se nao descesse, o mapeador nao seria periodico.
    bool decreased = false;
    double previous = mapper.map(100.0);
    for (double f = 100.0; f <= 1600.0; f *= 1.02) {
        const double current = mapper.map(f);
        if (current < previous - 1e6) decreased = true;
        previous = current;
    }
    CHECK(decreased);
}

TEST(mapping, octave_distingue_notas_dentro_da_oitava) {
    // Preservar oitava nao pode significar colapsar tudo: notas diferentes na
    // mesma oitava precisam continuar tendo cores diferentes.
    const OctaveMapper mapper({}, 440.0);
    CHECK(std::abs(mapper.map(440.0) - mapper.map(523.25)) > 1e12);  // A4 vs C5
    CHECK(std::abs(mapper.map(440.0) - mapper.map(659.25)) > 1e12);  // A4 vs E5
}

TEST(mapping, inversa_do_log_faz_ida_e_volta) {
    const LogMapper mapper;
    for (double f : {20.0, 100.0, 440.0, 1000.0, 5000.0, 20000.0}) {
        const auto back = mapper.inverse(mapper.map(f));
        CHECK(back.has_value());
        CHECK_NEAR(*back, f, f * 1e-9);
    }
}

TEST(mapping, inversa_do_linear_faz_ida_e_volta) {
    const LinearMapper mapper;
    for (double f : {20.0, 440.0, 9000.0, 20000.0}) {
        const auto back = mapper.inverse(mapper.map(f));
        CHECK(back.has_value());
        CHECK_NEAR(*back, f, f * 1e-9);
    }
}

TEST(mapping, inversa_rejeita_valores_fora_da_imagem) {
    const LogMapper mapper;
    CHECK(!mapper.inverse(1.0e10).has_value());   // muito abaixo do visivel
    CHECK(!mapper.inverse(1.0e18).has_value());   // muito acima
    CHECK(!mapper.inverse(-1.0).has_value());
    CHECK(!mapper.inverse(std::nan("")).has_value());
}

TEST(mapping, politica_clamp_prende_nas_bordas) {
    const MappingDomain domain;
    const LogMapper mapper(domain, OutOfRangePolicy::Clamp);
    CHECK_NEAR(mapper.map(1.0), domain.targetMinHz, domain.targetMinHz * 1e-12);
    CHECK_NEAR(mapper.map(1.0e6), domain.targetMaxHz, domain.targetMaxHz * 1e-12);
}

TEST(mapping, politica_discard_devolve_nan) {
    const LogMapper mapper({}, OutOfRangePolicy::Discard);
    CHECK(std::isnan(mapper.map(1.0)));
    CHECK(std::isnan(mapper.map(1.0e6)));
    CHECK(!std::isnan(mapper.map(440.0)));
}

TEST(mapping, politica_extrapolate_sai_do_visivel) {
    // Comportamento desejado: extrapolar produz frequencias EM fora do visivel,
    // que a camada de cor tratara como pseudocor. Nao e erro; e o ponto.
    const MappingDomain domain;
    const LogMapper mapper(domain, OutOfRangePolicy::Extrapolate);
    CHECK(mapper.map(1.0) < domain.targetMinHz);
    CHECK(mapper.map(1.0e6) > domain.targetMaxHz);
}

TEST(mapping, entradas_invalidas_devolvem_nan_em_toda_politica) {
    for (OutOfRangePolicy policy : {OutOfRangePolicy::Clamp, OutOfRangePolicy::Extrapolate,
                                    OutOfRangePolicy::Discard}) {
        const LogMapper mapper({}, policy);
        CHECK(std::isnan(mapper.map(0.0)));
        CHECK(std::isnan(mapper.map(-440.0)));
        CHECK(std::isnan(mapper.map(std::nan(""))));
        CHECK(std::isnan(mapper.map(std::numeric_limits<double>::infinity())));
    }
}

TEST(mapping, custom_interpola_e_respeita_os_pontos_de_controle) {
    const MappingDomain domain;
    const CustomMapper mapper({{20.0, domain.targetMinHz},
                               {1000.0, 5.5e14},
                               {20000.0, domain.targetMaxHz}});

    CHECK_NEAR(mapper.map(20.0), domain.targetMinHz, domain.targetMinHz * 1e-9);
    CHECK_NEAR(mapper.map(1000.0), 5.5e14, 5.5e14 * 1e-9);
    CHECK_NEAR(mapper.map(20000.0), domain.targetMaxHz, domain.targetMaxHz * 1e-9);
    CHECK(mapper.isMonotonic());

    // Entre dois pontos, interpolacao geometrica: a media geometrica das
    // frequencias de origem cai na media geometrica das de destino.
    const double middle = mapper.map(std::sqrt(20.0 * 1000.0));
    CHECK_NEAR(middle, std::sqrt(domain.targetMinHz * 5.5e14), 1e10);
}

TEST(mapping, custom_detecta_pontos_de_controle_nao_monotonicos) {
    const CustomMapper mapper({{20.0, 7.0e14}, {1000.0, 5.0e14}, {20000.0, 6.0e14}});
    CHECK(!mapper.isMonotonic());
    CHECK(mapper.describe().find("NAO monotonico") != std::string::npos);
}

TEST(mapping, custom_rejeita_entradas_degeneradas) {
    bool threwTooFew = false;
    try {
        const CustomMapper mapper({{20.0, 5.0e14}});
        (void)mapper;
    } catch (const std::invalid_argument&) {
        threwTooFew = true;
    }
    CHECK(threwTooFew);

    bool threwDuplicate = false;
    try {
        const CustomMapper mapper({{440.0, 5.0e14}, {440.0, 6.0e14}});
        (void)mapper;
    } catch (const std::invalid_argument&) {
        threwDuplicate = true;
    }
    CHECK(threwDuplicate);
}

TEST(mapping, todo_mapeador_se_descreve_de_forma_util) {
    // O manifesto de reprodutibilidade guarda describe(). Uma descricao vazia ou
    // generica tornaria o manifesto inutil, entao isto e testado.
    const LogMapper logMapper;
    const LinearMapper linearMapper;
    const OctaveMapper octaveMapper;

    for (const FrequencyMapper* mapper :
         {static_cast<const FrequencyMapper*>(&logMapper),
          static_cast<const FrequencyMapper*>(&linearMapper),
          static_cast<const FrequencyMapper*>(&octaveMapper)}) {
        CHECK(!mapper->name().empty());
        CHECK(mapper->describe().size() > 60);
    }
    CHECK(octaveMapper.describe().find("equivalencia de oitava") != std::string::npos);
    CHECK(logMapper.describe().find("NAO preserva equivalencia de oitava") != std::string::npos);
}

TEST(mapping, dominio_alternativo_e_respeitado) {
    // O mapeamento nao pode ter os limites do visivel embutidos: apontar para o
    // infravermelho tem de funcionar, e a camada de cor tratara como pseudocor.
    MappingDomain domain;
    domain.sourceMinHz = 50.0;
    domain.sourceMaxHz = 5000.0;
    domain.targetMinHz = 1.0e13;  // infravermelho
    domain.targetMaxHz = 3.0e13;

    const LogMapper mapper(domain);
    CHECK_NEAR(mapper.map(50.0), 1.0e13, 1.0e13 * 1e-12);
    CHECK_NEAR(mapper.map(5000.0), 3.0e13, 3.0e13 * 1e-12);
}

TEST(mapping, function_mapper_encapsula_uma_lambda) {
    const FunctionMapper mapper([](double f) { return f * 1.0e12; }, "teste: f * 1e12");
    CHECK_NEAR(mapper.map(440.0), 4.4e14, 1.0);
    CHECK_EQ(mapper.describe(), std::string("teste: f * 1e12"));
}
