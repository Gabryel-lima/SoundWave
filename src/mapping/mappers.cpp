#include "soundwave/mapping/mappers.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace soundwave {
namespace {

const double kNaN = std::numeric_limits<double>::quiet_NaN();

std::string formatHz(double hz) {
    std::ostringstream out;
    if (hz >= 1e12) {
        out.precision(4);
        out << std::fixed << (hz / 1e12) << " THz";
    } else {
        out.precision(1);
        out << std::fixed << hz << " Hz";
    }
    return out.str();
}

}  // namespace

// ---------------------------------------------------------------------------

MapperBase::MapperBase(MappingDomain domain, OutOfRangePolicy policy)
    : domain_(domain), policy_(policy) {
    if (!domain_.valid()) {
        throw std::invalid_argument(
            "MappingDomain invalido: exige 0 < sourceMin < sourceMax e 0 < targetMin < targetMax");
    }
}

double MapperBase::applyPolicy(double frequencyHz) const {
    if (!std::isfinite(frequencyHz) || frequencyHz <= 0.0) return kNaN;
    if (frequencyHz >= domain_.sourceMinHz && frequencyHz <= domain_.sourceMaxHz) {
        return frequencyHz;
    }
    switch (policy_) {
        case OutOfRangePolicy::Clamp:
            return std::clamp(frequencyHz, domain_.sourceMinHz, domain_.sourceMaxHz);
        case OutOfRangePolicy::Extrapolate:
            return frequencyHz;
        case OutOfRangePolicy::Discard:
            return kNaN;
    }
    return kNaN;
}

// ---------------------------------------------------------------------------

LinearMapper::LinearMapper(MappingDomain domain, OutOfRangePolicy policy)
    : MapperBase(domain, policy) {}

double LinearMapper::map(double frequencyHz) const {
    const double f = applyPolicy(frequencyHz);
    if (std::isnan(f)) return kNaN;
    const double x = (f - domain_.sourceMinHz) / (domain_.sourceMaxHz - domain_.sourceMinHz);
    return domain_.targetMinHz + x * (domain_.targetMaxHz - domain_.targetMinHz);
}

std::optional<double> LinearMapper::inverse(double emFrequencyHz) const {
    if (!std::isfinite(emFrequencyHz)) return std::nullopt;
    const double span = domain_.targetMaxHz - domain_.targetMinHz;
    const double x = (emFrequencyHz - domain_.targetMinHz) / span;
    if (policy_ != OutOfRangePolicy::Extrapolate && (x < 0.0 || x > 1.0)) return std::nullopt;
    return domain_.sourceMinHz + x * (domain_.sourceMaxHz - domain_.sourceMinHz);
}

std::string LinearMapper::describe() const {
    std::ostringstream out;
    out << "Linear em frequencia: x = (f - " << formatHz(domain_.sourceMinHz) << ") / ("
        << formatHz(domain_.sourceMaxHz) << " - " << formatHz(domain_.sourceMinHz)
        << "); f_EM = " << formatHz(domain_.targetMinHz) << " + x * ("
        << formatHz(domain_.targetMaxHz) << " - " << formatHz(domain_.targetMinHz) << "). "
        << "Contra-exemplo pedagogico: ignora que a percepcao de altura e logaritmica, "
        << "entao metade do eixo de cor cobre a ultima oitava audivel. "
        << "Borda: " << outOfRangePolicyName(policy_) << ".";
    return out.str();
}

// ---------------------------------------------------------------------------

LogMapper::LogMapper(MappingDomain domain, OutOfRangePolicy policy) : MapperBase(domain, policy) {}

double LogMapper::map(double frequencyHz) const {
    const double f = applyPolicy(frequencyHz);
    if (std::isnan(f)) return kNaN;
    const double x = (std::log(f) - std::log(domain_.sourceMinHz)) /
                     (std::log(domain_.sourceMaxHz) - std::log(domain_.sourceMinHz));
    return domain_.targetMinHz * std::pow(domain_.targetMaxHz / domain_.targetMinHz, x);
}

std::optional<double> LogMapper::inverse(double emFrequencyHz) const {
    if (!std::isfinite(emFrequencyHz) || emFrequencyHz <= 0.0) return std::nullopt;
    const double x = std::log(emFrequencyHz / domain_.targetMinHz) /
                     std::log(domain_.targetMaxHz / domain_.targetMinHz);
    if (policy_ != OutOfRangePolicy::Extrapolate && (x < -1e-12 || x > 1.0 + 1e-12)) {
        return std::nullopt;
    }
    return domain_.sourceMinHz * std::pow(domain_.sourceMaxHz / domain_.sourceMinHz, x);
}

std::string LogMapper::describe() const {
    std::ostringstream out;
    out.precision(3);
    out << std::fixed
        << "Logaritmico: x = (ln f - ln " << domain_.sourceMinHz << ") / (ln "
        << domain_.sourceMaxHz << " - ln " << domain_.sourceMinHz << "); f_EM = "
        << formatHz(domain_.targetMinHz) << " * (" << formatHz(domain_.targetMaxHz) << " / "
        << formatHz(domain_.targetMinHz) << ")^x. "
        << "Preserva a ordem e a distancia relativa de altura. Comprime "
        << domain_.sourceOctaves() << " oitavas sonoras em " << domain_.targetOctaves()
        << " oitava(s) de luz (fator " << domain_.compressionRatio()
        << "x), portanto NAO preserva equivalencia de oitava: uma oitava musical desloca f_EM "
        << "em apenas " << ((std::pow(2.0, 1.0 / domain_.compressionRatio()) - 1.0) * 100.0)
        << "%. Borda: " << outOfRangePolicyName(policy_) << ".";
    return out.str();
}

// ---------------------------------------------------------------------------

OctaveMapper::OctaveMapper(MappingDomain domain, double referenceHz, OutOfRangePolicy policy)
    : MapperBase(domain, policy), referenceHz_(referenceHz) {
    if (!(referenceHz_ > 0.0)) {
        throw std::invalid_argument("OctaveMapper: referenceHz deve ser positivo");
    }
}

double OctaveMapper::map(double frequencyHz) const {
    const double f = applyPolicy(frequencyHz);
    if (std::isnan(f)) return kNaN;

    const double octaves = std::log2(f / referenceHz_);
    // Parte fracionaria sempre em [0,1), inclusive para oitavas negativas:
    // std::fmod(-0.25, 1.0) devolve -0.25, nao 0.75.
    double x = octaves - std::floor(octaves);
    if (x < 0.0) x += 1.0;
    if (x >= 1.0) x = 0.0;

    return domain_.targetMinHz * std::pow(domain_.targetMaxHz / domain_.targetMinHz, x);
}

std::string OctaveMapper::describe() const {
    std::ostringstream out;
    out.precision(1);
    out << std::fixed
        << "Classe de altura: x = frac(log2(f / " << referenceHz_ << " Hz)); f_EM = "
        << formatHz(domain_.targetMinHz) << " * (" << formatHz(domain_.targetMaxHz) << " / "
        << formatHz(domain_.targetMinHz) << ")^x. "
        << "Unico mapeador que preserva equivalencia de oitava (f e 2f recebem a mesma cor). "
        << "Nao e monotonico nem injetivo: nao ha inversa, e ha descontinuidade de cor a cada "
        << "oitava. Borda: " << outOfRangePolicyName(policy_) << ".";
    return out.str();
}

// ---------------------------------------------------------------------------

CustomMapper::CustomMapper(std::vector<ControlPoint> points, MappingDomain domain,
                           OutOfRangePolicy policy)
    : MapperBase(domain, policy), points_(std::move(points)) {
    if (points_.size() < 2) {
        throw std::invalid_argument("CustomMapper: sao necessarios ao menos 2 pontos de controle");
    }
    for (const auto& [source, target] : points_) {
        if (!(source > 0.0) || !(target > 0.0)) {
            throw std::invalid_argument("CustomMapper: pontos de controle devem ser positivos");
        }
    }

    std::sort(points_.begin(), points_.end(),
              [](const ControlPoint& a, const ControlPoint& b) { return a.first < b.first; });

    for (std::size_t i = 1; i < points_.size(); ++i) {
        if (points_[i].first == points_[i - 1].first) {
            throw std::invalid_argument("CustomMapper: pontos de controle com f_som duplicada");
        }
        if (points_[i].second <= points_[i - 1].second) monotonic_ = false;
    }
}

double CustomMapper::map(double frequencyHz) const {
    const double f = applyPolicy(frequencyHz);
    if (std::isnan(f)) return kNaN;

    if (f <= points_.front().first) return points_.front().second;
    if (f >= points_.back().first) return points_.back().second;

    // Interpolacao no espaco log-log: em escala linear, um segmento entre dois
    // pontos distantes varias oitavas produziria uma curva visualmente errada,
    // concentrando toda a variacao no extremo agudo.
    const auto it = std::upper_bound(
        points_.begin(), points_.end(), f,
        [](double value, const ControlPoint& point) { return value < point.first; });
    const auto& hi = *it;
    const auto& lo = *(it - 1);

    const double t = (std::log(f) - std::log(lo.first)) / (std::log(hi.first) - std::log(lo.first));
    return std::exp(std::log(lo.second) + t * (std::log(hi.second) - std::log(lo.second)));
}

std::string CustomMapper::describe() const {
    std::ostringstream out;
    out.precision(1);
    out << std::fixed << "Customizado: interpolacao linear por partes em log-log sobre "
        << points_.size() << " pontos de controle [";
    for (std::size_t i = 0; i < points_.size(); ++i) {
        if (i != 0) out << ", ";
        out << "(" << points_[i].first << " Hz -> " << formatHz(points_[i].second) << ")";
    }
    out << "]. " << (monotonic_ ? "Monotonico." : "NAO monotonico (pontos de controle invertidos).")
        << " Borda: " << outOfRangePolicyName(policy_) << ".";
    return out.str();
}

// ---------------------------------------------------------------------------

FunctionMapper::FunctionMapper(std::function<double(double)> fn, std::string label,
                               MappingDomain domain, OutOfRangePolicy policy)
    : MapperBase(domain, policy), fn_(std::move(fn)), label_(std::move(label)) {
    if (!fn_) throw std::invalid_argument("FunctionMapper: funcao nula");
}

double FunctionMapper::map(double frequencyHz) const {
    const double f = applyPolicy(frequencyHz);
    if (std::isnan(f)) return kNaN;
    return fn_(f);
}

}  // namespace soundwave
