#include "test_framework.hpp"

#include <filesystem>

#include "soundwave/core/config.hpp"

using namespace soundwave;

TEST(config, padroes_sao_validos) {
    const AnalysisConfig config;
    CHECK(validateConfig(config).empty());
}

TEST(config, parser_le_um_documento_completo) {
    const ConfigLoadResult result = parseConfig(R"(
# comentario de cabecalho
analysis:
  fft_size: 8192
  hop_size: 2048
  window: blackman
  remove_dc: false

mapping:
  type: octave
  reference_hz: 432.0
  out_of_range: discard

color:
  mode: visible-only
  gamut: clip

interpretation:
  mode: weighted
  weight_exponent: 2.0
)");

    CHECK(result.ok);
    CHECK(result.warnings.empty());
    CHECK_EQ(result.config.analysis.fftSize, std::size_t{8192});
    CHECK_EQ(result.config.analysis.hopSize, std::size_t{2048});
    CHECK_EQ(result.config.analysis.window, std::string("blackman"));
    CHECK(!result.config.analysis.removeDc);
    CHECK_EQ(result.config.mapping.type, std::string("octave"));
    CHECK_NEAR(result.config.mapping.referenceHz, 432.0, 0.0);
    CHECK_EQ(result.config.mapping.outOfRange, std::string("discard"));
    CHECK_EQ(result.config.color.mode, std::string("visible-only"));
    CHECK_EQ(result.config.interpretation.mode, std::string("weighted"));
    CHECK_NEAR(result.config.interpretation.weightExponent, 2.0, 0.0);
}

TEST(config, chave_desconhecida_gera_aviso_e_nao_silencio) {
    // Um parametro digitado errado que fosse ignorado em silencio faria o
    // manifesto registrar uma configuracao diferente da que rodou.
    const ConfigLoadResult result = parseConfig("analysis:\n  fft_sizee: 1024\n");
    CHECK(result.ok);
    CHECK_EQ(result.warnings.size(), std::size_t{1});
    CHECK(result.warnings[0].find("fft_sizee") != std::string::npos);
    CHECK_EQ(result.config.analysis.fftSize, std::size_t{4096});  // manteve o padrao
}

TEST(config, valor_invalido_gera_aviso_e_mantem_o_padrao) {
    const ConfigLoadResult result = parseConfig("analysis:\n  fft_size: nao-e-numero\n");
    CHECK(result.ok);
    CHECK_EQ(result.warnings.size(), std::size_t{1});
    CHECK_EQ(result.config.analysis.fftSize, std::size_t{4096});
}

TEST(config, chave_indentada_sem_secao_e_erro) {
    const ConfigLoadResult result = parseConfig("  fft_size: 1024\n");
    CHECK(!result.ok);
    CHECK(!result.error.empty());
}

TEST(config, linha_sem_dois_pontos_e_erro) {
    const ConfigLoadResult result = parseConfig("analysis:\n  isto nao tem separador\n");
    CHECK(!result.ok);
}

TEST(config, pontos_de_controle_sao_lidos) {
    const ConfigLoadResult result = parseConfig(
        "mapping:\n  type: custom\n  control_points: [[20, 4.0e14], [20000, 7.5e14]]\n");
    CHECK(result.ok);
    CHECK_EQ(result.config.mapping.controlPoints.size(), std::size_t{2});
    CHECK_NEAR(result.config.mapping.controlPoints[0].first, 20.0, 0.0);
    CHECK_NEAR(result.config.mapping.controlPoints[1].second, 7.5e14, 0.0);
}

TEST(config, validacao_pega_fft_nao_potencia_de_dois) {
    AnalysisConfig config;
    config.analysis.fftSize = 3000;
    const std::vector<std::string> problems = validateConfig(config);
    CHECK(problems.size() == 1);
    CHECK(problems[0].find("potencia de dois") != std::string::npos);
}

TEST(config, validacao_pega_nomes_desconhecidos) {
    AnalysisConfig config;
    config.analysis.window = "janela-inventada";
    config.mapping.type = "tipo-inventado";
    config.color.mode = "modo-inventado";
    CHECK(validateConfig(config).size() == 3);
}

TEST(config, validacao_pega_custom_sem_pontos_suficientes) {
    AnalysisConfig config;
    config.mapping.type = "custom";
    const std::vector<std::string> problems = validateConfig(config);
    CHECK(!problems.empty());
    CHECK(problems[0].find("control_points") != std::string::npos);
}

TEST(config, validacao_pega_salto_maior_que_a_fft) {
    AnalysisConfig config;
    config.analysis.fftSize = 1024;
    config.analysis.hopSize = 4096;
    const std::vector<std::string> problems = validateConfig(config);
    CHECK(!problems.empty());
}

TEST(config, yaml_faz_ida_e_volta_sem_perder_precisao) {
    // Essencial para reprodutibilidade: gravar e reler a configuracao nao pode
    // alterar nenhum valor, nem no ultimo bit.
    AnalysisConfig original;
    original.analysis.fftSize = 8192;
    original.analysis.hopSize = 512;
    original.analysis.window = "blackman-harris";
    original.mapping.type = "custom";
    original.mapping.referenceHz = 432.123456789;
    original.mapping.controlPoints = {{20.0, 4.0e14}, {1000.0, 5.1234e14}, {20000.0, 7.5e14}};
    original.interpretation.weightExponent = 1.7320508075688772;
    original.interpretation.silenceThreshold = 1.2345e-7;

    const ConfigLoadResult reloaded = parseConfig(configToYaml(original));
    CHECK(reloaded.ok);
    CHECK(reloaded.warnings.empty());

    CHECK_EQ(reloaded.config.analysis.fftSize, original.analysis.fftSize);
    CHECK_EQ(reloaded.config.analysis.window, original.analysis.window);
    CHECK(reloaded.config.mapping.referenceHz == original.mapping.referenceHz);
    CHECK(reloaded.config.interpretation.weightExponent == original.interpretation.weightExponent);
    CHECK(reloaded.config.interpretation.silenceThreshold == original.interpretation.silenceThreshold);
    CHECK_EQ(reloaded.config.mapping.controlPoints.size(), std::size_t{3});
    CHECK(reloaded.config.mapping.controlPoints[1].second ==
          original.mapping.controlPoints[1].second);
}

TEST(config, fabrica_constroi_o_mapeador_certo) {
    AnalysisConfig config;
    config.mapping.type = "linear";
    CHECK_EQ(makeMapper(config)->name(), std::string("linear"));
    config.mapping.type = "logarithmic";
    CHECK_EQ(makeMapper(config)->name(), std::string("logarithmic"));
    config.mapping.type = "octave";
    CHECK_EQ(makeMapper(config)->name(), std::string("octave"));

    config.mapping.type = "custom";
    config.mapping.controlPoints = {{20.0, 4.0e14}, {20000.0, 7.5e14}};
    CHECK_EQ(makeMapper(config)->name(), std::string("custom"));
}

TEST(config, fabrica_propaga_o_dominio) {
    AnalysisConfig config;
    config.mapping.sourceMinHz = 50.0;
    config.mapping.sourceMaxHz = 5000.0;
    config.mapping.targetMinHz = 4.5e14;
    config.mapping.targetMaxHz = 7.0e14;

    const std::unique_ptr<FrequencyMapper> mapper = makeMapper(config);
    CHECK_NEAR(mapper->domain().sourceMinHz, 50.0, 0.0);
    CHECK_NEAR(mapper->map(50.0), 4.5e14, 4.5e14 * 1e-12);
    CHECK_NEAR(mapper->map(5000.0), 7.0e14, 7.0e14 * 1e-12);
}

TEST(config, impressao_digital_detecta_mudanca_de_conteudo) {
    const std::vector<double> a = {0.1, 0.2, 0.3};
    const std::vector<double> b = {0.1, 0.2, 0.3};
    const std::vector<double> c = {0.1, 0.2, 0.3000000000000001};

    CHECK_EQ(fingerprintSamples(a), fingerprintSamples(b));
    CHECK(fingerprintSamples(a) != fingerprintSamples(c));
    CHECK(fingerprintSamples(a) != fingerprintSamples({0.1, 0.2}));
    CHECK(fingerprintSamples(a).rfind("fnv1a64:", 0) == 0);
}

TEST(config, manifesto_registra_tudo_que_afeta_o_resultado) {
    RunManifest manifest;
    manifest.soundwaveVersion = versionString();
    manifest.inputPath = "teste.wav";
    manifest.inputFingerprint = fingerprintSamples({0.1, 0.2});
    manifest.inputSampleRate = 44100.0;
    manifest.inputSamples = 2;
    manifest.mapperDescription = "descricao do mapeamento sob teste";

    const std::string yaml = manifestToYaml(manifest);
    CHECK(yaml.find("algorithm_version") != std::string::npos);
    CHECK(yaml.find("input_fingerprint") != std::string::npos);
    CHECK(yaml.find("descricao do mapeamento sob teste") != std::string::npos);
    CHECK(yaml.find("mapping:") != std::string::npos);
    CHECK(yaml.find("interpretation:") != std::string::npos);

    // O manifesto tem de conter uma configuracao que o proprio parser releia.
    const ConfigLoadResult reparsed = parseConfig(configToYaml(manifest.config));
    CHECK(reparsed.ok);
    CHECK(reparsed.warnings.empty());
}

TEST(config, arquivo_inexistente_devolve_erro) {
    const ConfigLoadResult result = loadConfig("/caminho/inexistente/config.yaml");
    CHECK(!result.ok);
    CHECK(!result.error.empty());
}
