#include "emv/transaction_service.hpp"
#include <stdexcept>
#include <utility>

namespace emv {
namespace {

TransactionResult offlineResult(const KernelTransaction& transaction, const FirstCryptogram& first,
                                TlvList emvData) {
    TransactionResult result;
    result.approved = first.type == CryptogramType::TC;
    result.message = result.approved ? "Approved offline" : "Declined offline";
    result.selectedAid = transaction.application.aid;
    result.first = first;
    result.issuerScriptStatus = "Not applicable: transaction did not go online";
    result.emvData = std::move(emvData);
    return result;
}

void reportOfflineSkips(ITransactionObserver& observer) {
    const std::string reason = "Skipped: first GENERATE AC did not request online authorization";
    observer.onStage(TransactionStage::OnlineAuthorization, StageStatus::Skipped, reason);
    observer.onStage(TransactionStage::SecondCardActionAnalysis, StageStatus::Skipped, reason);
    observer.onStage(TransactionStage::IssuerScriptProcessing, StageStatus::Skipped, reason);
}

std::string withIssuerAuthentication(const std::string& detail, const OnlineCompletion& completion) {
    return completion.issuerAuthenticationStatus.empty() ? detail
                                                         : completion.issuerAuthenticationStatus + " · " + detail;
}

void reportOnlineCompletion(ITransactionObserver& observer, const OnlineCompletion& completion) {
    if (completion.finalCryptogram) {
        observer.onStage(TransactionStage::SecondCardActionAnalysis, StageStatus::Completed,
                         withIssuerAuthentication(
                             "Second GENERATE AC returned " + toString(completion.finalCryptogram->type), completion));
    } else {
        observer.onStage(TransactionStage::SecondCardActionAnalysis, StageStatus::Skipped,
                         withIssuerAuthentication("Kernel has no second GENERATE AC", completion));
    }
    observer.onStage(TransactionStage::IssuerScriptProcessing,
                     completion.issuerScriptsProcessed ? StageStatus::Completed : StageStatus::Skipped,
                     completion.issuerScriptStatus);
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
    NullTransactionObserver observer;
    return runSale(mode, amountMinor, transactionId, observer);
}

TransactionResult TransactionService::runSale(EntryMode mode, std::int64_t amountMinor, const std::string& transactionId,
                                              ITransactionObserver& observer) {
    if (amountMinor <= 0) throw std::invalid_argument("Amount must be positive minor units");
    if (transactionId.empty()) throw std::invalid_argument("Transaction ID is required");

    const CardProfile card = reader_.waitForCard(mode);
    const KernelTransaction transaction{selectApplication(card, observer), mode, amountMinor, config_.currencyCode,
                                        transactionId};

    const FirstCryptogram first = kernel_.processUntilFirstCryptogram(transaction, observer);
    if (first.type != CryptogramType::ARQC) {
        reportOfflineSkips(observer);
        return offlineResult(transaction, first, kernel_.transactionData());
    }
    return authorizeOnline(card, transaction, first, observer);
}

ApplicationCandidate TransactionService::selectApplication(const CardProfile& card, ITransactionObserver& observer) {
    observer.onStage(TransactionStage::ApplicationSelection, StageStatus::Started, "");
    const auto candidates = kernel_.buildCandidateList(card);
    if (candidates.empty()) {
        throw std::runtime_error("Application selection failed: the card has no application (AID) supported by "
                                 "this terminal");
    }

    auto selected = selector_.select(candidates);
    if (!selected.has_value()) throw std::runtime_error("Application selection cancelled or failed");
    observer.onStage(TransactionStage::ApplicationSelection, StageStatus::Completed,
                     "Selected AID " + selected->aid + " (" + selected->label + ")");
    return std::move(*selected);
}

TransactionResult TransactionService::authorizeOnline(const CardProfile& card, const KernelTransaction& transaction,
                                                      const FirstCryptogram& first, ITransactionObserver& observer) {
    HostRequest request;
    request.amountMinor = transaction.amountMinor;
    request.currencyCode = transaction.currencyCode;
    request.aid = transaction.application.aid;
    request.transactionId = transaction.transactionId;
    request.cryptogramType = first.type;
    request.opaqueCryptogram = first.opaqueCryptogram;
    request.cardToken = card.token;
    request.iccData = first.iccData;

    // Do not translate host timeout/error into approval. Real systems require scheme/kernel-specific recovery.
    observer.onStage(TransactionStage::OnlineAuthorization, StageStatus::Started, "");
    const HostResponse host = host_.authorize(request);
    observer.onStage(TransactionStage::OnlineAuthorization, StageStatus::Completed,
                     "Host response " + host.responseCode + ": " + host.message);

    observer.onStage(TransactionStage::SecondCardActionAnalysis, StageStatus::Started, "");
    const OnlineCompletion completion = kernel_.completeOnlineTransaction(host);
    reportOnlineCompletion(observer, completion);

    TransactionResult result;
    result.approved = isOnlineApproval(host, completion);
    result.message = result.approved ? "Approved" : "Declined";
    result.selectedAid = transaction.application.aid;
    result.first = first;
    result.final = completion.finalCryptogram;
    result.host = host;
    result.issuerScriptsProcessed = completion.issuerScriptsProcessed;
    result.issuerScriptStatus = completion.issuerScriptStatus;
    result.emvData = kernel_.transactionData();
    return result;
}

} // namespace emv
