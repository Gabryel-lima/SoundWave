#include "soundwave/physics/medium_filter.hpp"

#include <cmath>
#include <limits>

#include "soundwave/core/constants.hpp"

namespace soundwave {
namespace {

// dB -> fator linear de amplitude. Atenuacao de A dB corresponde a 10^(-A/20).
double decibelsToAmplitude(double attenuationDb) {
    return std::pow(10.0, -attenuationDb / 20.0);
}

double halfDistance(double dbPerMetre) {
    // -3 dB e a queda de metade da POTENCIA (amplitude 1/sqrt(2)), que e a
    // convencao usual para "distancia de meia potencia".
    return dbPerMetre > 0.0 ? 3.0 / dbPerMetre : std::numeric_limits<double>::infinity();
}

}  // namespace

double acousticTransmittance(const Medium& medium, double frequencyHz, double pathLengthM,
                             const Conditions& conditions) {
    if (!(pathLengthM > 0.0)) return 1.0;
    const double db = soundAbsorptionDbPerMetre(medium, frequencyHz, conditions) * pathLengthM;
    return decibelsToAmplitude(db);
}

double opticalTransmittance(const Medium& medium, double vacuumWavelengthNm, double pathLengthM) {
    if (!(pathLengthM > 0.0)) return 1.0;
    const double db = opticalAbsorptionDbPerMetre(medium, vacuumWavelengthNm) * pathLengthM;
    return decibelsToAmplitude(db);
}

Spectrum applyAcousticFilter(const Spectrum& spectrum, const MediumFilterSettings& settings) {
    Spectrum filtered = spectrum;
    for (SpectralBin& bin : filtered.bins) {
        bin.magnitude *= acousticTransmittance(settings.acoustic, bin.frequencyHz,
                                               settings.pathLengthM, settings.conditions);
    }
    return filtered;
}

std::vector<FilterPoint> acousticProfile(const MediumFilterSettings& settings,
                                         const std::vector<double>& frequenciesHz) {
    std::vector<FilterPoint> points;
    points.reserve(frequenciesHz.size());
    for (double f : frequenciesHz) {
        FilterPoint point;
        point.frequencyHz = f;
        point.attenuationDbPerM =
            soundAbsorptionDbPerMetre(settings.acoustic, f, settings.conditions);
        point.transmittance =
            acousticTransmittance(settings.acoustic, f, settings.pathLengthM, settings.conditions);
        point.halfDistanceM = halfDistance(point.attenuationDbPerM);
        points.push_back(point);
    }
    return points;
}

std::vector<FilterPoint> opticalProfile(const MediumFilterSettings& settings,
                                        const std::vector<double>& wavelengthsNm) {
    std::vector<FilterPoint> points;
    points.reserve(wavelengthsNm.size());
    for (double nm : wavelengthsNm) {
        FilterPoint point;
        point.wavelengthNm = nm;
        point.frequencyHz = nm > 0.0 ? kSpeedOfLight / (nm * 1e-9) : 0.0;
        point.attenuationDbPerM = opticalAbsorptionDbPerMetre(settings.optical, nm);
        point.transmittance = opticalTransmittance(settings.optical, nm, settings.pathLengthM);
        point.halfDistanceM = halfDistance(point.attenuationDbPerM);
        points.push_back(point);
    }
    return points;
}

double mostTransmittedVisibleNm(const Medium& medium) {
    double best = 0.0;
    double lowest = std::numeric_limits<double>::infinity();
    for (double nm = 400.0; nm <= 750.0; nm += 1.0) {
        const double alpha = opticalAbsorptionDbPerMetre(medium, nm);
        if (alpha < lowest) {
            lowest = alpha;
            best = nm;
        }
    }
    return best;
}

double mostTransmittedAudibleHz(const Medium& medium, double lowHz, double highHz,
                                const Conditions& conditions) {
    double best = lowHz;
    double lowest = std::numeric_limits<double>::infinity();
    // Varredura logaritmica: a absorcao varia por ordens de grandeza na banda.
    for (double f = lowHz; f <= highHz; f *= 1.02) {
        const double alpha = soundAbsorptionDbPerMetre(medium, f, conditions);
        if (alpha < lowest) {
            lowest = alpha;
            best = f;
        }
    }
    return best;
}

FidelityBudget filterFidelity(const MediumFilterSettings& settings, double vacuumWavelengthNm) {
    FidelityBudget budget;
    budget.add(soundAbsorptionClaim(settings.acoustic));
    budget.add(opticalAbsorptionClaim(settings.optical, vacuumWavelengthNm));
    return budget;
}

}  // namespace soundwave
