#pragma once

#include <string>

#include "soundwave/audio/pcm_buffer.hpp"

namespace soundwave {

// Leitor/escritor WAV (RIFF) proprio, sem dependencias.
//
// Cobre o que o MVP precisa: PCM inteiro 8/16/24/32 bits e IEEE float 32/64
// bits, mono ou multicanal, incluindo WAVE_FORMAT_EXTENSIBLE. Nao cobre formatos
// comprimidos dentro de WAV (ADPCM, mu-law) -- para MP3/FLAC/OGG use o backend
// FFmpeg opcional em audio/decoder.hpp.
struct WavLoadResult {
    bool ok = false;
    std::string error;
    PcmBuffer buffer;
    std::size_t sourceChannels = 0;  // antes da mistura para mono
    std::size_t sourceBitDepth = 0;
};

[[nodiscard]] WavLoadResult loadWavFile(const std::string& path);

// Grava WAV PCM 16 bits mono. Usado por `soundwave gen` e pelos testes de
// ida e volta. Amostras fora de [-1,1] sao ceifadas e o retorno informa isso.
struct WavWriteResult {
    bool ok = false;
    std::string error;
    std::size_t clippedSamples = 0;
};

[[nodiscard]] WavWriteResult writeWav16(const std::string& path, const PcmBuffer& buffer);

}  // namespace soundwave
