#include "soundwave/render/image.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>

#ifdef SOUNDWAVE_WITH_ZLIB
#include <zlib.h>
#endif

namespace soundwave {
namespace {

std::uint32_t crc32(const std::uint8_t* data, std::size_t length, std::uint32_t crc = 0xFFFFFFFFU) {
    static std::uint32_t table[256];
    static bool ready = false;
    if (!ready) {
        for (std::uint32_t n = 0; n < 256; ++n) {
            std::uint32_t c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1U) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
            table[n] = c;
        }
        ready = true;
    }
    for (std::size_t i = 0; i < length; ++i) {
        crc = table[(crc ^ data[i]) & 0xFFU] ^ (crc >> 8);
    }
    return crc;
}

std::uint32_t adler32(const std::vector<std::uint8_t>& data) {
    std::uint32_t a = 1;
    std::uint32_t b = 0;
    for (std::uint8_t byte : data) {
        a = (a + byte) % 65521U;
        b = (b + a) % 65521U;
    }
    return (b << 16) | a;
}

void appendBigEndian32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

void appendChunk(std::vector<std::uint8_t>& out, const char type[4],
                 const std::vector<std::uint8_t>& body) {
    appendBigEndian32(out, static_cast<std::uint32_t>(body.size()));
    const std::size_t crcStart = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), body.begin(), body.end());
    const std::uint32_t crc =
        crc32(out.data() + crcStart, out.size() - crcStart) ^ 0xFFFFFFFFU;
    appendBigEndian32(out, crc);
}

// Fluxo zlib para o chunk IDAT.
//
// Com zlib presente (SOUNDWAVE_WITH_ZLIB), comprime de verdade -- um
// espectrograma tipico cai de ~1,8 MB para algumas dezenas de kB.
//
// Sem zlib, usa apenas blocos deflate "stored" (BTYPE 00). O resultado e um PNG
// valido e universalmente decodificavel, so que grande. Esse caminho existe para
// que o nucleo continue compilando e gravando imagens em uma maquina sem nenhuma
// dependencia instalada -- a mesma garantia que vale para a FFT e o leitor WAV.
std::vector<std::uint8_t> deflateStream(const std::vector<std::uint8_t>& raw) {
#ifdef SOUNDWAVE_WITH_ZLIB
    uLongf capacity = compressBound(static_cast<uLong>(raw.size()));
    std::vector<std::uint8_t> out(capacity);
    const int status = compress2(out.data(), &capacity, raw.data(),
                                 static_cast<uLong>(raw.size()), Z_BEST_COMPRESSION);
    if (status == Z_OK) {
        out.resize(capacity);
        return out;
    }
    // Falha de compressao nao pode custar a imagem: cai para o caminho stored.
#endif
    std::vector<std::uint8_t> z;
    z.push_back(0x78);  // CMF: deflate, janela de 32k
    z.push_back(0x01);  // FLG: sem dicionario; (0x7801 % 31) == 0, como exige a spec
    constexpr std::size_t kMaxBlock = 65535;
    if (raw.empty()) {
        z.insert(z.end(), {0x01, 0x00, 0x00, 0xFF, 0xFF});  // bloco final vazio
    }
    for (std::size_t offset = 0; offset < raw.size(); offset += kMaxBlock) {
        const std::size_t length = std::min(kMaxBlock, raw.size() - offset);
        const bool last = (offset + length) >= raw.size();
        z.push_back(last ? 1 : 0);  // BFINAL, BTYPE = 00 (stored)
        z.push_back(static_cast<std::uint8_t>(length & 0xFFU));
        z.push_back(static_cast<std::uint8_t>((length >> 8) & 0xFFU));
        z.push_back(static_cast<std::uint8_t>(~length & 0xFFU));
        z.push_back(static_cast<std::uint8_t>((~length >> 8) & 0xFFU));
        z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset),
                 raw.begin() + static_cast<std::ptrdiff_t>(offset + length));
    }
    appendBigEndian32(z, adler32(raw));
    return z;
}

}  // namespace

Image::Image(std::size_t width, std::size_t height, Rgb fillColour)
    : width_(width), height_(height), pixels_(width * height * 3) {
    fill(fillColour);
}

void Image::set(std::size_t x, std::size_t y, Rgb colour) {
    if (x >= width_ || y >= height_) return;
    const std::size_t index = (y * width_ + x) * 3;
    pixels_[index] = colour.r;
    pixels_[index + 1] = colour.g;
    pixels_[index + 2] = colour.b;
}

Rgb Image::get(std::size_t x, std::size_t y) const {
    if (x >= width_ || y >= height_) return Rgb{0, 0, 0};
    const std::size_t index = (y * width_ + x) * 3;
    return Rgb{pixels_[index], pixels_[index + 1], pixels_[index + 2]};
}

void Image::fill(Rgb colour) {
    for (std::size_t i = 0; i + 2 < pixels_.size(); i += 3) {
        pixels_[i] = colour.r;
        pixels_[i + 1] = colour.g;
        pixels_[i + 2] = colour.b;
    }
}

void Image::fillRect(std::size_t x, std::size_t y, std::size_t w, std::size_t h, Rgb colour) {
    for (std::size_t dy = 0; dy < h; ++dy) {
        for (std::size_t dx = 0; dx < w; ++dx) set(x + dx, y + dy, colour);
    }
}

void Image::drawVerticalLine(std::size_t x, std::size_t y0, std::size_t y1, Rgb colour) {
    if (y0 > y1) std::swap(y0, y1);
    for (std::size_t y = y0; y <= y1 && y < height_; ++y) set(x, y, colour);
}

void Image::drawHorizontalLine(std::size_t y, std::size_t x0, std::size_t x1, Rgb colour) {
    if (x0 > x1) std::swap(x0, x1);
    for (std::size_t x = x0; x <= x1 && x < width_; ++x) set(x, y, colour);
}

void Image::drawLine(long long x0, long long y0, long long x1, long long y1, Rgb colour) {
    // Bresenham em inteiros: sem acumulo de erro de ponto flutuante, e o
    // resultado e identico em qualquer plataforma (importa para os testes de
    // reprodutibilidade, que comparam imagens byte a byte).
    const long long dx = std::llabs(x1 - x0);
    const long long dy = -std::llabs(y1 - y0);
    const long long sx = x0 < x1 ? 1 : -1;
    const long long sy = y0 < y1 ? 1 : -1;
    long long err = dx + dy;

    while (true) {
        if (x0 >= 0 && y0 >= 0) {
            set(static_cast<std::size_t>(x0), static_cast<std::size_t>(y0), colour);
        }
        if (x0 == x1 && y0 == y1) break;
        const long long e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

bool writePng(const std::string& path, const Image& image, std::string* error) {
    if (image.empty()) {
        if (error) *error = "imagem vazia";
        return false;
    }

    // Dado cru do PNG: cada linha precedida por um byte de filtro. Usamos o
    // filtro 0 (None) -- sem filtragem preditiva, ja que nao comprimimos.
    std::vector<std::uint8_t> raw;
    raw.reserve(image.height() * (1 + image.width() * 3));
    for (std::size_t y = 0; y < image.height(); ++y) {
        raw.push_back(0);
        const std::uint8_t* row = image.data().data() + y * image.width() * 3;
        raw.insert(raw.end(), row, row + image.width() * 3);
    }

    const std::vector<std::uint8_t> z = deflateStream(raw);

    std::vector<std::uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

    std::vector<std::uint8_t> ihdr;
    appendBigEndian32(ihdr, static_cast<std::uint32_t>(image.width()));
    appendBigEndian32(ihdr, static_cast<std::uint32_t>(image.height()));
    ihdr.push_back(8);  // bits por canal
    ihdr.push_back(2);  // tipo de cor 2 = RGB verdadeiro
    ihdr.push_back(0);  // compressao: deflate
    ihdr.push_back(0);  // metodo de filtro
    ihdr.push_back(0);  // sem entrelacamento
    appendChunk(png, "IHDR", ihdr);
    appendChunk(png, "IDAT", z);
    appendChunk(png, "IEND", {});

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        if (error) *error = "nao foi possivel gravar em: " + path;
        return false;
    }
    file.write(reinterpret_cast<const char*>(png.data()),
               static_cast<std::streamsize>(png.size()));
    if (!file.good()) {
        if (error) *error = "falha de escrita em: " + path;
        return false;
    }
    return true;
}

bool writePpm(const std::string& path, const Image& image, std::string* error) {
    if (image.empty()) {
        if (error) *error = "imagem vazia";
        return false;
    }
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        if (error) *error = "nao foi possivel gravar em: " + path;
        return false;
    }
    file << "P6\n" << image.width() << " " << image.height() << "\n255\n";
    file.write(reinterpret_cast<const char*>(image.data().data()),
               static_cast<std::streamsize>(image.data().size()));
    if (!file.good()) {
        if (error) *error = "falha de escrita em: " + path;
        return false;
    }
    return true;
}

}  // namespace soundwave
