#pragma once
#include "emv/domain.hpp"
#include "emv/ports/application_selector.hpp"
#include "emv/ports/card_reader.hpp"
#include "emv/ports/emv_kernel.hpp"
#include "emv/ports/host_authorizer.hpp"
#include <cstdint>
#include <string>

namespace emv {

// Orchestrates one sale across the ports. Depends only on abstractions, so any reader,
// kernel, selection policy or host transport can be substituted without changes here.
class TransactionService {
public:
    TransactionService(TerminalConfig config,
                       ICardReader& reader,
                       IEmvKernel& kernel,
                       IApplicationSelector& selector,
                       IHostAuthorizer& host);

    TransactionResult runSale(EntryMode mode, std::int64_t amountMinor, const std::string& transactionId);

private:
    ApplicationCandidate selectApplication(const CardProfile& card);
    TransactionResult authorizeOnline(const CardProfile& card, const KernelTransaction& transaction,
                                      const FirstCryptogram& first);

    TerminalConfig config_;
    ICardReader& reader_;
    IEmvKernel& kernel_;
    IApplicationSelector& selector_;
    IHostAuthorizer& host_;
};

} // namespace emv
