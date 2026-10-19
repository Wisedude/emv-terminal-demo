#pragma once
#include "emv/kernel/phased_emv_kernel.hpp"

namespace emv::simulated {

// Stand-in for a contact kernel SDK that exposes each EMV Book 3 phase as its own call.
// A real vendor adapter has the same shape: derive from PhasedEmvKernel, forward each phase to the SDK.
class SimulatedContactKernel final : public PhasedEmvKernel {
public:
    std::vector<ApplicationCandidate> buildCandidateList(const CardProfile& card) override;
    OnlineCompletion completeOnlineTransaction(const HostResponse& issuerResponse) override;

private:
    void initiateApplicationProcessing(const KernelTransaction& transaction) override;
    void readApplicationData(const KernelTransaction& transaction) override;
    void checkProcessingRestrictions(const KernelTransaction& transaction) override;
    void performOfflineDataAuthentication(const KernelTransaction& transaction) override;
    void validateCertificates(const KernelTransaction& transaction) override;
    void performCardholderVerification(const KernelTransaction& transaction) override;
    void performTerminalRiskManagement(const KernelTransaction& transaction) override;
    void performTerminalActionAnalysis(const KernelTransaction& transaction) override;
    FirstCryptogram firstGenerateAc(const KernelTransaction& transaction) override;
};

} // namespace emv::simulated
