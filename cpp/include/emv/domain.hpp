#pragma once
#include "emv/tlv.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Kernel- and vendor-neutral value types shared by every layer.
namespace emv {

enum class EntryMode { Tap, Insert, Swipe, Manual };
enum class CryptogramType { AAC, TC, ARQC };

// The 13 externally visible stages of an EMV sale, in order.
enum class TransactionStage {
    ApplicationSelection,
    InitiateApplicationProcessing,
    ReadApplicationData,
    ProcessingRestrictions,
    OfflineDataAuthentication,
    Certificates,
    CardholderVerification,
    TerminalRiskManagement,
    TerminalActionAnalysis,
    FirstCardActionAnalysis,
    OnlineAuthorization,
    SecondCardActionAnalysis,
    IssuerScriptProcessing,
};
enum class StageStatus { Started, Completed, Skipped };

std::string toString(EntryMode mode);
std::string toString(CryptogramType type);
std::string toString(TransactionStage stage);
std::string toString(StageStatus status);

// Parse the toString() forms; nullopt for anything else.
std::optional<EntryMode> entryModeFromString(const std::string& text);
std::optional<CryptogramType> cryptogramTypeFromString(const std::string& text);

struct TerminalConfig {
    std::string currencyCode;
};

struct ApplicationCandidate {
    std::string aid;
    std::string label;
};

struct CardProfile {
    std::string token;
    std::vector<ApplicationCandidate> applications;
};

// Cryptograms are opaque to the application layer: only the kernel produces them.
struct FirstCryptogram {
    CryptogramType type{CryptogramType::AAC};
    std::string opaqueCryptogram;
    Bytes iccData;  // BER-TLV ICC data for the authorisation request (ISO 8583 DE 55), built by the kernel
};

struct FinalCryptogram {
    CryptogramType type{CryptogramType::AAC};
    std::string opaqueCryptogram;
};

// Everything a kernel needs to process one transaction for the selected application.
struct KernelTransaction {
    ApplicationCandidate application;
    EntryMode entryMode{EntryMode::Insert};
    std::int64_t amountMinor{0};
    std::string currencyCode;
    std::string transactionId;
};

struct HostRequest {
    std::int64_t amountMinor{0};
    std::string currencyCode;
    std::string aid;
    std::string transactionId;
    CryptogramType cryptogramType{CryptogramType::ARQC};
    std::string opaqueCryptogram;
    std::string cardToken;
    Bytes iccData;  // DE 55: proves to the host that this was an EMV chip transaction
};

struct HostResponse {
    bool approved{false};
    std::string responseCode;
    std::string authorizationCode;
    std::optional<Bytes> issuerAuthenticationData;  // tag 91: ARPC + response code, for the card to verify
    std::string message;
    std::optional<Bytes> issuerScripts;  // tag 71 / 72 templates, applied to the card by the kernel
};

struct OnlineCompletion {
    // Empty when the kernel has no second GENERATE AC (e.g. most contactless kernels).
    std::optional<FinalCryptogram> finalCryptogram;
    bool issuerScriptsProcessed{false};
    std::string issuerScriptStatus;
    std::string issuerAuthenticationStatus;
};

struct TransactionResult {
    bool approved{false};
    std::string message;
    std::string selectedAid;
    FirstCryptogram first;
    std::optional<FinalCryptogram> final;
    std::optional<HostResponse> host;
    bool issuerScriptsProcessed{false};
    std::string issuerScriptStatus;
    TlvList emvData;  // kernel data at the end of the transaction (TVR, TSI, CVM Results, ...), no PAN
};

} // namespace emv
