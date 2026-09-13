#include "soundwave/physics/survival.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "soundwave/color/em_band.hpp"
#include "soundwave/color/spectral.hpp"
#include "soundwave/core/constants.hpp"

namespace soundwave {
namespace {

const double kNaN = std::numeric_limits<double>::quiet_NaN();

// Melhor transmitancia optica disponivel no visivel, e onde ela ocorre.
struct OpticalPeak {
    double transmittance = 0.0;
    double nm = 0.0;
};

OpticalPeak bestVisibleTransmittance(const MediumFilterSettings& settings) {
    OpticalPeak peak;
    for (double nm = kVisibleMinWavelengthM * 1e9; nm <= kVisibleMaxWavelengthM * 1e9; nm += 1.0) {
        const double t = opticalTransmittance(settings.optical, nm, settings.pathLengthM);
        if (t > peak.transmittance) {
            peak.transmittance = t;
            peak.nm = nm;
        }
    }
    return peak;
}

}  // namespace

SurvivalScore jointSurvival(const FrequencyMapper& mapper, const MediumFilterSettings& settings,
                            const MappingDomain& domain, std::size_t samples) {
    SurvivalScore score;
    if (samples < 2 || !domain.valid()) return score;

    const OpticalPeak peak = bestVisibleTransmittance(settings);
    score.opticalBest = peak.transmittance;
    score.opticalBestNm = peak.nm;

    double jointSum = 0.0;
    double acousticSum = 0.0;
    double ceilingSum = 0.0;
    std::size_t visibleCount = 0;

    for (std::size_t i = 0; i < samples; ++i) {
        // Amostragem log-espacada: a percepcao de altura e logaritmica, e em
        // amostragem linear a ultima oitava levaria metade do peso.
        const double t = static_cast<double>(i) / static_cast<double>(samples - 1);
        const double f = domain.sourceMinHz * std::pow(domain.sourceMaxHz / domain.sourceMinHz, t);

        const double ts =
            acousticTransmittance(settings.acoustic, f, settings.pathLengthM, settings.conditions);
        acousticSum += ts;
        ceilingSum += ts * peak.transmittance;

        const double em = mapper.map(f);
        if (!std::isfinite(em) || em <= 0.0) continue;  // descartado: contribui zero

        const double nm = kSpeedOfLight / em * 1e9;
        if (classifyByWavelength(nm * 1e-9) != EmBand::Visible) continue;

        ++visibleCount;
        jointSum += ts * opticalTransmittance(settings.optical, nm, settings.pathLengthM);
    }

    const double n = static_cast<double>(samples);
    score.joint = jointSum / n;
    score.acousticMean = acousticSum / n;
    score.ceiling = ceilingSum / n;
    score.efficiency = score.ceiling > 0.0 ? score.joint / score.ceiling : 0.0;
    score.visibleFraction = static_cast<double>(visibleCount) / n;
    return score;
}

// ---------------------------------------------------------------------------

std::string_view AlignedMapper::candidateName(Candidate candidate) {
    switch (candidate) {
        case Candidate::LogAscending:     return "log-ascendente";
        case Candidate::LogDescending:    return "log-descendente";
        case Candidate::OctaveAscending:  return "oitava-ascendente";
        case Candidate::OctaveDescending: return "oitava-descendente";
    }
    return "desconhecido";
}

AlignedMapper::AlignedMapper(MediumFilterSettings settings, MappingDomain domain,
                             double referenceHz)
    : settings_(std::move(settings)), domain_(domain), referenceHz_(referenceHz) {
    if (!domain_.valid()) throw std::invalid_argument("AlignedMapper: dominio invalido");
    if (!(referenceHz_ > 0.0)) {
        throw std::invalid_argument("AlignedMapper: referenceHz deve ser positivo");
    }

    // Adaptador para pontuar um candidato sem instanciar outro mapeador.
    class CandidateView final : public FrequencyMapper {
    public:
        CandidateView(const AlignedMapper& owner, Candidate candidate)
            : owner_(&owner), candidate_(candidate) {}
        [[nodiscard]] double map(double f) const override {
            return owner_->evaluate(candidate_, f);
        }
        [[nodiscard]] std::string name() const override { return "candidate"; }
        [[nodiscard]] std::string describe() const override { return "candidato interno"; }
        [[nodiscard]] const MappingDomain& domain() const override { return owner_->domain(); }

    private:
        const AlignedMapper* owner_;
        Candidate candidate_;
    };

    const Candidate candidates[] = {Candidate::LogAscending, Candidate::LogDescending,
                                    Candidate::OctaveAscending, Candidate::OctaveDescending};

    double best = -1.0;
    for (std::size_t i = 0; i < 4; ++i) {
        const CandidateView view(*this, candidates[i]);
        scores_[i] = jointSurvival(view, settings_, domain_);
        // Desempate deterministico pela ordem do enum: sem isso, dois candidatos
        // empatados dariam resultados diferentes entre execucoes.
        if (scores_[i].joint > best) {
            best = scores_[i].joint;
            chosen_ = candidates[i];
            score_ = scores_[i];
        }
    }
}

double AlignedMapper::evaluate(Candidate candidate, double frequencyHz) const {
    if (!std::isfinite(frequencyHz) || frequencyHz <= 0.0) return kNaN;
    const double f = std::clamp(frequencyHz, domain_.sourceMinHz, domain_.sourceMaxHz);

    double x = 0.0;
    switch (candidate) {
        case Candidate::LogAscending:
        case Candidate::LogDescending:
            x = (std::log(f) - std::log(domain_.sourceMinHz)) /
                (std::log(domain_.sourceMaxHz) - std::log(domain_.sourceMinHz));
            break;
        case Candidate::OctaveAscending:
        case Candidate::OctaveDescending: {
            const double octaves = std::log2(f / referenceHz_);
            x = octaves - std::floor(octaves);
            if (x < 0.0) x += 1.0;
            if (x >= 1.0) x = 0.0;
            break;
        }
    }

    // Inverter a orientacao e uma reflexao em x. E o unico grau de liberdade
    // real desta familia -- e, no fim, o que decide o alinhamento.
    if (candidate == Candidate::LogDescending || candidate == Candidate::OctaveDescending) {
        x = 1.0 - x;
    }
    return domain_.targetMinHz * std::pow(domain_.targetMaxHz / domain_.targetMinHz, x);
}

double AlignedMapper::map(double frequencyHz) const { return evaluate(chosen_, frequencyHz); }

std::optional<double> AlignedMapper::inverse(double emFrequencyHz) const {
    if (!isMonotonic()) return std::nullopt;  // candidatos de oitava nao sao injetivos
    if (!std::isfinite(emFrequencyHz) || emFrequencyHz <= 0.0) return std::nullopt;

    double x = std::log(emFrequencyHz / domain_.targetMinHz) /
               std::log(domain_.targetMaxHz / domain_.targetMinHz);
    if (x < -1e-12 || x > 1.0 + 1e-12) return std::nullopt;
    if (chosen_ == Candidate::LogDescending) x = 1.0 - x;
    return domain_.sourceMinHz * std::pow(domain_.sourceMaxHz / domain_.sourceMinHz, x);
}

bool AlignedMapper::isMonotonic() const {
    return chosen_ == Candidate::LogAscending || chosen_ == Candidate::LogDescending;
}

bool AlignedMapper::preservesOctaveEquivalence() const {
    return chosen_ == Candidate::OctaveAscending || chosen_ == Candidate::OctaveDescending;
}

std::string AlignedMapper::describe() const {
    std::ostringstream out;
    out << std::fixed << std::setprecision(4)
        << "Alinhado por sobrevivencia conjunta: escolheu " << candidateName(chosen_)
        << " entre 4 candidatos declarados, maximizando J = media de T_som * T_luz em "
        << settings_.acoustic.name << " com " << std::setprecision(1) << settings_.pathLengthM
        << " m de caminho. J = " << std::setprecision(4) << score_.joint << " (eficiencia "
        << std::setprecision(1) << (score_.efficiency * 100.0) << "% do teto, "
        << (score_.visibleFraction * 100.0) << "% do audivel no visivel). ";

    // O contraste com o padrao classico e o resultado, entao vai na descricao.
    const double ascending = scores_[0].joint;
    if (ascending > 0.0 && chosen_ != Candidate::LogAscending) {
        out << std::setprecision(2) << "Supera o log-ascendente classico em "
            << (score_.joint / ascending) << "x. ";
    }
    out << "NAO e um mapeamento fisico: o criterio, o meio e o caminho sao escolhas. "
        << "O ganho e que a escolha DENTRO da familia foi decidida por medida, nao por gosto.";
    return out.str();
}

FidelityBudget AlignedMapper::fidelity() const {
    FidelityBudget budget;
    budget.add(soundAbsorptionClaim(settings_.acoustic));
    budget.add(opticalAbsorptionClaim(settings_.optical, score_.opticalBestNm));

    FidelityClaim choice;
    choice.quantity = "mapeamento f_som -> f_EM (alinhado por sobrevivencia)";
    choice.tier = Fidelity::Arbitrary;
    std::ostringstream source;
    source << "escolhido por medida entre 4 candidatos: " << candidateName(chosen_);
    choice.source = source.str();
    choice.validity = "otimo para " + settings_.acoustic.name + " no caminho configurado";
    choice.caveat =
        "continua arbitrario, mas por uma razao diferente dos demais: a familia de "
        "candidatos, o criterio (sobrevivencia conjunta), o meio e o comprimento do "
        "caminho sao todos escolhas. O que NAO e escolha e qual candidato vence -- isso "
        "e medido. Trocar o meio ou o caminho pode trocar o vencedor";
    budget.add(choice);
    return budget;
}

// ---------------------------------------------------------------------------
// Separacao de escalas
// ---------------------------------------------------------------------------

double TransitionWindow::centreM() const {
    return found ? std::sqrt(lowM * highM) : 0.0;
}

double TransitionWindow::decades() const {
    return found && lowM > 0.0 ? std::log10(highM / lowM) : 0.0;
}

namespace {

// Varredura logaritmica de distancias, procurando onde o contraste entre o
// extremo preservado e o destruido fica na faixa [minContrast, maxContrast].
template <typename ContrastFn>
TransitionWindow findWindow(ContrastFn contrast, double minContrast, double maxContrast) {
    TransitionWindow window;
    for (double L = 1e-4; L < 1e10; L *= 1.05) {
        const double ratio = contrast(L);
        if (!std::isfinite(ratio)) continue;
        if (ratio >= minContrast && ratio <= maxContrast) {
            if (!window.found) {
                window.lowM = L;
                window.found = true;
            }
            window.highM = L;
        }
    }
    return window;
}

}  // namespace

ScaleSeparation scaleSeparation(const MediumFilterSettings& settings, const MappingDomain& domain,
                                double minContrast, double maxContrast) {
    ScaleSeparation separation;
    if (!domain.valid()) return separation;

    const OpticalPeak peak = bestVisibleTransmittance({settings.acoustic, settings.optical,
                                                       settings.conditions, 1.0});
    const double bestNm = peak.nm > 0.0 ? peak.nm : 485.0;
    const double worstNm = kVisibleMaxWavelengthM * 1e9;  // 750 nm: o mais absorvido

    separation.acoustic = findWindow(
        [&](double L) {
            const double good = acousticTransmittance(settings.acoustic, domain.sourceMinHz, L,
                                                      settings.conditions);
            const double bad = acousticTransmittance(settings.acoustic, domain.sourceMaxHz, L,
                                                     settings.conditions);
            return bad > 1e-300 ? good / bad : std::numeric_limits<double>::infinity();
        },
        minContrast, maxContrast);

    separation.optical = findWindow(
        [&](double L) {
            const double good = opticalTransmittance(settings.optical, bestNm, L);
            const double bad = opticalTransmittance(settings.optical, worstNm, L);
            return bad > 1e-300 ? good / bad : std::numeric_limits<double>::infinity();
        },
        minContrast, maxContrast);

    if (separation.acoustic.found && separation.optical.found) {
        separation.overlaps = separation.acoustic.lowM <= separation.optical.highM &&
                              separation.optical.lowM <= separation.acoustic.highM;
        const double a = separation.acoustic.centreM();
        const double o = separation.optical.centreM();
        if (a > 0.0 && o > 0.0) {
            separation.separationFactor = std::max(a, o) / std::min(a, o);
            separation.separationDecades = std::log10(separation.separationFactor);
        }
    }
    return separation;
}

}  // namespace soundwave
