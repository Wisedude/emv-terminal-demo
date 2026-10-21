#pragma once
#include "emv/config/emv_configuration.hpp"
#include "emv/kernel/phased_emv_kernel.hpp"
#include "simulated/simulated_emv_engine.hpp"
#include "simulated/simulation_options.hpp"
#include <memory>

namespace emv::simulated {

// Stand-in for a contact kernel SDK that exposes each EMV Book 3 phase as its own call.
// A real vendor adapter has the same shape: derive from PhasedEmvKernel, forward each phase to the SDK.
class SimulatedContactKernel final : public PhasedEmvKernel {
public:
    SimulatedContactKernel(std::shared_ptr<const config::EmvConfiguration> configuration, SimulationOptions options = {});

    std::vector<ApplicationCandidate> buildCandidateList(const CardProfile& card) override;
    OnlineCompletion completeOnlineTransaction(const HostResponse& issuerResponse) override;
    TlvList transactionData() const override;

private:
    std::string initiateApplicationProcessing(const KernelTransaction& transaction) override;
    std::string readApplicationData(const KernelTransaction& transaction) override;
    std::string checkProcessingRestrictions(const KernelTransaction& transaction) override;
    std::string performOfflineDataAuthentication(const KernelTransaction& transaction) override;
    std::string validateCertificates(const KernelTransaction& transaction) override;
    std::string performCardholderVerification(const KernelTransaction& transaction) override;
    std::string performTerminalRiskManagement(const KernelTransaction& transaction) override;
    std::string performTerminalActionAnalysis(const KernelTransaction& transaction) override;
    FirstCryptogram firstGenerateAc(const KernelTransaction& transaction) override;

    SimulatedEmvEngine engine_;
};

} // namespace emv::simulated
