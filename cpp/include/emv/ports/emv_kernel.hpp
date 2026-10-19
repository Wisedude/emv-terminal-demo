#pragma once
#include "emv/domain.hpp"
#include <vector>

namespace emv {

// What the transaction service needs from any EMV level-2 kernel, and nothing more.
//
// The contract is deliberately coarse: how a kernel gets from a selected application
// to its first cryptogram (discrete Book 3 phases, one vendor performTransaction() call,
// a contactless C-x kernel, ...) is the adapter's business, not the orchestrator's.
// Adapters for SDKs that expose individual phases can derive from PhasedEmvKernel.
//
// Lifecycle per transaction: buildCandidateList -> processUntilFirstCryptogram
// -> completeOnlineTransaction (only when the first cryptogram is an ARQC).
class IEmvKernel {
public:
    virtual ~IEmvKernel() = default;

    virtual std::vector<ApplicationCandidate> buildCandidateList(const CardProfile& card) = 0;

    // Runs all pre-authorisation processing and the first GENERATE AC.
    virtual FirstCryptogram processUntilFirstCryptogram(const KernelTransaction& transaction) = 0;

    // The kernel owns issuer authentication, issuer script timing and any second GENERATE AC.
    virtual OnlineCompletion completeOnlineTransaction(const HostResponse& issuerResponse) = 0;
};

} // namespace emv
