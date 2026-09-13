#pragma once

#include <string>

namespace soundwave {

// Versao do PROJETO.
inline constexpr int kVersionMajor = 2;
inline constexpr int kVersionMinor = 0;
inline constexpr int kVersionPatch = 0;

// Versao do ALGORITMO -- proposital e separada da versao do projeto.
//
// Incremente sempre que uma mudanca alterar a saida numerica para a mesma
// entrada e a mesma configuracao: normalizacao de magnitude, coeficientes das
// CMF, formula de mapeamento, tratamento de gamut. Correcoes de interface,
// documentacao ou desempenho NAO devem incrementar.
//
// Sem isso, "mesmo arquivo + mesma configuracao -> mesmo resultado" e uma
// promessa impossivel de verificar entre versoes diferentes do binario.
inline constexpr int kAlgorithmVersion = 1;

[[nodiscard]] std::string versionString();

}  // namespace soundwave
