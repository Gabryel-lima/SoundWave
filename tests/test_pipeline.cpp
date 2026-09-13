#include "test_framework.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>

#include "soundwave/audio/signal_generator.hpp"
#include "soundwave/core/config.hpp"
#include "soundwave/dsp/analyzer.hpp"
#include "soundwave/render/image.hpp"
#include "soundwave/render/interpretation.hpp"
#include "soundwave/mapping/mappers.hpp"
#include "soundwave/render/plots.hpp"

using namespace soundwave;

namespace {

struct Pipeline {
    PcmBuffer signal;
    std::vector<Spectrum> frames;
    std::unique_ptr<FrequencyMapper> mapper;
    ColorEngine engine;
    std::vector<FrameInterpretation> interpretations;
};

Pipeline runPipeline(const PcmBuffer& signal, AnalysisConfig config) {
    Pipeline pipeline;
    pipeline.signal = signal;
    const SpectrumAnalyzer analyzer(makeAnalyzerSettings(config), signal.sampleRate);
    pipeline.frames = analyzer.analyzeAll(signal);
    pipeline.mapper = makeMapper(config);
    pipeline.engine = ColorEngine(makeColorEngineSettings(config));
    const Interpreter interpreter(*pipeline.mapper, pipeline.engine,
                                  makeInterpreterSettings(config));
    pipeline.interpretations = interpreter.interpretAll(pipeline.frames);
    return pipeline;
}

}  // namespace

TEST(pipeline, senoide_de_440hz_atravessa_as_quatro_camadas) {
    // Percurso completo com um valor conhecido, verificando cada junta.
    const PcmBuffer signal = signals::sine(440.0, 0.5, 44100.0);
    const Pipeline pipeline = runPipeline(signal, AnalysisConfig{});

    CHECK(!pipeline.interpretations.empty());
    const FrameInterpretation& frame = pipeline.interpretations[2];

    CHECK(!frame.silent);
    CHECK_NEAR(frame.dominantSourceHz, 440.0, 2.0);          // camada 1
    CHECK(frame.colour.emFrequencyHz > kVisibleMinHz);        // camada 2
    CHECK(frame.colour.emFrequencyHz < kVisibleMaxHz);
    CHECK(frame.colour.band == EmBand::Visible);              // camada 3
    CHECK(!frame.colour.isFalseColour);
    CHECK(frame.colour.rgb.r + frame.colour.rgb.g + frame.colour.rgb.b > 0);

    // Confere a cadeia inteira a mao: 440 Hz -> log -> lambda.
    // A f_EM do quadro vem do pico INTERPOLADO (~440,0x Hz), nao de 440,0
    // exatos, entao ela so bate com map(440.0) dentro da tolerancia da FFT.
    const double em = pipeline.mapper->map(440.0);
    CHECK_NEAR(frame.colour.emFrequencyHz, em, em * 0.01);

    // Ja lambda = c / f_EM e uma identidade exata e tem de valer ao nivel do
    // arredondamento de double -- conferida contra a f_EM que o resultado de
    // fato carrega, nao contra uma recalculada a partir de 440,0.
    CHECK_NEAR(frame.colour.wavelengthM, kSpeedOfLight / frame.colour.emFrequencyHz, 1e-20);
}

TEST(pipeline, silencio_e_marcado_e_nao_ganha_cor) {
    const PcmBuffer signal = signals::silence(0.5, 44100.0);
    const Pipeline pipeline = runPipeline(signal, AnalysisConfig{});

    for (const FrameInterpretation& frame : pipeline.interpretations) {
        CHECK(frame.silent);
        CHECK_EQ(static_cast<int>(frame.colour.rgb.r), 0);
        CHECK_EQ(static_cast<int>(frame.colour.rgb.g), 0);
        CHECK_EQ(static_cast<int>(frame.colour.rgb.b), 0);
    }
}

TEST(pipeline, mesma_entrada_e_configuracao_dao_resultado_identico) {
    // Criterio de reprodutibilidade do plano (secao 10), verificado bit a bit e
    // nao "dentro de uma tolerancia": o pipeline e inteiramente determinista.
    const PcmBuffer signal = signals::chord({220.0, 277.18, 329.63}, 0.5, 44100.0);
    AnalysisConfig config;
    config.interpretation.mode = "weighted";

    const Pipeline a = runPipeline(signal, config);
    const Pipeline b = runPipeline(signal, config);

    CHECK_EQ(a.interpretations.size(), b.interpretations.size());
    for (std::size_t i = 0; i < a.interpretations.size(); ++i) {
        CHECK(a.interpretations[i].colour.rgb.r == b.interpretations[i].colour.rgb.r);
        CHECK(a.interpretations[i].colour.rgb.g == b.interpretations[i].colour.rgb.g);
        CHECK(a.interpretations[i].colour.rgb.b == b.interpretations[i].colour.rgb.b);
        CHECK(a.interpretations[i].dominantSourceHz == b.interpretations[i].dominantSourceHz);
        CHECK(a.interpretations[i].colour.emFrequencyHz ==
              b.interpretations[i].colour.emFrequencyHz);
    }
}

TEST(pipeline, imagens_geradas_sao_identicas_entre_execucoes) {
    const PcmBuffer signal = signals::logSweep(100.0, 8000.0, 0.5, 44100.0);
    AnalysisConfig config;
    config.render.width = 200;
    config.render.height = 120;

    const Pipeline pipeline = runPipeline(signal, config);
    PlotSettings plot;
    plot.width = config.render.width;
    plot.height = config.render.height;

    const Image a =
        renderSpectrogram(pipeline.frames, *pipeline.mapper, pipeline.engine, plot);
    const Image b =
        renderSpectrogram(pipeline.frames, *pipeline.mapper, pipeline.engine, plot);
    CHECK_EQ(a.data().size(), b.data().size());
    CHECK(a.data() == b.data());
}

TEST(pipeline, trocar_o_mapeamento_troca_o_resultado) {
    // A demonstracao central do projeto: a cor vem da funcao escolhida, nao do
    // som. Se log e linear produzissem a mesma coisa, o projeto nao teria objeto.
    const PcmBuffer signal = signals::sine(440.0, 0.3, 44100.0);

    AnalysisConfig logConfig;
    logConfig.mapping.type = "logarithmic";
    AnalysisConfig linearConfig;
    linearConfig.mapping.type = "linear";

    const Pipeline logRun = runPipeline(signal, logConfig);
    const Pipeline linearRun = runPipeline(signal, linearConfig);

    // O dado MEDIDO e identico: mesma FFT, mesma frequencia dominante.
    CHECK_NEAR(logRun.interpretations[2].dominantSourceHz,
               linearRun.interpretations[2].dominantSourceHz, 1e-12);

    // A cor, nao. 440 Hz em escala linear fica pertissimo do minimo audivel.
    const Rgb logColour = logRun.interpretations[2].colour.rgb;
    const Rgb linearColour = linearRun.interpretations[2].colour.rgb;
    const int distance = std::abs(logColour.r - linearColour.r) +
                         std::abs(logColour.g - linearColour.g) +
                         std::abs(logColour.b - linearColour.b);
    CHECK(distance > 60);
}

TEST(pipeline, modo_octave_da_a_mesma_cor_para_oitavas) {
    // Verificacao ponta a ponta da propriedade que motiva o OctaveMapper.
    AnalysisConfig config;
    config.mapping.type = "octave";
    config.mapping.referenceHz = 440.0;

    const Pipeline low = runPipeline(signals::sine(440.0, 0.3, 44100.0), config);
    const Pipeline high = runPipeline(signals::sine(880.0, 0.3, 44100.0), config);

    const Rgb a = low.interpretations[2].colour.rgb;
    const Rgb b = high.interpretations[2].colour.rgb;
    // Nao exigimos igualdade exata: a frequencia detectada tem erro de FFT, que
    // se propaga para a cor. Exigimos proximidade forte.
    CHECK(std::abs(a.r - b.r) <= 3);
    CHECK(std::abs(a.g - b.g) <= 3);
    CHECK(std::abs(a.b - b.b) <= 3);
}

TEST(pipeline, modo_logarithmic_NAO_da_a_mesma_cor_para_oitavas) {
    // O contraponto do teste acima. Se este falhasse, o LogMapper estaria
    // acidentalmente periodico e a documentacao estaria errada.
    AnalysisConfig config;
    config.mapping.type = "logarithmic";

    const Pipeline low = runPipeline(signals::sine(440.0, 0.3, 44100.0), config);
    const Pipeline high = runPipeline(signals::sine(880.0, 0.3, 44100.0), config);

    const Rgb a = low.interpretations[2].colour.rgb;
    const Rgb b = high.interpretations[2].colour.rgb;
    const int distance = std::abs(a.r - b.r) + std::abs(a.g - b.g) + std::abs(a.b - b.b);
    CHECK(distance > 5);
}

TEST(pipeline, modo_dominant_e_weighted_diferem_em_sinal_complexo) {
    const PcmBuffer signal = signals::harmonicSeries(220.0, 8, 0.5, 44100.0);

    AnalysisConfig dominantConfig;
    dominantConfig.interpretation.mode = "dominant";
    AnalysisConfig weightedConfig;
    weightedConfig.interpretation.mode = "weighted";

    const Pipeline dominant = runPipeline(signal, dominantConfig);
    const Pipeline weighted = runPipeline(signal, weightedConfig);

    CHECK(dominant.interpretations[2].contributions.empty());
    CHECK(!weighted.interpretations[2].contributions.empty());

    // Os harmonicos puxam a cor ponderada para longe da cor da fundamental.
    const Rgb a = dominant.interpretations[2].colour.rgb;
    const Rgb b = weighted.interpretations[2].colour.rgb;
    CHECK(std::abs(a.r - b.r) + std::abs(a.g - b.g) + std::abs(a.b - b.b) > 5);
}

TEST(pipeline, pesos_do_modo_weighted_somam_um) {
    AnalysisConfig config;
    config.interpretation.mode = "weighted";
    const Pipeline pipeline = runPipeline(signals::chord({220.0, 330.0, 440.0}, 0.5, 44100.0),
                                          config);

    const FrameInterpretation& frame = pipeline.interpretations[2];
    CHECK(!frame.contributions.empty());
    double sum = 0.0;
    for (const BinContribution& contribution : frame.contributions) sum += contribution.weight;
    CHECK_NEAR(sum, 1.0, 1e-9);
}

TEST(pipeline, expoente_de_peso_alto_aproxima_do_modo_dominant) {
    // Propriedade util e nao obvia: o expoente interpola continuamente entre
    // Weighted e Dominant. Vale testar para que ninguem a quebre sem perceber.
    const PcmBuffer signal = signals::harmonicSeries(220.0, 8, 0.5, 44100.0);

    AnalysisConfig dominantConfig;
    dominantConfig.interpretation.mode = "dominant";
    AnalysisConfig softConfig;
    softConfig.interpretation.mode = "weighted";
    softConfig.interpretation.weightExponent = 1.0;
    AnalysisConfig sharpConfig;
    sharpConfig.interpretation.mode = "weighted";
    sharpConfig.interpretation.weightExponent = 12.0;

    const Rgb dominant = runPipeline(signal, dominantConfig).interpretations[2].colour.rgb;
    const Rgb soft = runPipeline(signal, softConfig).interpretations[2].colour.rgb;
    const Rgb sharp = runPipeline(signal, sharpConfig).interpretations[2].colour.rgb;

    auto distance = [](Rgb a, Rgb b) {
        return std::abs(a.r - b.r) + std::abs(a.g - b.g) + std::abs(a.b - b.b);
    };
    CHECK(distance(sharp, dominant) < distance(soft, dominant));
}

TEST(pipeline, politica_discard_produz_bins_pretos_sem_quebrar) {
    AnalysisConfig config;
    config.mapping.outOfRange = "discard";
    config.mapping.sourceMinHz = 200.0;
    config.mapping.sourceMaxHz = 2000.0;
    config.interpretation.mode = "weighted";

    // Sinal com componentes dentro e fora da faixa mapeada.
    const PcmBuffer signal = signals::chord({100.0, 500.0, 8000.0}, 0.5, 44100.0);
    const Pipeline pipeline = runPipeline(signal, config);

    const FrameInterpretation& frame = pipeline.interpretations[3];
    bool sawDiscarded = false;
    for (const BinContribution& contribution : frame.contributions) {
        if (std::isnan(contribution.emHz)) {
            sawDiscarded = true;
            CHECK_NEAR(contribution.weight, 0.0, 0.0);  // descartado nao pesa
        }
    }
    CHECK(sawDiscarded);
    CHECK(!frame.silent);
}

TEST(pipeline, alvo_fora_do_visivel_produz_pseudocor_declarada) {
    // Integracao entre a politica de banda e o rotulo de procedencia: se o
    // mapeamento apontar para o infravermelho, o resultado precisa DIZER que a
    // cor e inventada, ate o fim do pipeline.
    AnalysisConfig config;
    config.mapping.targetMinHz = 1.0e13;
    config.mapping.targetMaxHz = 3.0e13;
    config.color.mode = "visible-with-bands";

    const Pipeline pipeline = runPipeline(signals::sine(440.0, 0.3, 44100.0), config);
    const FrameInterpretation& frame = pipeline.interpretations[2];
    CHECK(frame.colour.band == EmBand::Infrared);
    CHECK(frame.colour.isFalseColour);
}

TEST(pipeline, renderizadores_produzem_imagens_do_tamanho_pedido) {
    const Pipeline pipeline =
        runPipeline(signals::sine(440.0, 0.3, 44100.0), AnalysisConfig{});
    PlotSettings plot;
    plot.width = 320;
    plot.height = 180;

    for (const Image& image :
         {renderSpectrogram(pipeline.frames, *pipeline.mapper, pipeline.engine, plot),
          renderColorTimeline(pipeline.interpretations, plot),
          renderSpectrumPlot(pipeline.frames[2], *pipeline.mapper, pipeline.engine, plot),
          renderMappingRuler(*pipeline.mapper, pipeline.engine, plot)}) {
        CHECK_EQ(image.width(), std::size_t{320});
        CHECK_EQ(image.height(), std::size_t{180});
        CHECK_EQ(image.data().size(), std::size_t{320 * 180 * 3});
    }
}

TEST(pipeline, renderizadores_lidam_com_entrada_vazia) {
    const std::unique_ptr<FrequencyMapper> mapper = std::make_unique<LogMapper>();
    const ColorEngine engine;
    PlotSettings plot;
    plot.width = 64;
    plot.height = 32;

    const Image spectrogram = renderSpectrogram({}, *mapper, engine, plot);
    const Image timeline = renderColorTimeline({}, plot);
    CHECK_EQ(spectrogram.width(), std::size_t{64});
    CHECK_EQ(timeline.width(), std::size_t{64});
}

TEST(pipeline, png_gerado_tem_assinatura_e_chunks_validos) {
    Image image(8, 4, Rgb{10, 20, 30});
    image.set(0, 0, Rgb{255, 0, 0});

    const auto path = std::filesystem::temp_directory_path() / "soundwave_test.png";
    std::string error;
    CHECK(writePng(path.string(), image, &error));

    std::ifstream file(path, std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());
    CHECK(bytes.size() > 60);

    const unsigned char signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    for (int i = 0; i < 8; ++i) CHECK_EQ(static_cast<int>(bytes[i]), static_cast<int>(signature[i]));

    const std::string content(bytes.begin(), bytes.end());
    CHECK(content.find("IHDR") != std::string::npos);
    CHECK(content.find("IDAT") != std::string::npos);
    CHECK(content.find("IEND") != std::string::npos);
    std::filesystem::remove(path);
}

TEST(pipeline, imagem_recorta_acessos_fora_dos_limites) {
    Image image(4, 4, Rgb{0, 0, 0});
    image.set(100, 100, Rgb{255, 255, 255});  // nao pode escrever fora
    image.drawLine(-50, -50, 50, 50, Rgb{255, 0, 0});
    CHECK_EQ(static_cast<int>(image.get(100, 100).r), 0);
    CHECK_EQ(static_cast<int>(image.get(0, 0).r), 255);  // a linha passa pela diagonal
}
