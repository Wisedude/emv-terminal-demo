#pragma once
#include "emv/domain.hpp"
#include <optional>
#include <vector>

namespace emv {

// Terminal-side choice among the kernel's mutually supported applications
// (cardholder prompt, merchant priority, scheme rules). Kept out of the kernel so
// the policy survives a kernel swap.
class IApplicationSelector {
public:
    virtual ~IApplicationSelector() = default;
    // Returns nullopt when the cardholder cancels or no candidate is acceptable.
    virtual std::optional<ApplicationCandidate> select(const std::vector<ApplicationCandidate>& candidates) = 0;
};

} // namespace emv
