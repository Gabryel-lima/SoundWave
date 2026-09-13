#include "soundwave/mapping/physical_mappers.hpp"

#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "soundwave/core/constants.hpp"

namespace soundwave {
namespace {

const double kNaN = std::numeric_limits<double>::quiet_NaN();

// Constante de Planck. Exata por definicao do SI desde 2019 (define o kg).
constexpr double kPlanck = 6.62607015e-34;      // J*s
constexpr double kElectronVolt = 1.602176634e-19;  // J, tambem exata por definicao

std::string formatSpectral(double hz) {
    std::ostringstream out;
    const double nm = hz > 0.0 ? kSpeedOfLight / hz * 1e9 : 0.0;
    out << std::scientific << std::setprecision(3) << hz << " Hz";
    if (nm > 0.0) {
        out << " (lambda = ";
        if (nm >= 1e9) out << std::fixed << std::setprecision(1) << (nm / 1e9) << " m";
        else if (nm >= 1e6) out << std::fixed << std::setprecision(1) << (nm / 1e6) << " mm";
        else if (nm >= 1e3) out << std::fixed << std::setprecision(1) << (nm / 1e3) << " um";
        else out << std::fixed << std::setprecision(1) << nm << " nm";
        out << ")";
    }
    return out.str();
}

}  // namespace

// =============================================================================
// IdentityMapper
// =============================================================================

IdentityMapper::IdentityMapper(MappingDomain domain) : domain_(domain) {
    if (!domain_.valid()) throw std::invalid_argument("IdentityMapper: dominio invalido");
}

double IdentityMapper::map(double frequencyHz) const {
    // Sem prender nas bordas e sem politica de borda: a identidade e exata em
    // todo o dominio, e limita-la seria introduzir uma escolha onde nao ha.
    if (!std::isfinite(frequencyHz) || frequencyHz <= 0.0) return kNaN;
    return frequencyHz;
}

std::optional<double> IdentityMapper::inverse(double emFrequencyHz) const {
    if (!std::isfinite(emFrequencyHz) || emFrequencyHz <= 0.0) return std::nullopt;
    return emFrequencyHz;
}

double IdentityMapper::quantumEnergyJoules(double frequencyHz) {
    return kPlanck * frequencyHz;
}

double IdentityMapper::quantumEnergyElectronVolts(double frequencyHz) {
    return quantumEnergyJoules(frequencyHz) / kElectronVolt;
}

std::string IdentityMapper::describe() const {
    std::ostringstream out;
    out << "Identidade: f_EM = f_som. UNICO mapeamento com justificativa fisica plena: "
        << "um fonon e um foton de mesma frequencia carregam a mesma energia por quantum "
        << "(E = h*f), entao a identidade preserva energia exatamente. "
        << "Consequencia: a faixa audivel vira radio ELF/VLF -- "
        << formatSpectral(domain_.sourceMinHz) << " a " << formatSpectral(domain_.sourceMaxHz)
        << ". Nada visivel. E esse o ponto: sem uma escolha arbitraria, nao ha cor.";
    return out.str();
}

FidelityClaim IdentityMapper::fidelity() const {
    return {"mapeamento f_som -> f_EM (identidade)", Fidelity::Exact, 0.0,
            "E = h*f vale para fonon e foton; h e exata por definicao do SI",
            "todas as frequencias positivas",
            "preserva energia por quantum, nao percepcao: o resultado e "
            "invisivel por construcao"};
}

// =============================================================================
// ScaleMapper
// =============================================================================

ScaleMapper::ScaleMapper(Medium acousticMedium, Medium opticalMedium, Conditions conditions,
                         double scale, MappingDomain domain)
    : acousticMedium_(std::move(acousticMedium)),
      opticalMedium_(std::move(opticalMedium)),
      conditions_(conditions),
      scale_(scale),
      domain_(domain) {
    if (!domain_.valid()) throw std::invalid_argument("ScaleMapper: dominio invalido");
    if (!(scale_ > 0.0) || !std::isfinite(scale_)) {
        throw std::invalid_argument("ScaleMapper: escala deve ser positiva e finita");
    }
    if (!acousticMedium_.carriesSound()) {
        throw std::invalid_argument("ScaleMapper: o meio acustico escolhido nao propaga som (" +
                                    acousticMedium_.name + ")");
    }
}

ScaleMapper ScaleMapper::anchored(Medium acousticMedium, Medium opticalMedium,
                                  Conditions conditions, double anchorHz, double anchorNm,
                                  MappingDomain domain) {
    if (!(anchorHz > 0.0) || !(anchorNm > 0.0)) {
        throw std::invalid_argument("ScaleMapper::anchored: ancora deve ser positiva");
    }
    const double soundLambda = soundWavelengthM(acousticMedium, anchorHz, conditions);
    if (!(soundLambda > 0.0)) {
        throw std::invalid_argument("ScaleMapper::anchored: meio acustico sem velocidade de som");
    }
    // K tal que lambda_som(ancora) * K = lambda_alvo(ancora) no meio optico.
    const double targetInMedium = anchorNm * 1e-9 / refractiveIndexAt(opticalMedium, anchorNm);
    const double scale = targetInMedium / soundLambda;
    return ScaleMapper(std::move(acousticMedium), std::move(opticalMedium), conditions, scale,
                       domain);
}

double ScaleMapper::map(double frequencyHz) const {
    if (!std::isfinite(frequencyHz) || frequencyHz <= 0.0) return kNaN;

    // 1) lambda acustico no meio -- fisica, dado v.
    const double soundLambda = soundWavelengthM(acousticMedium_, frequencyHz, conditions_);
    if (!(soundLambda > 0.0)) return kNaN;

    // 2) a unica escolha: uma escala.
    const double targetInMediumM = soundLambda * scale_;

    // 3) de volta ao comprimento de onda de vacuo, com a dispersao real do meio.
    const auto vacuumNm = vacuumWavelengthFromMediumNm(opticalMedium_, targetInMediumM * 1e9);
    if (!vacuumNm) return kNaN;

    // 4) f = c / lambda_vac -- exata.
    return kSpeedOfLight / (*vacuumNm * 1e-9);
}

std::optional<double> ScaleMapper::inverse(double emFrequencyHz) const {
    if (!std::isfinite(emFrequencyHz) || emFrequencyHz <= 0.0) return std::nullopt;

    const double vacuumNm = kSpeedOfLight / emFrequencyHz * 1e9;
    const double inMediumM = vacuumNm * 1e-9 / refractiveIndexAt(opticalMedium_, vacuumNm);
    const double soundLambda = inMediumM / scale_;
    if (!(soundLambda > 0.0)) return std::nullopt;

    const double v = soundSpeedMs(acousticMedium_, conditions_);
    if (!(v > 0.0)) return std::nullopt;
    return v / soundLambda;
}

ScaleMapper::VisibleWindow ScaleMapper::visibleWindow() const {
    VisibleWindow window;

    // A inversa leva as bordas do visivel de volta ao dominio sonoro.
    const auto atRed = inverse(kVisibleMinHz);   // 750 nm
    const auto atViolet = inverse(kVisibleMaxHz); // 400 nm
    if (!atRed || !atViolet) return window;

    window.lowHz = std::min(*atRed, *atViolet);
    window.highHz = std::max(*atRed, *atViolet);
    if (!window.valid()) return window;

    window.octaves = std::log2(window.highHz / window.lowHz);
    // Fracao da banda audivel coberta, medida em oitavas -- que e a medida certa
    // para percepcao de altura, e nao a fracao linear de frequencia.
    window.fractionOfAudible = domain_.sourceOctaves() > 0.0
                                   ? window.octaves / domain_.sourceOctaves()
                                   : 0.0;
    return window;
}

std::string ScaleMapper::describe() const {
    std::ostringstream out;
    out << "Escala: lambda_som = v(" << acousticMedium_.name << ")/f, depois lambda_luz = K * "
        << "lambda_som com K = " << std::scientific << std::setprecision(4) << scale_
        << " (adimensional), depois f_EM = c/lambda_vac usando a dispersao real de "
        << opticalMedium_.name << ". "
        << "A FORMA da funcao e fisica (lambda ~ 1/f); so a escala K e escolhida -- "
        << "toda a arbitrariedade cabe em um numero. "
        << "Como escala pura e isometria em espaco logaritmico, o visivel captura "
        << "exatamente sua propria largura: ";

    const VisibleWindow window = visibleWindow();
    if (window.valid()) {
        out << std::fixed << std::setprecision(1) << window.lowHz << " a " << window.highHz
            << " Hz (" << std::setprecision(2) << window.octaves << " de "
            << domain_.sourceOctaves() << " oitavas, " << std::setprecision(1)
            << (window.fractionOfAudible * 100.0) << "% do audivel). ";
    } else {
        out << "nenhuma parte do audivel. ";
    }
    out << "O resto cai fora do visivel e deve ser mostrado como pseudocor.";
    return out.str();
}

FidelityBudget ScaleMapper::fidelity() const {
    FidelityBudget budget;
    budget.add(soundSpeedClaim(acousticMedium_));
    budget.add(refractiveIndexClaim(opticalMedium_));
    budget.add(claims::speedOfLight());
    budget.add(claims::wavelengthFromFrequency());

    // A etapa que contamina tudo. Uma unica constante, mas sem valor verdadeiro.
    FidelityClaim choice;
    choice.quantity = "constante de escala K";
    choice.tier = Fidelity::Arbitrary;
    std::ostringstream source;
    source << "escolhida pelo usuario: K = " << std::scientific << std::setprecision(4) << scale_;
    choice.source = source.str();
    choice.caveat = "nao existe razao fisica para nenhum valor de K; e uma razao de "
                    "escala pura entre dois dominios sem relacao. Esta e a unica "
                    "etapa arbitraria da cadeia -- e basta ela para que o resultado "
                    "final nao seja uma medida";
    budget.add(choice);
    return budget;
}

}  // namespace soundwave
