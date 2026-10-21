#include "emv/kernel/phased_emv_kernel.hpp"

namespace emv {
namespace {

template <typename Phase>
void runPhase(ITransactionObserver& observer, TransactionStage stage, Phase phase) {
    observer.onStage(stage, StageStatus::Started, "");
    const std::string detail = phase();
    observer.onStage(stage, StageStatus::Completed, detail);
}

} // namespace

FirstCryptogram PhasedEmvKernel::processUntilFirstCryptogram(const KernelTransaction& transaction,
                                                             ITransactionObserver& observer) {
    using Stage = TransactionStage;
    runPhase(observer, Stage::InitiateApplicationProcessing, [&] { return initiateApplicationProcessing(transaction); });
    runPhase(observer, Stage::ReadApplicationData, [&] { return readApplicationData(transaction); });
    runPhase(observer, Stage::ProcessingRestrictions, [&] { return checkProcessingRestrictions(transaction); });
    runPhase(observer, Stage::OfflineDataAuthentication, [&] { return performOfflineDataAuthentication(transaction); });
    runPhase(observer, Stage::Certificates, [&] { return validateCertificates(transaction); });
    runPhase(observer, Stage::CardholderVerification, [&] { return performCardholderVerification(transaction); });
    runPhase(observer, Stage::TerminalRiskManagement, [&] { return performTerminalRiskManagement(transaction); });
    runPhase(observer, Stage::TerminalActionAnalysis, [&] { return performTerminalActionAnalysis(transaction); });

    observer.onStage(Stage::FirstCardActionAnalysis, StageStatus::Started, "");
    const FirstCryptogram first = firstGenerateAc(transaction);
    observer.onStage(Stage::FirstCardActionAnalysis, StageStatus::Completed,
                     "First GENERATE AC returned " + toString(first.type));
    return first;
}

} // namespace emv
