#pragma once
#include "emv/ports/application_selector.hpp"
#include <string>
#include <vector>

namespace emv {

// Picks the first candidate matching the merchant's AID preference order,
// falling back to the first candidate the kernel offered.
class PreferredAidSelector final : public IApplicationSelector {
public:
    explicit PreferredAidSelector(std::vector<std::string> preferredAids);
    std::optional<ApplicationCandidate> select(const std::vector<ApplicationCandidate>& candidates) override;

private:
    std::vector<std::string> preferredAids_;
};

} // namespace emv
