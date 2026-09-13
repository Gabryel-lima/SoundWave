#include "soundwave/color/srgb.hpp"

#include <algorithm>
#include <cmath>

namespace soundwave {

std::string_view gamutStrategyName(GamutStrategy strategy) {
    switch (strategy) {
        case GamutStrategy::Clip:               return "clip";
        case GamutStrategy::Desaturate:         return "desaturate";
        case GamutStrategy::DesaturateAndScale: return "desaturate-scale";
    }
    return "unknown";
}

bool parseGamutStrategy(std::string_view name, GamutStrategy& out) {
    if (name == "clip") {
        out = GamutStrategy::Clip;
    } else if (name == "desaturate") {
        out = GamutStrategy::Desaturate;
    } else if (name == "desaturate-scale" || name == "desaturate_scale") {
        out = GamutStrategy::DesaturateAndScale;
    } else {
        return false;
    }
    return true;
}

LinearRgb xyzToLinearRgb(const Xyz& c) {
    return LinearRgb{
        3.2404542 * c.x - 1.5371385 * c.y - 0.4985314 * c.z,
        -0.9692660 * c.x + 1.8760108 * c.y + 0.0415560 * c.z,
        0.0556434 * c.x - 0.2040259 * c.y + 1.0572252 * c.z,
    };
}

Xyz linearRgbToXyz(const LinearRgb& c) {
    return Xyz{
        0.4124564 * c.r + 0.3575761 * c.g + 0.1804375 * c.b,
        0.2126729 * c.r + 0.7151522 * c.g + 0.0721750 * c.b,
        0.0193339 * c.r + 0.1191920 * c.g + 0.9503041 * c.b,
    };
}

bool isInGamut(const LinearRgb& c, double tolerance) {
    const double lo = -tolerance;
    const double hi = 1.0 + tolerance;
    return c.r >= lo && c.g >= lo && c.b >= lo && c.r <= hi && c.g <= hi && c.b <= hi;
}

double gamutExcursion(const LinearRgb& c) {
    const double below = -std::min({c.r, c.g, c.b, 0.0});
    const double above = std::max({c.r, c.g, c.b, 1.0}) - 1.0;
    return below + above;
}

LinearRgb mapIntoGamut(const LinearRgb& colour, GamutStrategy strategy) {
    LinearRgb c = colour;

    if (strategy != GamutStrategy::Clip) {
        // Somar a mesma quantidade aos tres canais e caminhar na direcao do
        // branco em linha reta no espaco linear: a cromaticidade se desloca em
        // direcao ao ponto branco, mas o matiz permanece muito mais estavel do
        // que se cortassemos so o canal negativo.
        const double minimum = std::min({c.r, c.g, c.b});
        if (minimum < 0.0) {
            c.r -= minimum;
            c.g -= minimum;
            c.b -= minimum;
        }
    }

    if (strategy != GamutStrategy::Clip) {
        // Depois de dessaturar, o maior componente costuma passar de 1 (uma cor
        // espectral pura normalizada tem energia muito acima do que o monitor
        // emite). Aqui e obrigatorio ESCALAR os tres juntos, nao ceifar canal a
        // canal: ceifar so o maior muda as razoes entre canais, ou seja, muda o
        // matiz. Em 650 nm isso transformava vermelho em magenta.
        //
        // Escalar reduz o brilho e preserva o matiz -- a troca certa, ja que o
        // brilho absoluto aqui ja e uma escolha de visualizacao, nao um dado.
        const double maximum = std::max({c.r, c.g, c.b});
        const bool scaleUp = strategy == GamutStrategy::DesaturateAndScale;
        if (maximum > 0.0 && (maximum > 1.0 || scaleUp)) {
            c.r /= maximum;
            c.g /= maximum;
            c.b /= maximum;
        }
    }

    // Rede de seguranca numerica. Para Clip e o proprio algoritmo; para as
    // demais estrategias, a esta altura nenhum componente deveria estar fora de
    // [0,1] -- se estiver, e erro de arredondamento da ordem de 1e-16.
    c.r = std::clamp(c.r, 0.0, 1.0);
    c.g = std::clamp(c.g, 0.0, 1.0);
    c.b = std::clamp(c.b, 0.0, 1.0);
    return c;
}

double encodeSrgb(double linear) {
    if (!std::isfinite(linear)) return 0.0;
    const double v = std::clamp(linear, 0.0, 1.0);
    return v <= 0.0031308 ? 12.92 * v : 1.055 * std::pow(v, 1.0 / 2.4) - 0.055;
}

double decodeSrgb(double encoded) {
    if (!std::isfinite(encoded)) return 0.0;
    const double v = std::clamp(encoded, 0.0, 1.0);
    return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
}

Rgb toRgb8(const LinearRgb& colour, GamutStrategy strategy) {
    const LinearRgb mapped = mapIntoGamut(colour, strategy);
    auto quantise = [](double linear) {
        return static_cast<std::uint8_t>(std::lround(std::clamp(encodeSrgb(linear), 0.0, 1.0) * 255.0));
    };
    return Rgb{quantise(mapped.r), quantise(mapped.g), quantise(mapped.b)};
}

Rgb xyzToRgb8(const Xyz& colour, GamutStrategy strategy) {
    return toRgb8(xyzToLinearRgb(colour), strategy);
}

LinearRgb blendLinear(const LinearRgb& a, const LinearRgb& b, double weightB) {
    const double t = std::clamp(weightB, 0.0, 1.0);
    return LinearRgb{a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

Xyz blendXyz(const Xyz& a, const Xyz& b, double weightB) {
    const double t = std::clamp(weightB, 0.0, 1.0);
    return Xyz{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}

}  // namespace soundwave
