#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Kernel- and vendor-neutral value types shared by every layer.
namespace emv {

enum class EntryMode { Tap, Insert, Swipe, Manual };
enum class CryptogramType { AAC, TC, ARQC };

std::string toString(EntryMode mode);
std::string toString(CryptogramType type);

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
};

struct HostResponse {
    bool approved{false};
    std::string responseCode;
    std::string authorizationCode;
    std::optional<std::string> issuerAuthenticationData;
    std::string message;
};

struct OnlineCompletion {
    // Empty when the kernel has no second GENERATE AC (e.g. most contactless kernels).
    std::optional<FinalCryptogram> finalCryptogram;
    bool issuerScriptsProcessed{false};
    std::string issuerScriptStatus;
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
};

} // namespace emv
