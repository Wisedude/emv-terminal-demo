#include "emv/kernel/phased_emv_kernel.hpp"

namespace emv {

FirstCryptogram PhasedEmvKernel::processUntilFirstCryptogram(const KernelTransaction& transaction) {
    initiateApplicationProcessing(transaction);
    readApplicationData(transaction);
    checkProcessingRestrictions(transaction);
    performOfflineDataAuthentication(transaction);
    validateCertificates(transaction);
    performCardholderVerification(transaction);
    performTerminalRiskManagement(transaction);
    performTerminalActionAnalysis(transaction);
    return firstGenerateAc(transaction);
}

} // namespace emv
