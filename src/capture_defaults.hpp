#pragma once
#include <optional>

namespace ps {
// Resolve only known Windows client defaults. A rollout/unknown version is not guessed.
inline std::optional<bool> windowsCaptureDefault(unsigned major,unsigned minor,unsigned build,
    std::optional<unsigned> revision,bool workstation) {
    if(!workstation || major!=10 || minor!=0) return std::nullopt;
    if(build>=15063 && build<=19045) return false;
    if(build==22000) return false;
    if(build==22621) {
        if(!revision) return std::nullopt;
        if(*revision<1546) return false;
        if(*revision>=1928) return true;
        // The 22H2 feature was gradually rolled out before KB5027303.
        return std::nullopt;
    }
    if(build>=22631) return true;
    return std::nullopt;
}
}
