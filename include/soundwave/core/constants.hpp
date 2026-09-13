#pragma once

// ---------------------------------------------------------------------------
// Constantes físicas e limites de domínio.
//
// LEIA docs/theory.md antes de alterar qualquer valor aqui. Este arquivo mistura
// deliberadamente duas categorias muito diferentes de número, e a distinção é o
// ponto central do projeto:
//
//   (a) Constantes FÍSICAS   -> kSpeedOfLight. Medida, não escolhida.
//   (b) Limites CONVENCIONAIS -> faixas audível e visível. Escolhidos por nós.
//
// Confundir (a) com (b) é o erro conceitual que este projeto existe para evitar.
// ---------------------------------------------------------------------------

namespace soundwave {

// (a) FÍSICA — velocidade da luz no vácuo, exata por definição do SI (m/s).
inline constexpr double kSpeedOfLight = 299'792'458.0;

// (b) CONVENÇÃO — faixa audível nominal humana (Hz). Aproximação grosseira:
// varia por indivíduo, idade e nível de pressão sonora.
inline constexpr double kAudibleMinHz = 20.0;
inline constexpr double kAudibleMaxHz = 20'000.0;

// (b) CONVENÇÃO — faixa visível nominal (Hz). Derivada de 400..750 nm.
// As bordas do visível não são bruscas: a sensibilidade do observador decai
// continuamente. Qualquer corte é arbitrário.
inline constexpr double kVisibleMinHz = 3.997'232'773'333'333e14;  // ~750 nm
inline constexpr double kVisibleMaxHz = 7.494'811'450'000'000e14;  // ~400 nm

// (b) CONVENÇÃO — mesmos limites em comprimento de onda (metros).
inline constexpr double kVisibleMinWavelengthM = 400e-9;
inline constexpr double kVisibleMaxWavelengthM = 750e-9;

// Conveniências de unidade.
inline constexpr double kNanometre = 1e-9;
inline constexpr double kTeraHertz = 1e12;

// Piso usado em conversões para decibel: dB = 20*log10(A + kAmplitudeEpsilon).
// Evita log(0); escolhido bem abaixo do menor passo de PCM 24 bits (~6e-8).
inline constexpr double kAmplitudeEpsilon = 1e-12;

}  // namespace soundwave
