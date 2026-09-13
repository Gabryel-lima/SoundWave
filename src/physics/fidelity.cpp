#include "soundwave/physics/fidelity.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace soundwave {
namespace {
const double kNaN = std::numeric_limits<double>::quiet_NaN();
}

std::string_view fidelityName(Fidelity tier) {
    switch (tier) {
        case Fidelity::Exact:      return "exato";
        case Fidelity::Measured:   return "medido";
        case Fidelity::ModelFit:   return "ajuste";
        case Fidelity::Convention: return "convencao";
        case Fidelity::Arbitrary:  return "arbitrario";
    }
    return "desconhecido";
}

std::string_view fidelityMeaning(Fidelity tier) {
    switch (tier) {
        case Fidelity::Exact:
            return "exato por definicao ou identidade; erro so de ponto flutuante";
        case Fidelity::Measured:
            return "medida empirica com incerteza declarada";
        case Fidelity::ModelFit:
            return "ajuste empirico valido em faixa declarada";
        case Fidelity::Convention:
            return "convencao padronizada; nao ha valor verdadeiro, mas ha acordo";
        case Fidelity::Arbitrary:
            return "escolha livre sem base fisica; NAO admite barra de erro";
    }
    return "desconhecido";
}

int fidelityRank(Fidelity tier) {
    switch (tier) {
        case Fidelity::Exact:      return 0;
        case Fidelity::Measured:   return 1;
        case Fidelity::ModelFit:   return 2;
        case Fidelity::Convention: return 3;
        case Fidelity::Arbitrary:  return 4;
    }
    return 4;
}

bool FidelityClaim::hasUncertainty() const {
    return tier != Fidelity::Arbitrary && std::isfinite(relativeUncertainty);
}

std::string FidelityClaim::summary() const {
    std::ostringstream out;
    out << "[" << fidelityName(tier) << "] " << quantity;
    if (hasUncertainty()) {
        out << "  +/- ";
        // Incertezas aqui cobrem de 1e-16 (exato) a 1e0 (ordem de grandeza).
        // Porcentagem so e legivel na faixa do meio.
        if (relativeUncertainty >= 1e-4) {
            out << std::fixed << std::setprecision(3) << (relativeUncertainty * 100.0) << "%";
        } else {
            out << std::scientific << std::setprecision(1) << relativeUncertainty << " (rel.)";
        }
    } else if (tier == Fidelity::Arbitrary) {
        out << "  (sem incerteza definivel)";
    }
    if (!source.empty()) out << "\n      fonte  : " << source;
    if (!validity.empty()) out << "\n      valido : " << validity;
    if (!caveat.empty()) out << "\n      ressalva: " << caveat;
    return out.str();
}

void FidelityBudget::add(FidelityClaim claim) { claims_.push_back(std::move(claim)); }

void FidelityBudget::merge(const FidelityBudget& other) {
    claims_.insert(claims_.end(), other.claims_.begin(), other.claims_.end());
}

Fidelity FidelityBudget::weakestTier() const {
    Fidelity weakest = Fidelity::Exact;
    for (const FidelityClaim& claim : claims_) {
        if (fidelityRank(claim.tier) > fidelityRank(weakest)) weakest = claim.tier;
    }
    return weakest;
}

bool FidelityBudget::hasArbitraryStep() const {
    return std::any_of(claims_.begin(), claims_.end(),
                       [](const FidelityClaim& c) { return c.tier == Fidelity::Arbitrary; });
}

double FidelityBudget::combinedRelativeUncertainty() const {
    // Recusa deliberada. Uma cadeia que contem uma escolha livre nao produz uma
    // medida, e anexar "+/- x%" a ela daria aparencia de precisao ao que e, na
    // origem, uma decisao. Ver o comentario no cabecalho.
    if (hasArbitraryStep()) return kNaN;

    double sumSquares = 0.0;
    bool any = false;
    for (const FidelityClaim& claim : claims_) {
        if (!claim.hasUncertainty()) continue;
        sumSquares += claim.relativeUncertainty * claim.relativeUncertainty;
        any = true;
    }
    return any ? std::sqrt(sumSquares) : 0.0;
}

std::string FidelityBudget::report() const {
    std::ostringstream out;
    out << "ORCAMENTO DE FIDELIDADE\n" << std::string(72, '=') << "\n";

    if (claims_.empty()) {
        out << "  (nenhuma etapa registrada)\n";
        return out.str();
    }

    for (const FidelityClaim& claim : claims_) out << "  " << claim.summary() << "\n";

    const Fidelity weakest = weakestTier();
    out << std::string(72, '-') << "\n"
        << "  elo mais fraco : " << fidelityName(weakest) << " -- " << fidelityMeaning(weakest)
        << "\n";

    const double combined = combinedRelativeUncertainty();
    if (std::isnan(combined)) {
        out << "  incerteza      : INDEFINIDA\n"
            << "\n"
            << "  A cadeia contem ao menos uma etapa arbitraria, entao o resultado\n"
            << "  NAO e uma medida e nao admite barra de erro. As etapas fisicas\n"
            << "  acima continuam validas isoladamente -- o que nao se pode fazer e\n"
            << "  combina-las em um numero unico e chamar isso de precisao.\n";
    } else {
        out << "  incerteza      : +/- " << std::fixed << std::setprecision(4)
            << (combined * 100.0) << "%  (soma quadratica, fontes independentes)\n";
    }
    return out.str();
}

// ---------------------------------------------------------------------------

namespace claims {

FidelityClaim speedOfLight() {
    return {"velocidade da luz no vacuo (c)", Fidelity::Exact, 0.0,
            "SI: exata por definicao do metro desde 1983 (c = 299 792 458 m/s)", "universal", ""};
}

FidelityClaim wavelengthFromFrequency() {
    return {"lambda = c / f", Fidelity::Exact, 0.0,
            "identidade para onda plana monocromatica", "vacuo; em meio use lambda = c/(n f)",
            ""};
}

FidelityClaim cie1931Observer() {
    return {"funcoes de correspondencia CIE 1931 (2 graus)", Fidelity::Convention, 0.01,
            "CIE 1931, tabela oficial 380-780 nm, passo 5 nm",
            "380-780 nm, observador padrao",
            "e a media de um painel pequeno de 1931; a variacao entre observadores "
            "reais chega a varios por cento e excede esta incerteza"};
}

FidelityClaim srgbEncoding() {
    return {"primarias sRGB e ponto branco D65", Fidelity::Convention, 0.0,
            "IEC 61966-2-1:1999", "sRGB nominal",
            "define um monitor ideal; a tela real do leitor difere"};
}

FidelityClaim visibleRangeConvention() {
    return {"limites do visivel 400-750 nm", Fidelity::Convention, 0.0,
            "convencao do projeto", "",
            "a sensibilidade do olho decai de forma continua; nao existe corte objetivo"};
}

FidelityClaim gamutMapping() {
    return {"mapeamento para o gamut sRGB", Fidelity::Arbitrary, 0.0,
            "escolha do projeto (desaturate)", "",
            "mais de 90% das cores espectrais nao cabem no sRGB; toda saida e uma "
            "aproximacao dessaturada, e a estrategia de aproximacao e uma escolha"};
}

FidelityClaim luminanceNormalisation() {
    return {"normalizacao de luminancia", Fidelity::Arbitrary, 0.0,
            "escolha de visualizacao do projeto", "",
            "descarta a eficiencia luminosa real para tornar as cores comparaveis"};
}

FidelityClaim arbitraryMapping(const std::string& mapperName) {
    return {"mapeamento f_som -> f_EM (" + mapperName + ")", Fidelity::Arbitrary, 0.0,
            "funcao escolhida pelo usuario", "",
            "som e luz nao compartilham mecanismo nem escala; nao existe valor "
            "verdadeiro contra o qual medir o erro desta etapa"};
}

FidelityClaim fftResolution(double resolutionHz, double frequencyHz) {
    FidelityClaim claim;
    claim.quantity = "frequencia dominante (FFT + interpolacao parabolica)";
    claim.tier = Fidelity::Measured;
    // Limite empirico verificado em tests/test_analyzer.cpp: o erro da
    // interpolacao parabolica fica abaixo de 5% de um bin para tom puro.
    claim.relativeUncertainty =
        frequencyHz > 0.0 ? (0.05 * resolutionHz) / frequencyHz : 0.0;
    claim.source = "medido pela suite de testes do projeto";
    std::ostringstream validity;
    validity << "df = " << resolutionHz << " Hz; tom puro bem resolvido";
    claim.validity = validity.str();
    claim.caveat =
        "vale para um tom isolado; com parciais sobrepostos ou vazamento o erro "
        "cresce, e abaixo de df duas notas nao sao separaveis de forma alguma";
    return claim;
}

}  // namespace claims
}  // namespace soundwave
