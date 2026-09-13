#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "soundwave/mapping/frequency_mapper.hpp"

namespace soundwave {

// Base com o que toda implementacao compartilha: dominio, politica de borda e
// a normalizacao da entrada para x em [0,1].
class MapperBase : public FrequencyMapper {
public:
    MapperBase(MappingDomain domain, OutOfRangePolicy policy);

    [[nodiscard]] const MappingDomain& domain() const override { return domain_; }
    [[nodiscard]] OutOfRangePolicy policy() const { return policy_; }

protected:
    // Aplica a politica de borda. Devolve NaN para Discard fora do dominio.
    [[nodiscard]] double applyPolicy(double frequencyHz) const;

    MappingDomain domain_;
    OutOfRangePolicy policy_;
};

// -----------------------------------------------------------------------------
// Linear: x = (f - fmin) / (fmax - fmin);  f_EM = tmin + x * (tmax - tmin)
//
// Deliberadamente incluido como CONTRA-EXEMPLO util. A audicao e logaritmica:
// em escala linear, metade do eixo audivel (10 kHz a 20 kHz) e uma unica oitava
// que quase ninguem distingue, enquanto as nove oitavas restantes se espremem
// na outra metade. O resultado visual concentra quase toda a variacao de cor na
// regiao onde ha pouca informacao musical.
// -----------------------------------------------------------------------------
class LinearMapper final : public MapperBase {
public:
    explicit LinearMapper(MappingDomain domain = {},
                          OutOfRangePolicy policy = OutOfRangePolicy::Clamp);

    [[nodiscard]] double map(double frequencyHz) const override;
    [[nodiscard]] std::optional<double> inverse(double emFrequencyHz) const override;
    [[nodiscard]] std::string name() const override { return "linear"; }
    [[nodiscard]] std::string describe() const override;
};

// -----------------------------------------------------------------------------
// Logaritmico (padrao do projeto):
//   x    = (ln f - ln fmin) / (ln fmax - ln fmin)
//   f_EM = tmin * (tmax / tmin)^x
//
// Preserva ORDEM e distancia relativa de altura: intervalos musicais iguais
// (razoes de frequencia iguais) ocupam distancias iguais no eixo x. E o que faz
// dele o padrao razoavel.
//
// NAO preserva equivalencia de oitava: por causa da compressao de ~11x, uma
// oitava vira ~6,5% em f_EM. Se a pergunta for "oitavas devem ter a mesma cor?",
// a resposta deste mapeador e nao. Use OctaveMapper.
// -----------------------------------------------------------------------------
class LogMapper final : public MapperBase {
public:
    explicit LogMapper(MappingDomain domain = {},
                       OutOfRangePolicy policy = OutOfRangePolicy::Clamp);

    [[nodiscard]] double map(double frequencyHz) const override;
    [[nodiscard]] std::optional<double> inverse(double emFrequencyHz) const override;
    [[nodiscard]] std::string name() const override { return "logarithmic"; }
    [[nodiscard]] std::string describe() const override;
};

// -----------------------------------------------------------------------------
// Oitava / classe de altura:
//   x    = frac(log2(f / fRef))
//   f_EM = tmin * (tmax / tmin)^x
//
// Mapeia a CLASSE DE ALTURA, nao a altura. A4 (440 Hz) e A5 (880 Hz) recebem
// exatamente a mesma cor -- e o unico mapeador aqui que respeita a equivalencia
// de oitava, o fato perceptual mais solido da percepcao musical.
//
// O preco e real e deve ser declarado: a funcao nao e monotonica nem injetiva.
// Infinitas frequencias compartilham cada cor, entao nao existe inversa e
// "que som gerou esta cor?" deixa de ter resposta unica. Ha tambem uma
// descontinuidade de cor a cada oitava (x salta de 1 para 0), que aparece como
// emenda visivel em varreduras continuas.
//
// Troca-se invertibilidade por fidelidade perceptual. Nenhuma das duas escolhas
// e "correta"; elas respondem a perguntas diferentes.
// -----------------------------------------------------------------------------
class OctaveMapper final : public MapperBase {
public:
    explicit OctaveMapper(MappingDomain domain = {},
                          double referenceHz = 440.0,
                          OutOfRangePolicy policy = OutOfRangePolicy::Clamp);

    [[nodiscard]] double map(double frequencyHz) const override;
    [[nodiscard]] std::string name() const override { return "octave"; }
    [[nodiscard]] std::string describe() const override;
    [[nodiscard]] bool isMonotonic() const override { return false; }
    [[nodiscard]] bool preservesOctaveEquivalence() const override { return true; }
    [[nodiscard]] double referenceHz() const { return referenceHz_; }

private:
    double referenceHz_;
};

// -----------------------------------------------------------------------------
// Customizado: interpolacao linear por partes sobre pontos de controle
// (f_som, f_EM), no espaco log-log.
//
// Existe para tornar barato testar uma hipotese nova de mapeamento sem escrever
// C++: os pontos vem do arquivo de configuracao. Monotonico se, e somente se, os
// pontos de controle forem monotonicos -- verificado na construcao.
// -----------------------------------------------------------------------------
class CustomMapper final : public MapperBase {
public:
    using ControlPoint = std::pair<double, double>;  // (f_som Hz, f_EM Hz)

    CustomMapper(std::vector<ControlPoint> points, MappingDomain domain = {},
                 OutOfRangePolicy policy = OutOfRangePolicy::Clamp);

    [[nodiscard]] double map(double frequencyHz) const override;
    [[nodiscard]] std::string name() const override { return "custom"; }
    [[nodiscard]] std::string describe() const override;
    [[nodiscard]] bool isMonotonic() const override { return monotonic_; }
    [[nodiscard]] const std::vector<ControlPoint>& controlPoints() const { return points_; }

private:
    std::vector<ControlPoint> points_;
    bool monotonic_ = true;
};

// Adaptador para uma lambda arbitraria. Util em testes e prototipagem; nao e
// serializavel para o manifesto, entao describe() exige um rotulo do chamador.
class FunctionMapper final : public MapperBase {
public:
    FunctionMapper(std::function<double(double)> fn, std::string label,
                   MappingDomain domain = {},
                   OutOfRangePolicy policy = OutOfRangePolicy::Clamp);

    [[nodiscard]] double map(double frequencyHz) const override;
    [[nodiscard]] std::string name() const override { return "function"; }
    [[nodiscard]] std::string describe() const override { return label_; }
    [[nodiscard]] bool isMonotonic() const override { return false; }  // desconhecido

private:
    std::function<double(double)> fn_;
    std::string label_;
};

}  // namespace soundwave
