#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "soundwave/color/srgb.hpp"

namespace soundwave {

// Imagem RGB de 8 bits por canal, origem no canto superior esquerdo.
class Image {
public:
    Image() = default;
    Image(std::size_t width, std::size_t height, Rgb fill = Rgb{0, 0, 0});

    [[nodiscard]] std::size_t width() const { return width_; }
    [[nodiscard]] std::size_t height() const { return height_; }
    [[nodiscard]] bool empty() const { return pixels_.empty(); }

    // Fora dos limites vira no-op / preto, em vez de comportamento indefinido:
    // o codigo de desenho calcula coordenadas a partir de dados de audio e
    // arredondamentos na borda sao a regra, nao a excecao.
    void set(std::size_t x, std::size_t y, Rgb colour);
    [[nodiscard]] Rgb get(std::size_t x, std::size_t y) const;

    void fill(Rgb colour);
    void fillRect(std::size_t x, std::size_t y, std::size_t w, std::size_t h, Rgb colour);
    void drawVerticalLine(std::size_t x, std::size_t y0, std::size_t y1, Rgb colour);
    void drawHorizontalLine(std::size_t y, std::size_t x0, std::size_t x1, Rgb colour);
    void drawLine(long long x0, long long y0, long long x1, long long y1, Rgb colour);

    [[nodiscard]] const std::vector<std::uint8_t>& data() const { return pixels_; }

private:
    std::size_t width_ = 0;
    std::size_t height_ = 0;
    std::vector<std::uint8_t> pixels_;  // RGB intercalado
};

// Gravacao de PNG com dois caminhos, escolhidos em tempo de compilacao:
//
//  - com zlib (SOUNDWAVE_WITH_ZLIB): compressao deflate real. Um espectrograma
//    tipico cai de ~1,8 MB para algumas dezenas de kB.
//  - sem zlib: blocos deflate "stored", validos e decodificaveis em qualquer
//    lugar, apenas grandes.
//
// O segundo caminho nao e um remendo: e o que garante que o nucleo grave imagem
// em uma maquina sem nenhuma dependencia instalada, a mesma promessa que vale
// para a FFT e para o leitor de WAV. zlib melhora o tamanho, nunca a corretude.
[[nodiscard]] bool writePng(const std::string& path, const Image& image, std::string* error = nullptr);

// PPM (P6): texto de cabecalho + bytes crus. Util para diff binario em testes.
[[nodiscard]] bool writePpm(const std::string& path, const Image& image, std::string* error = nullptr);

}  // namespace soundwave
