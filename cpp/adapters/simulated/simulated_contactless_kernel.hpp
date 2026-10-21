#pragma once
#include "emv/config/emv_configuration.hpp"
#include "emv/ports/emv_kernel.hpp"
#include "simulated/simulated_emv_engine.hpp"
#include "simulated/simulation_options.hpp"
#include <memory>

namespace emv::simulated {

// Stand-in for a contactless (C-x) kernel: one call runs the whole card interaction, with the reader's
// contactless limits (transaction, floor and CVM-required) from the contactless AID file. The card has
// left the field before the issuer replies, so there is no second GENERATE AC and no issuer script or
// issuer authentication processing. Implements IEmvKernel directly: non-phased kernels plug in too.
class SimulatedContactlessKernel final : public IEmvKernel {
public:
    SimulatedContactlessKernel(std::shared_ptr<const config::EmvConfiguration> configuration,
                               SimulationOptions options = {});

    std::vector<ApplicationCandidate> buildCandidateList(const CardProfile& card) override;
    FirstCryptogram processUntilFirstCryptogram(const KernelTransaction& transaction,
                                                ITransactionObserver& observer) override;
    OnlineCompletion completeOnlineTransaction(const HostResponse& issuerResponse) override;
    TlvList transactionData() const override;

private:
    SimulatedEmvEngine engine_;
};

} // namespace emv::simulated
