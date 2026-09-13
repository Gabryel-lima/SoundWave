#pragma once

#include <string_view>

#include "soundwave/color/srgb.hpp"

namespace soundwave {

// Regioes do espectro eletromagnetico (plano, secao 8).
//
// As fronteiras entre bandas sao CONVENCAO, nao descontinuidades fisicas: nada
// muda no comportamento da radiacao ao cruzar 750 nm. Os nomes refletem como a
// radiacao e produzida e detectada, nao uma mudanca de natureza.
enum class EmBand {
    Radio,
    Microwave,
    Infrared,
    Visible,
    Ultraviolet,
    XRay,
    Gamma,
};

[[nodiscard]] std::string_view emBandName(EmBand band);
[[nodiscard]] EmBand classifyByWavelength(double wavelengthM);
[[nodiscard]] EmBand classifyByFrequency(double frequencyHz);

// -----------------------------------------------------------------------------
// PSEUDOCOR.
//
// Fora do visivel nao existe "a cor real" -- nao existe cor nenhuma. Cor e uma
// resposta do sistema visual humano, e o sistema visual humano nao responde a
// infravermelho nem a raios X. Qualquer cor mostrada para essas bandas e um
// rotulo inventado, exatamente como as cores de um mapa topografico.
//
// Por isso a funcao devolve `isFalseColour`: a interface e obrigada a saber se
// esta mostrando uma aproximacao de uma resposta visual medida ou um simbolo
// arbitrario, e nunca deve apresentar os dois do mesmo jeito.
// -----------------------------------------------------------------------------
struct BandColour {
    Rgb colour;
    bool isFalseColour = true;
    EmBand band = EmBand::Visible;
};

// Cor representativa da banda inteira. Para EmBand::Visible devolve cinza medio
// com isFalseColour = false e um aviso implicito: a cor real do visivel depende
// do comprimento de onda especifico e vem de spectralXyz(), nao daqui.
[[nodiscard]] BandColour pseudoColourForBand(EmBand band);

// Pseudocor com gradiente dentro da banda: `position` em [0,1] vai da borda de
// maior comprimento de onda para a de menor. Permite ver estrutura dentro de
// uma banda nao visivel em vez de um bloco chapado.
[[nodiscard]] BandColour pseudoColourForBand(EmBand band, double position);

}  // namespace soundwave
