#pragma once
#include "emv/domain.hpp"
#include <string>

namespace emv {

// Receives stage progress from the transaction service and the kernel, e.g. to drive a
// terminal display or stream progress to a UI. Implementations must not throw.
class ITransactionObserver {
public:
    virtual ~ITransactionObserver() = default;
    // detail is empty when there is nothing beyond the stage itself to report.
    virtual void onStage(TransactionStage stage, StageStatus status, const std::string& detail) = 0;
};

class NullTransactionObserver final : public ITransactionObserver {
public:
    void onStage(TransactionStage, StageStatus, const std::string&) override {}
};

} // namespace emv
