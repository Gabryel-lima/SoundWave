#include "test_framework.hpp"

#include <cmath>

#include "soundwave/audio/signal_generator.hpp"
#include "soundwave/dsp/analyzer.hpp"

using namespace soundwave;

namespace {

AnalyzerSettings defaultSettings(std::size_t fftSize = 4096) {
    AnalyzerSettings settings;
    settings.fftSize = fftSize;
    settings.hopSize = fftSize / 4;
    settings.window = WindowType::Hann;
    return settings;
}

}  // namespace

TEST(analyzer, resolucao_e_fs_sobre_n) {
    const SpectrumAnalyzer analyzer(defaultSettings(4096), 44100.0);
    CHECK_NEAR(analyzer.resolutionHz(), 44100.0 / 4096.0, 1e-12);
    CHECK_NEAR(analyzer.resolutionHz(), 10.7666015625, 1e-9);
}

TEST(analyzer, detecta_440hz_dentro_de_um_bin) {
    // Criterio do plano (secao 10): |f_detectada - f_esperada| <= df.
    const PcmBuffer signal = signals::sine(440.0, 1.0, 44100.0);
    const SpectrumAnalyzer analyzer(defaultSettings(4096), 44100.0);
    const Spectrum spectrum = analyzer.analyzeBlock(signal.samples, 0);

    const auto peak = dominantBin(spectrum, 20.0, 20000.0);
    CHECK(peak.has_value());
    CHECK(std::abs(peak->frequencyHz - 440.0) <= spectrum.resolutionHz());
}

TEST(analyzer, interpolacao_parabolica_supera_o_criterio_do_plano) {
    // O criterio "<= df" e fraco: com df = 10,77 Hz ele aceita qualquer coisa
    // entre 429 e 451 Hz. A interpolacao parabolica sobre o log-magnitude
    // reduz o erro para bem menos de um decimo de bin, e testamos ESSE limite
    // -- caso contrario uma regressao de precisao passaria despercebida.
    const PcmBuffer signal = signals::sine(440.0, 1.0, 44100.0);
    const SpectrumAnalyzer analyzer(defaultSettings(4096), 44100.0);
    const Spectrum spectrum = analyzer.analyzeBlock(signal.samples, 0);

    const auto coarse = dominantBin(spectrum, 20.0, 20000.0);
    const auto refined = dominantPeak(spectrum, 20.0, 20000.0);
    CHECK(coarse.has_value());
    CHECK(refined.has_value());

    const double coarseError = std::abs(coarse->frequencyHz - 440.0);
    const double refinedError = std::abs(refined->frequencyHz - 440.0);
    CHECK(refinedError < coarseError);
    CHECK(refinedError < spectrum.resolutionHz() * 0.05);
}

TEST(analyzer, deteccao_precisa_em_varias_frequencias) {
    const double sampleRate = 44100.0;
    const SpectrumAnalyzer analyzer(defaultSettings(4096), sampleRate);

    for (double frequency : {110.0, 440.0, 1000.0, 2000.0, 5000.0}) {
        const PcmBuffer signal = signals::sine(frequency, 0.5, sampleRate);
        const Spectrum spectrum = analyzer.analyzeBlock(signal.samples, 0);
        const auto peak = dominantPeak(spectrum, 20.0, 20000.0);
        CHECK(peak.has_value());
        CHECK_NEAR(peak->frequencyHz, frequency, spectrum.resolutionHz() * 0.1);
    }
}

TEST(analyzer, magnitude_recupera_a_amplitude_da_senoide) {
    // A normalizacao pelo ganho da janela tem de devolver a amplitude ORIGINAL.
    // Sem o fator 2 (juntando as frequencias +f e -f) o resultado sairia pela
    // metade -- erro silencioso que so um teste assim pega.
    const double sampleRate = 48000.0;
    const SpectrumAnalyzer analyzer(defaultSettings(8192), sampleRate);

    for (double amplitude : {0.1, 0.25, 0.5, 0.9}) {
        const PcmBuffer signal = signals::sine(1000.0, 0.5, sampleRate, amplitude);
        const Spectrum spectrum = analyzer.analyzeBlock(signal.samples, 0);
        const auto peak = dominantPeak(spectrum, 20.0, 20000.0);
        CHECK(peak.has_value());
        CHECK_NEAR(peak->magnitude, amplitude, amplitude * 0.02);
    }
}

TEST(analyzer, silencio_produz_magnitude_nula) {
    const PcmBuffer signal = signals::silence(0.5, 44100.0);
    const SpectrumAnalyzer analyzer(defaultSettings(), 44100.0);
    const Spectrum spectrum = analyzer.analyzeBlock(signal.samples, 0);

    for (const SpectralBin& bin : spectrum.bins) CHECK_NEAR(bin.magnitude, 0.0, 1e-15);
    CHECK_NEAR(bandEnergy(spectrum, 20.0, 20000.0), 0.0, 1e-20);
}

TEST(analyzer, remocao_de_continua_elimina_o_pico_em_dc) {
    PcmBuffer signal = signals::sine(440.0, 0.5, 44100.0, 0.3);
    for (double& sample : signal.samples) sample += 0.5;  // offset grande

    AnalyzerSettings settings = defaultSettings();
    settings.removeDc = true;
    const SpectrumAnalyzer withRemoval(settings, 44100.0);
    settings.removeDc = false;
    const SpectrumAnalyzer withoutRemoval(settings, 44100.0);

    const Spectrum cleaned = withRemoval.analyzeBlock(signal.samples, 0);
    const Spectrum raw = withoutRemoval.analyzeBlock(signal.samples, 0);

    // A remocao de continua NAO zera o bin DC, e esperar isso e um erro: a
    // media e removida ANTES do janelamento, e a janela pondera as amostras de
    // forma desigual, entao o bloco janelado volta a ter media levemente nao
    // nula. O que se garante e uma reducao de varias ordens de grandeza.
    CHECK(raw.bins[0].magnitude > 0.4);
    CHECK(cleaned.bins[0].magnitude < raw.bins[0].magnitude / 100.0);
    CHECK(cleaned.bins[0].magnitude < 1e-2);

    // E o mais importante: com a continua presente, ela DOMINA a deteccao.
    const auto rawPeak = dominantBin(raw, 0.0, 20000.0);
    const auto cleanPeak = dominantPeak(cleaned, 20.0, 20000.0);
    CHECK(rawPeak.has_value());
    CHECK(cleanPeak.has_value());
    CHECK_NEAR(cleanPeak->frequencyHz, 440.0, cleaned.resolutionHz() * 0.1);
}

TEST(analyzer, acorde_produz_tres_picos_distintos) {
    const double sampleRate = 44100.0;
    const PcmBuffer signal = signals::chord({261.63, 329.63, 392.00}, 1.0, sampleRate);
    const SpectrumAnalyzer analyzer(defaultSettings(8192), sampleRate);
    const Spectrum spectrum = analyzer.analyzeBlock(signal.samples, 0);

    const std::vector<SpectralPeak> peaks = findPeaks(spectrum, 20.0, 20000.0, 8, 0.2);
    CHECK(peaks.size() >= 3);

    const double tolerance = spectrum.resolutionHz();
    for (double expected : {261.63, 329.63, 392.00}) {
        bool found = false;
        for (const SpectralPeak& peak : peaks) {
            if (std::abs(peak.frequencyHz - expected) <= tolerance) found = true;
        }
        CHECK(found);
    }
}

TEST(analyzer, serie_harmonica_tem_fundamental_dominante) {
    const double sampleRate = 44100.0;
    const PcmBuffer signal = signals::harmonicSeries(220.0, 6, 1.0, sampleRate);
    const SpectrumAnalyzer analyzer(defaultSettings(8192), sampleRate);
    const Spectrum spectrum = analyzer.analyzeBlock(signal.samples, 0);

    const auto peak = dominantPeak(spectrum, 20.0, 20000.0);
    CHECK(peak.has_value());
    CHECK_NEAR(peak->frequencyHz, 220.0, spectrum.resolutionHz() * 0.2);

    // Com queda 1/n, os harmonicos ficam presentes mas mais fracos.
    const std::vector<SpectralPeak> peaks = findPeaks(spectrum, 20.0, 20000.0, 8, 0.05);
    CHECK(peaks.size() >= 4);
}

TEST(analyzer, ruido_branco_espalha_energia_por_todo_o_espectro) {
    const double sampleRate = 44100.0;
    const PcmBuffer signal = signals::whiteNoise(1.0, sampleRate);
    const SpectrumAnalyzer analyzer(defaultSettings(4096), sampleRate);
    const Spectrum spectrum = analyzer.analyzeBlock(signal.samples, 0);

    // Bandas de largura comparavel em escala linear devem ter energia da mesma
    // ordem. Nao exigimos igualdade: uma unica realizacao de ruido flutua muito.
    const double low = bandEnergy(spectrum, 1000.0, 6000.0);
    const double high = bandEnergy(spectrum, 10000.0, 15000.0);
    CHECK(low > 0.0);
    CHECK(high > 0.0);
    CHECK(low / high > 0.3);
    CHECK(low / high < 3.0);
}

TEST(analyzer, varredura_percorre_o_espectro_ao_longo_do_tempo) {
    const double sampleRate = 44100.0;
    const PcmBuffer signal = signals::logSweep(100.0, 8000.0, 2.0, sampleRate);
    const SpectrumAnalyzer analyzer(defaultSettings(4096), sampleRate);
    const std::vector<Spectrum> frames = analyzer.analyzeAll(signal);
    CHECK(frames.size() > 10);

    const auto early = dominantPeak(frames[2], 20.0, 20000.0);
    const auto late = dominantPeak(frames[frames.size() - 4], 20.0, 20000.0);
    CHECK(early.has_value());
    CHECK(late.has_value());
    CHECK(late->frequencyHz > early->frequencyHz * 4.0);
}

TEST(analyzer, zero_padding_no_ultimo_bloco_nao_quebra) {
    const PcmBuffer signal = signals::sine(440.0, 0.01, 44100.0);  // menor que a FFT
    const SpectrumAnalyzer analyzer(defaultSettings(4096), 44100.0);
    const Spectrum spectrum = analyzer.analyzeBlock(signal.samples, 0);
    CHECK_EQ(spectrum.bins.size(), std::size_t{4096 / 2 + 1});

    const Spectrum beyond = analyzer.analyzeBlock(signal.samples, 100000);
    for (const SpectralBin& bin : beyond.bins) CHECK_NEAR(bin.magnitude, 0.0, 1e-15);
}

TEST(analyzer, contagem_de_quadros_confere_com_analyze_all) {
    const PcmBuffer signal = signals::sine(440.0, 1.0, 44100.0);
    const SpectrumAnalyzer analyzer(defaultSettings(4096), 44100.0);
    CHECK_EQ(analyzer.analyzeAll(signal).size(), analyzer.frameCountFor(signal.samples.size()));
}

TEST(analyzer, tempo_dos_quadros_avanca_pelo_salto) {
    const PcmBuffer signal = signals::sine(440.0, 1.0, 44100.0);
    AnalyzerSettings settings = defaultSettings(4096);
    settings.hopSize = 1024;
    const SpectrumAnalyzer analyzer(settings, 44100.0);
    const std::vector<Spectrum> frames = analyzer.analyzeAll(signal);

    CHECK_NEAR(frames[0].timeOffsetSeconds, 0.0, 1e-12);
    CHECK_NEAR(frames[1].timeOffsetSeconds, 1024.0 / 44100.0, 1e-12);
    CHECK_NEAR(frames[4].timeOffsetSeconds, 4.0 * 1024.0 / 44100.0, 1e-12);
}

TEST(analyzer, fase_de_uma_senoide_e_estavel) {
    // A fase estimada tem de ser reproduzivel: e um campo declarado no plano
    // (phaseRad) e precisa significar alguma coisa, nao ser ruido.
    const double sampleRate = 44100.0;
    const std::size_t fftSize = 4096;
    // Frequencia exatamente no centro de um bin, para nao haver vazamento.
    const double frequency = 100.0 * sampleRate / static_cast<double>(fftSize);

    const SpectrumAnalyzer analyzer(defaultSettings(fftSize), sampleRate);
    const PcmBuffer a = signals::sine(frequency, 0.2, sampleRate, 0.5, 0.0);
    const PcmBuffer b = signals::sine(frequency, 0.2, sampleRate, 0.5, 0.0);

    const Spectrum sa = analyzer.analyzeBlock(a.samples, 0);
    const Spectrum sb = analyzer.analyzeBlock(b.samples, 0);
    CHECK_NEAR(sa.bins[100].phaseRad, sb.bins[100].phaseRad, 1e-15);

    // Deslocar a fase de entrada tem de deslocar a fase estimada na mesma medida.
    const PcmBuffer shifted = signals::sine(frequency, 0.2, sampleRate, 0.5, 1.0);
    const Spectrum ss = analyzer.analyzeBlock(shifted.samples, 0);
    double delta = ss.bins[100].phaseRad - sa.bins[100].phaseRad;
    while (delta > std::numbers::pi) delta -= 2.0 * std::numbers::pi;
    while (delta < -std::numbers::pi) delta += 2.0 * std::numbers::pi;
    CHECK_NEAR(delta, 1.0, 1e-6);
}
