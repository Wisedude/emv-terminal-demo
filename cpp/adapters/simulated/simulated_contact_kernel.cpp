#include "simulated/simulated_contact_kernel.hpp"
#include <iostream>

namespace emv::simulated {

std::vector<ApplicationCandidate> SimulatedContactKernel::buildCandidateList(const CardProfile& card) {
    std::cout << "[Contact kernel] Build candidate list: " << card.applications.size() << " application(s)\n";
    return card.applications;
}

void SimulatedContactKernel::initiateApplicationProcessing(const KernelTransaction& transaction) {
    std::cout << "[Contact kernel] Initiate application processing for " << transaction.application.aid << "\n";
}

void SimulatedContactKernel::readApplicationData(const KernelTransaction&) {
    std::cout << "[Contact kernel] Read application data (simulated)\n";
}

void SimulatedContactKernel::checkProcessingRestrictions(const KernelTransaction&) {
    std::cout << "[Contact kernel] Check application usage and processing restrictions (simulated)\n";
}

void SimulatedContactKernel::performOfflineDataAuthentication(const KernelTransaction&) {
    std::cout << "[Contact kernel] Perform offline data authentication (simulated)\n";
}

void SimulatedContactKernel::validateCertificates(const KernelTransaction&) {
    std::cout << "[Contact kernel] Validate EMV certificate chain as part of ODA (simulated)\n";
}

void SimulatedContactKernel::performCardholderVerification(const KernelTransaction&) {
    std::cout << "[Contact kernel] Perform cardholder verification (simulated)\n";
}

void SimulatedContactKernel::performTerminalRiskManagement(const KernelTransaction& transaction) {
    std::cout << "[Contact kernel] Terminal risk management for amount minor=" << transaction.amountMinor
              << " (simulated)\n";
}

void SimulatedContactKernel::performTerminalActionAnalysis(const KernelTransaction&) {
    std::cout << "[Contact kernel] Perform terminal action analysis (simulated)\n";
}

FirstCryptogram SimulatedContactKernel::firstGenerateAc(const KernelTransaction& transaction) {
    std::cout << "[Contact kernel] First card action analysis / GENERATE AC for " << transaction.application.aid
              << ", amount minor=" << transaction.amountMinor << "\n";
    // Opaque placeholder only. Never generate/derive EMV cryptograms in the application layer.
    return {CryptogramType::ARQC, "SIMULATED-ARQC"};
}

OnlineCompletion SimulatedContactKernel::completeOnlineTransaction(const HostResponse& issuerResponse) {
    std::cout << "[Contact kernel] Issuer response: " << issuerResponse.responseCode
              << "; issuer auth data present=" << (issuerResponse.issuerAuthenticationData ? "yes" : "no") << "\n"
              << "[Contact kernel] Process issuer scripts in kernel-defined order (none in stub)\n"
              << "[Contact kernel] Second card action analysis / GENERATE AC\n";

    OnlineCompletion completion;
    completion.finalCryptogram = issuerResponse.approved
        ? FinalCryptogram{CryptogramType::TC, "SIMULATED-TC"}
        : FinalCryptogram{CryptogramType::AAC, "SIMULATED-AAC"};
    completion.issuerScriptsProcessed = true;
    completion.issuerScriptStatus = "No issuer scripts supplied; kernel hook completed";
    return completion;
}

} // namespace emv::simulated
