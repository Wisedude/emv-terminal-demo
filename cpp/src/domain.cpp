#include "emv/domain.hpp"
#include <stdexcept>

namespace emv {

std::string toString(EntryMode mode) {
    switch (mode) {
        case EntryMode::Tap: return "Tap";
        case EntryMode::Insert: return "Insert";
        case EntryMode::Swipe: return "Swipe";
        case EntryMode::Manual: return "Manual";
    }
    throw std::logic_error("Unknown entry mode");
}

std::string toString(CryptogramType type) {
    switch (type) {
        case CryptogramType::AAC: return "AAC";
        case CryptogramType::TC: return "TC";
        case CryptogramType::ARQC: return "ARQC";
    }
    throw std::logic_error("Unknown cryptogram type");
}

std::string toString(TransactionStage stage) {
    switch (stage) {
        case TransactionStage::ApplicationSelection: return "APPLICATION_SELECTION";
        case TransactionStage::InitiateApplicationProcessing: return "INITIATE_APPLICATION_PROCESSING";
        case TransactionStage::ReadApplicationData: return "READ_APPLICATION_DATA";
        case TransactionStage::ProcessingRestrictions: return "PROCESSING_RESTRICTIONS";
        case TransactionStage::OfflineDataAuthentication: return "OFFLINE_DATA_AUTHENTICATION";
        case TransactionStage::Certificates: return "CERTIFICATES";
        case TransactionStage::CardholderVerification: return "CARDHOLDER_VERIFICATION";
        case TransactionStage::TerminalRiskManagement: return "TERMINAL_RISK_MANAGEMENT";
        case TransactionStage::TerminalActionAnalysis: return "TERMINAL_ACTION_ANALYSIS";
        case TransactionStage::FirstCardActionAnalysis: return "FIRST_CARD_ACTION_ANALYSIS";
        case TransactionStage::OnlineAuthorization: return "ONLINE_AUTHORIZATION";
        case TransactionStage::SecondCardActionAnalysis: return "SECOND_CARD_ACTION_ANALYSIS";
        case TransactionStage::IssuerScriptProcessing: return "ISSUER_SCRIPT_PROCESSING";
    }
    throw std::logic_error("Unknown transaction stage");
}

std::string toString(StageStatus status) {
    switch (status) {
        case StageStatus::Started: return "STARTED";
        case StageStatus::Completed: return "COMPLETED";
        case StageStatus::Skipped: return "SKIPPED";
    }
    throw std::logic_error("Unknown stage status");
}

std::optional<EntryMode> entryModeFromString(const std::string& text) {
    for (const auto mode : {EntryMode::Tap, EntryMode::Insert, EntryMode::Swipe, EntryMode::Manual}) {
        if (toString(mode) == text) return mode;
    }
    return std::nullopt;
}

std::optional<CryptogramType> cryptogramTypeFromString(const std::string& text) {
    for (const auto type : {CryptogramType::AAC, CryptogramType::TC, CryptogramType::ARQC}) {
        if (toString(type) == text) return type;
    }
    return std::nullopt;
}

} // namespace emv
