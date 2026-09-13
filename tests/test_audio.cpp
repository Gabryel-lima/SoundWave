#include "test_framework.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "soundwave/audio/signal_generator.hpp"
#include "soundwave/audio/wav_reader.hpp"

using namespace soundwave;

namespace {

std::filesystem::path tempPath(const char* name) {
    return std::filesystem::temp_directory_path() / name;
}

}  // namespace

TEST(audio, senoide_tem_amplitude_e_rms_esperados) {
    const PcmBuffer signal = signals::sine(440.0, 1.0, 44100.0, 0.5);
    CHECK_EQ(signal.frameCount(), std::size_t{44100});
    CHECK_NEAR(signal.durationSeconds(), 1.0, 1e-12);
    CHECK_NEAR(signal.peakAmplitude(), 0.5, 0.001);
    // RMS de uma senoide = A / sqrt(2).
    CHECK_NEAR(signal.rmsAmplitude(), 0.5 / std::sqrt(2.0), 0.001);
}

TEST(audio, silencio_e_exatamente_zero) {
    const PcmBuffer signal = signals::silence(0.5, 44100.0);
    CHECK_NEAR(signal.peakAmplitude(), 0.0, 0.0);
    CHECK_NEAR(signal.rmsAmplitude(), 0.0, 0.0);
}

TEST(audio, ruido_e_determinista_com_a_mesma_semente) {
    // Reprodutibilidade tambem vale para o ruido: se ele nao for determinista,
    // nenhum teste que o use pode ser confiavel.
    const PcmBuffer a = signals::whiteNoise(0.1, 44100.0, 0.25, 12345);
    const PcmBuffer b = signals::whiteNoise(0.1, 44100.0, 0.25, 12345);
    const PcmBuffer c = signals::whiteNoise(0.1, 44100.0, 0.25, 999);

    CHECK_EQ(a.frameCount(), b.frameCount());
    for (std::size_t i = 0; i < a.frameCount(); ++i) CHECK(a.samples[i] == b.samples[i]);

    bool differs = false;
    for (std::size_t i = 0; i < a.frameCount(); ++i) {
        if (a.samples[i] != c.samples[i]) differs = true;
    }
    CHECK(differs);
}

TEST(audio, varredura_tem_fase_continua) {
    // Uma varredura mal construida (usando f(t) direto no argumento do seno)
    // tem saltos de fase. Detectamos verificando que amostras consecutivas nunca
    // dao um salto grande demais para a frequencia instantanea maxima.
    const double sampleRate = 44100.0;
    const PcmBuffer sweep = signals::logSweep(100.0, 4000.0, 1.0, sampleRate);

    double maxJump = 0.0;
    for (std::size_t i = 1; i < sweep.samples.size(); ++i) {
        maxJump = std::max(maxJump, std::abs(sweep.samples[i] - sweep.samples[i - 1]));
    }
    // Passo maximo teorico: A * 2*pi*f_max/fs = 0.5 * 2*pi*4000/44100 ~ 0.285.
    CHECK(maxJump < 0.35);
}

TEST(audio, serie_harmonica_respeita_nyquist) {
    // Harmonicos acima de Nyquist seriam rebatidos e apareceriam como
    // frequencias falsas. O gerador tem de para-los antes.
    const double sampleRate = 8000.0;  // Nyquist = 4000 Hz
    const PcmBuffer signal = signals::harmonicSeries(1000.0, 10, 0.5, sampleRate);
    CHECK(signal.peakAmplitude() > 0.0);
    CHECK(signal.peakAmplitude() <= 1.0);
}

TEST(audio, downmix_faz_media_dos_canais) {
    const std::vector<double> stereo = {1.0, 0.0, 0.5, 0.5, -1.0, 1.0};
    const std::vector<double> mono = downmixToMono(stereo, 2);
    CHECK_EQ(mono.size(), std::size_t{3});
    CHECK_NEAR(mono[0], 0.5, 1e-15);
    CHECK_NEAR(mono[1], 0.5, 1e-15);
    CHECK_NEAR(mono[2], 0.0, 1e-15);

    // Mono passa direto, sem copia desnecessaria de semantica.
    const std::vector<double> passthrough = downmixToMono({0.1, 0.2}, 1);
    CHECK_EQ(passthrough.size(), std::size_t{2});
}

TEST(wav, gravar_e_ler_faz_ida_e_volta) {
    const auto path = tempPath("soundwave_roundtrip.wav");
    const PcmBuffer original = signals::sine(440.0, 0.25, 44100.0, 0.5);

    const WavWriteResult written = writeWav16(path.string(), original);
    CHECK(written.ok);
    CHECK_EQ(written.clippedSamples, std::size_t{0});

    const WavLoadResult loaded = loadWavFile(path.string());
    CHECK(loaded.ok);
    CHECK_NEAR(loaded.buffer.sampleRate, 44100.0, 0.0);
    CHECK_EQ(loaded.buffer.frameCount(), original.frameCount());
    CHECK_EQ(loaded.sourceChannels, std::size_t{1});
    CHECK_EQ(loaded.sourceBitDepth, std::size_t{16});

    // Tolerancia do passo de quantizacao de 16 bits (1/32767 ~ 3.05e-5).
    for (std::size_t i = 0; i < original.frameCount(); ++i) {
        CHECK_NEAR(loaded.buffer.samples[i], original.samples[i], 3.1e-5);
    }
    std::filesystem::remove(path);
}

TEST(wav, ceifamento_e_reportado_e_nao_silencioso) {
    const auto path = tempPath("soundwave_clip.wav");
    PcmBuffer loud;
    loud.sampleRate = 44100.0;
    loud.samples = {0.0, 1.5, -2.0, 0.5};

    const WavWriteResult written = writeWav16(path.string(), loud);
    CHECK(written.ok);
    CHECK_EQ(written.clippedSamples, std::size_t{2});
    std::filesystem::remove(path);
}

TEST(wav, arquivo_inexistente_devolve_erro_util) {
    const WavLoadResult loaded = loadWavFile("/caminho/que/nao/existe.wav");
    CHECK(!loaded.ok);
    CHECK(!loaded.error.empty());
    CHECK(loaded.buffer.empty());
}

TEST(wav, arquivo_nao_riff_e_rejeitado) {
    const auto path = tempPath("soundwave_bogus.wav");
    {
        std::ofstream file(path, std::ios::binary);
        file << "isto definitivamente nao e um arquivo WAV valido";
    }
    const WavLoadResult loaded = loadWavFile(path.string());
    CHECK(!loaded.ok);
    CHECK(loaded.error.find("RIFF") != std::string::npos);
    std::filesystem::remove(path);
}

TEST(wav, arquivo_truncado_nao_quebra) {
    const auto path = tempPath("soundwave_truncated.wav");
    {
        std::ofstream file(path, std::ios::binary);
        file << "RIFF";  // curto demais para ter cabecalho
    }
    const WavLoadResult loaded = loadWavFile(path.string());
    CHECK(!loaded.ok);
    CHECK(!loaded.error.empty());
    std::filesystem::remove(path);
}

TEST(wav, chunk_desconhecido_entre_fmt_e_data_e_ignorado) {
    // Arquivos reais de DAW carregam LIST, fact, cue e afins. Um leitor que
    // assuma posicoes fixas falha neles; este percorre os chunks.
    const auto path = tempPath("soundwave_extra_chunk.wav");
    {
        std::ofstream file(path, std::ios::binary);
        auto u32 = [&file](std::uint32_t v) {
            const char bytes[4] = {static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF),
                                   static_cast<char>((v >> 16) & 0xFF),
                                   static_cast<char>((v >> 24) & 0xFF)};
            file.write(bytes, 4);
        };
        auto u16 = [&file](std::uint16_t v) {
            const char bytes[2] = {static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF)};
            file.write(bytes, 2);
        };
        const std::uint32_t dataBytes = 8;
        file.write("RIFF", 4);
        u32(4 + 24 + 12 + 8 + dataBytes);
        file.write("WAVE", 4);
        file.write("fmt ", 4);
        u32(16); u16(1); u16(1); u32(44100); u32(88200); u16(2); u16(16);
        file.write("LIST", 4);  // chunk intruso
        u32(4);
        file.write("INFO", 4);
        file.write("data", 4);
        u32(dataBytes);
        for (int i = 0; i < 4; ++i) u16(static_cast<std::uint16_t>(1000 * i));
    }

    const WavLoadResult loaded = loadWavFile(path.string());
    CHECK(loaded.ok);
    CHECK_EQ(loaded.buffer.frameCount(), std::size_t{4});
    CHECK_NEAR(loaded.buffer.sampleRate, 44100.0, 0.0);
    std::filesystem::remove(path);
}
