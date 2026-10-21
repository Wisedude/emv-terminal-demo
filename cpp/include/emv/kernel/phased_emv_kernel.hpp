#pragma once
#include "emv/ports/emv_kernel.hpp"
#include <string>

namespace emv {

// Base for adapters over kernel SDKs that expose the EMV Book 3 phases as separate calls.
// The phase order is fixed here once (Template Method), so a vendor adapter only maps each phase onto
// its SDK and cannot get the sequence wrong. Each phase returns a short detail for the progress display
// (empty when there is nothing to add).
class PhasedEmvKernel : public IEmvKernel {
public:
    FirstCryptogram processUntilFirstCryptogram(const KernelTransaction& transaction,
                                                ITransactionObserver& observer) final;

protected:
    virtual std::string initiateApplicationProcessing(const KernelTransaction& transaction) = 0;
    virtual std::string readApplicationData(const KernelTransaction& transaction) = 0;
    virtual std::string checkProcessingRestrictions(const KernelTransaction& transaction) = 0;
    virtual std::string performOfflineDataAuthentication(const KernelTransaction& transaction) = 0;
    virtual std::string validateCertificates(const KernelTransaction& transaction) = 0;
    virtual std::string performCardholderVerification(const KernelTransaction& transaction) = 0;
    virtual std::string performTerminalRiskManagement(const KernelTransaction& transaction) = 0;
    virtual std::string performTerminalActionAnalysis(const KernelTransaction& transaction) = 0;
    virtual FirstCryptogram firstGenerateAc(const KernelTransaction& transaction) = 0;
};

} // namespace emv
