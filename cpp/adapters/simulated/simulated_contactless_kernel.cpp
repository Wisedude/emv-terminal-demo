#include "simulated/simulated_contactless_kernel.hpp"
#include <utility>

namespace emv::simulated {
namespace {

// The kernel does all of this inside one call; it still emits progress events, as vendor kernels do.
template <typename Step>
void report(ITransactionObserver& observer, TransactionStage stage, Step step) {
    observer.onStage(stage, StageStatus::Started, "");
    observer.onStage(stage, StageStatus::Completed, "Inside the contactless kernel: " + step());
}

} // namespace

SimulatedContactlessKernel::SimulatedContactlessKernel(std::shared_ptr<const config::EmvConfiguration> configuration,
                                                       SimulationOptions options)
    : engine_(std::move(configuration), CardInterface::Contactless, options.cardBehaviour) {}

std::vector<ApplicationCandidate> SimulatedContactlessKernel::buildCandidateList(const CardProfile& card) {
    return engine_.buildCandidateList(card);
}

FirstCryptogram SimulatedContactlessKernel::processUntilFirstCryptogram(const KernelTransaction& transaction,
                                                                        ITransactionObserver& observer) {
    using Stage = TransactionStage;
    report(observer, Stage::InitiateApplicationProcessing, [&] { return engine_.initiateApplicationProcessing(transaction); });
    report(observer, Stage::ReadApplicationData, [&] { return engine_.readApplicationData(); });
    report(observer, Stage::ProcessingRestrictions, [&] { return engine_.checkProcessingRestrictions(); });
    report(observer, Stage::OfflineDataAuthentication, [&] { return engine_.selectOfflineDataAuthentication(); });
    report(observer, Stage::Certificates, [&] { return engine_.authenticateOfflineData(); });
    report(observer, Stage::CardholderVerification, [&] { return engine_.verifyCardholder(); });
    report(observer, Stage::TerminalRiskManagement, [&] { return engine_.manageTerminalRisk(); });
    report(observer, Stage::TerminalActionAnalysis, [&] { return engine_.analyseTerminalActions(); });

    observer.onStage(Stage::FirstCardActionAnalysis, StageStatus::Started, "");
    const FirstCryptogram first = engine_.generateFirstAc();
    observer.onStage(Stage::FirstCardActionAnalysis, StageStatus::Completed,
                     "Contactless kernel returned " + toString(first.type));
    return first;
}

OnlineCompletion SimulatedContactlessKernel::completeOnlineTransaction(const HostResponse& issuerResponse) {
    return engine_.completeOnlineTransaction(issuerResponse);
}

TlvList SimulatedContactlessKernel::transactionData() const {
    return engine_.transactionData();
}

} // namespace emv::simulated
