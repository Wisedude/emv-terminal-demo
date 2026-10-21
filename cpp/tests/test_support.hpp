#pragma once
// Minimal test harness and port fakes shared by the C++ test executables.
#include "emv/ports/application_selector.hpp"
#include "emv/ports/card_reader.hpp"
#include "emv/ports/emv_kernel.hpp"
#include "emv/ports/host_authorizer.hpp"
#include "emv/ports/transaction_observer.hpp"
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace emv::test {

inline void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename Exception>
void requireThrows(const std::function<void()>& action, const std::string& message) {
    try {
        action();
    } catch (const Exception&) {
        return;
    }
    throw std::runtime_error(message);
}

struct TestCase {
    const char* name;
    void (*run)();
};

inline int runTests(const char* suite, const std::vector<TestCase>& tests) {
    int failures = 0;
    for (const auto& test : tests) {
        try {
            test.run();
            std::cout << "[PASS] " << suite << "." << test.name << "\n";
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << suite << "." << test.name << ": " << error.what() << "\n";
        }
    }
    return failures == 0 ? 0 : 1;
}

class FakeReader final : public ICardReader {
public:
    explicit FakeReader(std::vector<ApplicationCandidate> applications = {{"A0000000031010", "Visa test"}})
        : applications_(std::move(applications)) {}
    CardProfile waitForCard(EntryMode) override { return {"SIMULATED-CARD-01", applications_}; }

private:
    std::vector<ApplicationCandidate> applications_;
};

class FirstCandidateSelector final : public IApplicationSelector {
public:
    std::optional<ApplicationCandidate> select(const std::vector<ApplicationCandidate>& candidates) override {
        if (candidates.empty()) return std::nullopt;
        return candidates.front();
    }
};

class CancellingSelector final : public IApplicationSelector {
public:
    std::optional<ApplicationCandidate> select(const std::vector<ApplicationCandidate>&) override {
        return std::nullopt;
    }
};

// Configurable kernel: choose the first cryptogram and whether a second GENERATE AC exists.
class FakeKernel final : public IEmvKernel {
public:
    explicit FakeKernel(CryptogramType firstOutcome, bool hasSecondGenerateAc = true)
        : firstOutcome_(firstOutcome), hasSecondGenerateAc_(hasSecondGenerateAc) {}

    std::vector<ApplicationCandidate> buildCandidateList(const CardProfile& card) override {
        return card.applications;
    }
    FirstCryptogram processUntilFirstCryptogram(const KernelTransaction& transaction, ITransactionObserver&) override {
        ++firstCryptogramRequests;
        lastTransaction = transaction;
        return {firstOutcome_, "SIMULATED-FIRST", Bytes{0x9F, 0x26, 0x01, 0xAA}};
    }
    TlvList transactionData() const override {
        return {{0x95, Bytes{0x00, 0x00, 0x00, 0x80, 0x00}}};
    }
    OnlineCompletion completeOnlineTransaction(const HostResponse& response) override {
        ++onlineCompletions;
        OnlineCompletion completion;
        if (hasSecondGenerateAc_) {
            completion.finalCryptogram = response.approved
                ? FinalCryptogram{CryptogramType::TC, "SIMULATED-TC"}
                : FinalCryptogram{CryptogramType::AAC, "SIMULATED-AAC"};
            completion.issuerScriptsProcessed = true;
        }
        return completion;
    }

    int firstCryptogramRequests = 0;
    int onlineCompletions = 0;
    KernelTransaction lastTransaction;

private:
    CryptogramType firstOutcome_;
    bool hasSecondGenerateAc_;
};

// Records every stage event as "STAGE:STATUS".
class RecordingObserver final : public ITransactionObserver {
public:
    void onStage(TransactionStage stage, StageStatus status, const std::string& detail) override {
        events.push_back(toString(stage) + ":" + toString(status));
        details.push_back(detail);
    }
    std::string detailOf(const std::string& event) const {
        for (std::size_t i = 0; i < events.size(); ++i) {
            if (events[i] == event) return details[i];
        }
        return {};
    }
    bool saw(const std::string& event) const {
        for (const auto& recorded : events) if (recorded == event) return true;
        return false;
    }

    std::vector<std::string> events;
    std::vector<std::string> details;
};

class UnreachableHost final : public IHostAuthorizer {
public:
    HostResponse authorize(const HostRequest&) override { throw HostUnavailableError("connection refused"); }
};

class FakeHost final : public IHostAuthorizer {
public:
    explicit FakeHost(bool approve = true) : approve_(approve) {}
    HostResponse authorize(const HostRequest& request) override {
        ++calls;
        lastRequest = request;
        return {approve_, approve_ ? "00" : "05", "TEST01", Bytes{0xA1, 0xB2}, approve_ ? "Approved" : "Declined"};
    }

    int calls = 0;
    HostRequest lastRequest;

private:
    bool approve_;
};

} // namespace emv::test
