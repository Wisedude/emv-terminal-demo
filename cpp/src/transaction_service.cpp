#include "emv/transaction_service.hpp"
#include <stdexcept>
#include <utility>

namespace emv {
namespace {

TransactionResult offlineResult(const KernelTransaction& transaction, const FirstCryptogram& first) {
    TransactionResult result;
    result.approved = first.type == CryptogramType::TC;
    result.message = result.approved ? "Approved offline" : "Declined offline";
    result.selectedAid = transaction.application.aid;
    result.first = first;
    result.issuerScriptStatus = "Not applicable: transaction did not go online";
    return result;
}

// The kernel has the last word: an issuer approval still declines if the card returns an AAC.
bool isOnlineApproval(const HostResponse& host, const OnlineCompletion& completion) {
    const bool kernelAccepted = !completion.finalCryptogram.has_value()
        || completion.finalCryptogram->type == CryptogramType::TC;
    return host.approved && kernelAccepted;
}

} // namespace

TransactionService::TransactionService(TerminalConfig config,
                                       ICardReader& reader,
                                       IEmvKernel& kernel,
                                       IApplicationSelector& selector,
                                       IHostAuthorizer& host)
    : config_(std::move(config)), reader_(reader), kernel_(kernel), selector_(selector), host_(host) {
    if (config_.currencyCode.empty()) throw std::invalid_argument("Terminal currency code is required");
}

TransactionResult TransactionService::runSale(EntryMode mode, std::int64_t amountMinor, const std::string& transactionId) {
    if (amountMinor <= 0) throw std::invalid_argument("Amount must be positive minor units");
    if (transactionId.empty()) throw std::invalid_argument("Transaction ID is required");

    const CardProfile card = reader_.waitForCard(mode);
    const KernelTransaction transaction{selectApplication(card), mode, amountMinor, config_.currencyCode, transactionId};

    const FirstCryptogram first = kernel_.processUntilFirstCryptogram(transaction);
    if (first.type != CryptogramType::ARQC) return offlineResult(transaction, first);
    return authorizeOnline(card, transaction, first);
}

ApplicationCandidate TransactionService::selectApplication(const CardProfile& card) {
    const auto candidates = kernel_.buildCandidateList(card);
    if (candidates.empty()) throw std::runtime_error("No supported EMV applications found");

    auto selected = selector_.select(candidates);
    if (!selected.has_value()) throw std::runtime_error("Application selection cancelled or failed");
    return std::move(*selected);
}

TransactionResult TransactionService::authorizeOnline(const CardProfile& card, const KernelTransaction& transaction,
                                                      const FirstCryptogram& first) {
    HostRequest request;
    request.amountMinor = transaction.amountMinor;
    request.currencyCode = transaction.currencyCode;
    request.aid = transaction.application.aid;
    request.transactionId = transaction.transactionId;
    request.cryptogramType = first.type;
    request.opaqueCryptogram = first.opaqueCryptogram;
    request.cardToken = card.token;

    // Do not translate host timeout/error into approval. Real systems require scheme/kernel-specific recovery.
    const HostResponse host = host_.authorize(request);
    const OnlineCompletion completion = kernel_.completeOnlineTransaction(host);

    TransactionResult result;
    result.approved = isOnlineApproval(host, completion);
    result.message = result.approved ? "Approved" : "Declined";
    result.selectedAid = transaction.application.aid;
    result.first = first;
    result.final = completion.finalCryptogram;
    result.host = host;
    result.issuerScriptsProcessed = completion.issuerScriptsProcessed;
    result.issuerScriptStatus = completion.issuerScriptStatus;
    return result;
}

} // namespace emv
