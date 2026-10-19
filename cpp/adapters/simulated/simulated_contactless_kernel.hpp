#pragma once
#include "emv/ports/emv_kernel.hpp"

namespace emv::simulated {

// Stand-in for a contactless (C-x) kernel: one call runs the whole card interaction,
// and the card has left the field before the issuer replies, so there is no second
// GENERATE AC. Implements IEmvKernel directly to show that non-phased kernels plug in too.
class SimulatedContactlessKernel final : public IEmvKernel {
public:
    std::vector<ApplicationCandidate> buildCandidateList(const CardProfile& card) override;
    FirstCryptogram processUntilFirstCryptogram(const KernelTransaction& transaction) override;
    OnlineCompletion completeOnlineTransaction(const HostResponse& issuerResponse) override;
};

} // namespace emv::simulated
