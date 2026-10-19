#include "simulated/simulated_contactless_kernel.hpp"
#include <iostream>

namespace emv::simulated {

std::vector<ApplicationCandidate> SimulatedContactlessKernel::buildCandidateList(const CardProfile& card) {
    std::cout << "[Contactless kernel] Entry point: " << card.applications.size() << " combination(s) found\n";
    return card.applications;
}

FirstCryptogram SimulatedContactlessKernel::processUntilFirstCryptogram(const KernelTransaction& transaction) {
    std::cout << "[Contactless kernel] Perform transaction for " << transaction.application.aid
              << ", amount minor=" << transaction.amountMinor << " (single kernel call, simulated)\n";
    // Opaque placeholder only. Never generate/derive EMV cryptograms in the application layer.
    return {CryptogramType::ARQC, "SIMULATED-CL-ARQC"};
}

OnlineCompletion SimulatedContactlessKernel::completeOnlineTransaction(const HostResponse& issuerResponse) {
    std::cout << "[Contactless kernel] Outcome from issuer response " << issuerResponse.responseCode
              << " (card already removed; no second GENERATE AC)\n";

    OnlineCompletion completion;
    completion.issuerScriptStatus = "Not applicable: contactless card left the field";
    return completion;
}

} // namespace emv::simulated
