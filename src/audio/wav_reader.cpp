#include "soundwave/audio/wav_reader.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

namespace soundwave {
namespace {

constexpr std::uint16_t kFormatPcm = 0x0001;
constexpr std::uint16_t kFormatFloat = 0x0003;
constexpr std::uint16_t kFormatExtensible = 0xFFFE;

// WAV e little-endian por especificacao. Lemos byte a byte em vez de fazer
// memcpy de um struct: isso mantem o leitor correto em maquinas big-endian e
// imune a padding do compilador.
std::uint32_t readU32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

std::uint16_t readU16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[0]) |
                                      (static_cast<std::uint16_t>(p[1]) << 8));
}

// Converte uma amostra bruta para double em [-1, 1).
//
// Inteiros com sinal sao divididos por 2^(bits-1), nao por (2^(bits-1) - 1):
// o formato e assimetrico (-32768..32767 em 16 bits) e dividir pelo maximo
// positivo faria o minimo estourar para -1.000030. Preferimos o mapeamento
// exato em potencia de dois, que e o que todo DAW usa.
double decodeSample(const std::uint8_t* p, std::uint16_t format, std::uint16_t bits) {
    if (format == kFormatFloat) {
        if (bits == 32) {
            std::uint32_t raw = readU32(p);
            float value = 0.0F;
            std::memcpy(&value, &raw, sizeof(value));
            return static_cast<double>(value);
        }
        std::uint64_t raw = 0;
        for (int i = 7; i >= 0; --i) raw = (raw << 8) | p[i];
        double value = 0.0;
        std::memcpy(&value, &raw, sizeof(value));
        return value;
    }

    switch (bits) {
        case 8:  // 8 bits em WAV e SEM sinal, deslocado de 128. Excecao da spec.
            return (static_cast<double>(p[0]) - 128.0) / 128.0;
        case 16: {
            const auto value = static_cast<std::int16_t>(readU16(p));
            return static_cast<double>(value) / 32768.0;
        }
        case 24: {
            std::int32_t value = static_cast<std::int32_t>(p[0]) |
                                 (static_cast<std::int32_t>(p[1]) << 8) |
                                 (static_cast<std::int32_t>(p[2]) << 16);
            if (value & 0x800000) value |= ~0xFFFFFF;  // extensao de sinal
            return static_cast<double>(value) / 8388608.0;
        }
        case 32: {
            const auto value = static_cast<std::int32_t>(readU32(p));
            return static_cast<double>(value) / 2147483648.0;
        }
        default:
            return 0.0;
    }
}

}  // namespace

WavLoadResult loadWavFile(const std::string& path) {
    WavLoadResult result;

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        result.error = "nao foi possivel abrir o arquivo: " + path;
        return result;
    }

    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
    if (data.size() < 12) {
        result.error = "arquivo curto demais para ser um RIFF valido";
        return result;
    }
    if (std::memcmp(data.data(), "RIFF", 4) != 0 || std::memcmp(data.data() + 8, "WAVE", 4) != 0) {
        result.error = "assinatura RIFF/WAVE ausente (o arquivo e realmente WAV?)";
        return result;
    }

    std::uint16_t format = 0;
    std::uint16_t channels = 0;
    std::uint32_t sampleRate = 0;
    std::uint16_t bits = 0;
    const std::uint8_t* audio = nullptr;
    std::size_t audioBytes = 0;

    // Percorremos os chunks em vez de assumir que `fmt ` e `data` vem em
    // posicoes fixas: arquivos reais frequentemente carregam LIST/INFO, cue,
    // fact ou metadados de DAW entre eles.
    std::size_t offset = 12;
    while (offset + 8 <= data.size()) {
        const char* id = reinterpret_cast<const char*>(data.data() + offset);
        const std::uint32_t chunkSize = readU32(data.data() + offset + 4);
        const std::size_t body = offset + 8;
        if (body > data.size()) break;
        const std::size_t available = std::min<std::size_t>(chunkSize, data.size() - body);

        if (std::memcmp(id, "fmt ", 4) == 0 && available >= 16) {
            format = readU16(data.data() + body);
            channels = readU16(data.data() + body + 2);
            sampleRate = readU32(data.data() + body + 4);
            bits = readU16(data.data() + body + 14);
            // Em WAVE_FORMAT_EXTENSIBLE o formato real fica nos dois primeiros
            // bytes do GUID do subformato, no fim do bloco de extensao.
            if (format == kFormatExtensible && available >= 40) {
                format = readU16(data.data() + body + 24);
            }
        } else if (std::memcmp(id, "data", 4) == 0) {
            audio = data.data() + body;
            audioBytes = available;
        }

        offset = body + chunkSize + (chunkSize & 1U);  // chunks tem padding par
    }

    if (channels == 0 || sampleRate == 0 || bits == 0) {
        result.error = "chunk 'fmt ' ausente ou invalido";
        return result;
    }
    if (audio == nullptr) {
        result.error = "chunk 'data' ausente";
        return result;
    }
    if (format != kFormatPcm && format != kFormatFloat) {
        result.error = "formato WAV comprimido nao suportado (code " + std::to_string(format) +
                       "); use o backend FFmpeg";
        return result;
    }
    if (format == kFormatPcm && bits != 8 && bits != 16 && bits != 24 && bits != 32) {
        result.error = "profundidade PCM nao suportada: " + std::to_string(bits) + " bits";
        return result;
    }
    if (format == kFormatFloat && bits != 32 && bits != 64) {
        result.error = "float de " + std::to_string(bits) + " bits nao suportado";
        return result;
    }

    const std::size_t bytesPerSample = bits / 8U;
    const std::size_t frameBytes = bytesPerSample * channels;
    const std::size_t frames = frameBytes == 0 ? 0 : audioBytes / frameBytes;

    std::vector<double> interleaved(frames * channels);
    for (std::size_t i = 0; i < frames * channels; ++i) {
        interleaved[i] = decodeSample(audio + i * bytesPerSample, format, bits);
    }

    result.ok = true;
    result.sourceChannels = channels;
    result.sourceBitDepth = bits;
    result.buffer.sampleRate = static_cast<double>(sampleRate);
    result.buffer.samples = downmixToMono(interleaved, channels);
    result.buffer.sourceName = path;
    return result;
}

WavWriteResult writeWav16(const std::string& path, const PcmBuffer& buffer) {
    WavWriteResult result;

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        result.error = "nao foi possivel gravar em: " + path;
        return result;
    }

    const auto frames = static_cast<std::uint32_t>(buffer.samples.size());
    const std::uint32_t dataBytes = frames * 2U;
    const std::uint32_t rate = static_cast<std::uint32_t>(buffer.sampleRate);

    auto putU32 = [&file](std::uint32_t v) {
        const char bytes[4] = {static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF),
                               static_cast<char>((v >> 16) & 0xFF),
                               static_cast<char>((v >> 24) & 0xFF)};
        file.write(bytes, 4);
    };
    auto putU16 = [&file](std::uint16_t v) {
        const char bytes[2] = {static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF)};
        file.write(bytes, 2);
    };

    file.write("RIFF", 4);
    putU32(36U + dataBytes);
    file.write("WAVE", 4);
    file.write("fmt ", 4);
    putU32(16U);
    putU16(kFormatPcm);
    putU16(1U);          // mono
    putU32(rate);
    putU32(rate * 2U);   // bytes por segundo
    putU16(2U);          // alinhamento de bloco
    putU16(16U);         // bits por amostra
    file.write("data", 4);
    putU32(dataBytes);

    for (double s : buffer.samples) {
        if (s > 1.0 || s < -1.0) ++result.clippedSamples;
        const double clamped = std::clamp(s, -1.0, 1.0);
        // Escala por 32767 na gravacao (e nao 32768) para que +1.0 nao estoure.
        const auto value = static_cast<std::int16_t>(std::lround(clamped * 32767.0));
        putU16(static_cast<std::uint16_t>(value));
    }

    result.ok = file.good();
    if (!result.ok) result.error = "falha de escrita em: " + path;
    return result;
}

}  // namespace soundwave
