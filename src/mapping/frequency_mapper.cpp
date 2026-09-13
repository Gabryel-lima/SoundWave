#include "soundwave/mapping/frequency_mapper.hpp"

#include <cmath>

namespace soundwave {

std::string_view outOfRangePolicyName(OutOfRangePolicy policy) {
    switch (policy) {
        case OutOfRangePolicy::Clamp:       return "clamp";
        case OutOfRangePolicy::Extrapolate: return "extrapolate";
        case OutOfRangePolicy::Discard:     return "discard";
    }
    return "unknown";
}

bool parseOutOfRangePolicy(std::string_view name, OutOfRangePolicy& out) {
    if (name == "clamp") {
        out = OutOfRangePolicy::Clamp;
    } else if (name == "extrapolate") {
        out = OutOfRangePolicy::Extrapolate;
    } else if (name == "discard" || name == "drop") {
        out = OutOfRangePolicy::Discard;
    } else {
        return false;
    }
    return true;
}

bool MappingDomain::valid() const {
    return sourceMinHz > 0.0 && sourceMaxHz > sourceMinHz && targetMinHz > 0.0 &&
           targetMaxHz > targetMinHz;
}

double MappingDomain::sourceOctaves() const {
    if (!valid()) return 0.0;
    return std::log2(sourceMaxHz / sourceMinHz);
}

double MappingDomain::targetOctaves() const {
    if (!valid()) return 0.0;
    return std::log2(targetMaxHz / targetMinHz);
}

double MappingDomain::compressionRatio() const {
    const double target = targetOctaves();
    return target > 0.0 ? sourceOctaves() / target : 0.0;
}

}  // namespace soundwave
