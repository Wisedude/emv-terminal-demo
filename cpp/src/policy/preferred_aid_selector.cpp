#include "emv/policy/preferred_aid_selector.hpp"
#include <algorithm>
#include <utility>

namespace emv {

PreferredAidSelector::PreferredAidSelector(std::vector<std::string> preferredAids)
    : preferredAids_(std::move(preferredAids)) {}

std::optional<ApplicationCandidate> PreferredAidSelector::select(const std::vector<ApplicationCandidate>& candidates) {
    for (const auto& preferredAid : preferredAids_) {
        const auto match = std::find_if(candidates.begin(), candidates.end(),
            [&](const ApplicationCandidate& candidate) { return candidate.aid == preferredAid; });
        if (match != candidates.end()) return *match;
    }
    if (candidates.empty()) return std::nullopt;
    return candidates.front();
}

} // namespace emv
