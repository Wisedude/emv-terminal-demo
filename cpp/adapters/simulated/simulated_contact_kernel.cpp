#include "simulated/simulated_contact_kernel.hpp"
#include <utility>

namespace emv::simulated {

SimulatedContactKernel::SimulatedContactKernel(std::shared_ptr<const config::EmvConfiguration> configuration,
                                               SimulationOptions options)
    : engine_(std::move(configuration), CardInterface::Contact, options.cardBehaviour) {}

std::vector<ApplicationCandidate> SimulatedContactKernel::buildCandidateList(const CardProfile& card) {
    return engine_.buildCandidateList(card);
}

std::string SimulatedContactKernel::initiateApplicationProcessing(const KernelTransaction& transaction) {
    return engine_.initiateApplicationProcessing(transaction);
}

std::string SimulatedContactKernel::readApplicationData(const KernelTransaction&) {
    return engine_.readApplicationData();
}

std::string SimulatedContactKernel::checkProcessingRestrictions(const KernelTransaction&) {
    return engine_.checkProcessingRestrictions();
}

std::string SimulatedContactKernel::performOfflineDataAuthentication(const KernelTransaction&) {
    return engine_.selectOfflineDataAuthentication();
}

std::string SimulatedContactKernel::validateCertificates(const KernelTransaction&) {
    return engine_.authenticateOfflineData();
}

std::string SimulatedContactKernel::performCardholderVerification(const KernelTransaction&) {
    return engine_.verifyCardholder();
}

std::string SimulatedContactKernel::performTerminalRiskManagement(const KernelTransaction&) {
    return engine_.manageTerminalRisk();
}

std::string SimulatedContactKernel::performTerminalActionAnalysis(const KernelTransaction&) {
    return engine_.analyseTerminalActions();
}

FirstCryptogram SimulatedContactKernel::firstGenerateAc(const KernelTransaction&) {
    return engine_.generateFirstAc();
}

OnlineCompletion SimulatedContactKernel::completeOnlineTransaction(const HostResponse& issuerResponse) {
    return engine_.completeOnlineTransaction(issuerResponse);
}

TlvList SimulatedContactKernel::transactionData() const {
    return engine_.transactionData();
}

} // namespace emv::simulated
