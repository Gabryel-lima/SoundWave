#include "soundwave/core/version.hpp"

namespace soundwave {

std::string versionString() {
    return std::to_string(kVersionMajor) + "." + std::to_string(kVersionMinor) + "." +
           std::to_string(kVersionPatch);
}

}  // namespace soundwave
