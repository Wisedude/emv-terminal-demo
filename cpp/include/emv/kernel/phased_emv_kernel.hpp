#pragma once
#include "emv/ports/emv_kernel.hpp"

namespace emv {

// Base for adapters over kernel SDKs that expose the EMV Book 3 phases as separate calls.
// The phase order is fixed here once (Template Method), so a vendor adapter only maps
// each phase onto its SDK and cannot get the sequence wrong.
class PhasedEmvKernel : public IEmvKernel {
public:
    FirstCryptogram processUntilFirstCryptogram(const KernelTransaction& transaction) final;

protected:
    virtual void initiateApplicationProcessing(const KernelTransaction& transaction) = 0;
    virtual void readApplicationData(const KernelTransaction& transaction) = 0;
    virtual void checkProcessingRestrictions(const KernelTransaction& transaction) = 0;
    virtual void performOfflineDataAuthentication(const KernelTransaction& transaction) = 0;
    virtual void validateCertificates(const KernelTransaction& transaction) = 0;
    virtual void performCardholderVerification(const KernelTransaction& transaction) = 0;
    virtual void performTerminalRiskManagement(const KernelTransaction& transaction) = 0;
    virtual void performTerminalActionAnalysis(const KernelTransaction& transaction) = 0;
    virtual FirstCryptogram firstGenerateAc(const KernelTransaction& transaction) = 0;
};

} // namespace emv
