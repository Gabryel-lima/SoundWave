#pragma once

#include <string>

#include "soundwave/mapping/frequency_mapper.hpp"
#include "soundwave/physics/medium.hpp"

namespace soundwave {

// ---------------------------------------------------------------------------
// MAPEADORES FISICAMENTE FUNDAMENTADOS
//
// Os mapeadores de mapping/mappers.hpp sao escolhas declaradas. Os dois aqui
// sao diferentes: eles sao as HIPOTESES NULAS do projeto -- o que acontece se
// voce se recusar a inventar a funcao.
//
// A resposta dos dois e a mesma, e e o resultado mais importante que este
// projeto produz: se voce nao inventar nada, nao ha cor nenhuma. O audivel nao
// cabe no visivel, e a unica forma de faze-lo caber e uma deformacao arbitraria.
//
// Eles existem para tornar esse argumento executavel, e nao apenas um paragrafo
// na documentacao.
// ---------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// IDENTIDADE: f_EM = f_som
//
// E o unico mapeamento com justificativa fisica PLENA. Um fonon de frequencia f
// tem energia E = h*f, exatamente como um foton de mesma frequencia. A
// identidade e, portanto, o mapeamento que preserva ENERGIA POR QUANTUM.
//
// Resultado: 20 Hz a 20 kHz em ondas eletromagneticas e a faixa ELF/VLF de
// radio. Comprimentos de onda de 15 km a 15.000 km. Nada visivel, e nem perto.
//
// Fidelidade: Exact. Nao ha nada escolhido aqui.
// -----------------------------------------------------------------------------
class IdentityMapper final : public FrequencyMapper {
public:
    explicit IdentityMapper(MappingDomain domain = {});

    [[nodiscard]] double map(double frequencyHz) const override;
    [[nodiscard]] std::optional<double> inverse(double emFrequencyHz) const override;
    [[nodiscard]] std::string name() const override { return "identity"; }
    [[nodiscard]] std::string describe() const override;
    [[nodiscard]] const MappingDomain& domain() const override { return domain_; }

    // Energia de um quantum desta frequencia, em joules e em eV.
    [[nodiscard]] static double quantumEnergyJoules(double frequencyHz);
    [[nodiscard]] static double quantumEnergyElectronVolts(double frequencyHz);

    [[nodiscard]] FidelityClaim fidelity() const;

private:
    MappingDomain domain_;
};

// -----------------------------------------------------------------------------
// ESCALA: lambda_luz = K * lambda_som
//
// A cadeia:
//   1. lambda_som = v_meio_acustico / f_som            <- FISICA (dado v)
//   2. lambda_alvo = K * lambda_som                    <- a UNICA escolha livre
//   3. lambda_vac  = n(lambda_vac) * lambda_alvo       <- FISICA (dispersao real)
//   4. f_EM        = c / lambda_vac                    <- FISICA exata
//
// A FORMA da funcao e fixada pela fisica: comprimento de onda e inversamente
// proporcional a frequencia, e ponto. Nao ha liberdade de curvatura. Toda a
// arbitrariedade se concentra em UM numero, K, que e uma razao de escala pura.
//
// Isso e estritamente melhor, do ponto de vista epistemico, que LogMapper ou
// LinearMapper: la a forma inteira da funcao e invencao; aqui so a escala e.
//
// E o preco dessa honestidade e alto e mensuravel. Como escala pura e uma
// isometria em espaco logaritmico, a janela visivel (0,907 oitava) captura
// exatamente 0,907 oitava do audivel -- 9,1% da banda. Ancorando 20 Hz em
// 750 nm, so 20-37,5 Hz cai no visivel; 440 Hz vai para o ultravioleta e
// 20 kHz para os raios X.
//
// Use com out_of_range: extrapolate e color.mode: visible-with-bands, que e
// como o projeto representa honestamente o que cai fora.
// -----------------------------------------------------------------------------
class ScaleMapper final : public FrequencyMapper {
public:
    // `scale` e o K adimensional. Prefira o construtor por ancora.
    ScaleMapper(Medium acousticMedium, Medium opticalMedium, Conditions conditions,
                double scale, MappingDomain domain = {});

    // Escolhe K de modo que `anchorHz` caia exatamente em `anchorNm` (no meio
    // optico). Torna a escolha legivel: "ancorei 20 Hz no vermelho profundo".
    [[nodiscard]] static ScaleMapper anchored(Medium acousticMedium, Medium opticalMedium,
                                              Conditions conditions, double anchorHz,
                                              double anchorNm, MappingDomain domain = {});

    [[nodiscard]] double map(double frequencyHz) const override;
    [[nodiscard]] std::optional<double> inverse(double emFrequencyHz) const override;
    [[nodiscard]] std::string name() const override { return "scale"; }
    [[nodiscard]] std::string describe() const override;
    [[nodiscard]] const MappingDomain& domain() const override { return domain_; }

    [[nodiscard]] double scale() const { return scale_; }
    [[nodiscard]] const Medium& acousticMedium() const { return acousticMedium_; }
    [[nodiscard]] const Medium& opticalMedium() const { return opticalMedium_; }

    // Faixa de f_som que este K de fato leva ao visivel, em Hz. Vazia quando
    // nenhuma parte do audivel cai la.
    struct VisibleWindow {
        double lowHz = 0.0;
        double highHz = 0.0;
        double octaves = 0.0;
        double fractionOfAudible = 0.0;
        [[nodiscard]] bool valid() const { return highHz > lowHz; }
    };
    [[nodiscard]] VisibleWindow visibleWindow() const;

    // Orcamento completo: as etapas fisicas mais a etapa arbitraria (K).
    [[nodiscard]] FidelityBudget fidelity() const;

private:
    Medium acousticMedium_;
    Medium opticalMedium_;
    Conditions conditions_;
    double scale_ = 1.0;
    MappingDomain domain_;
};

}  // namespace soundwave
