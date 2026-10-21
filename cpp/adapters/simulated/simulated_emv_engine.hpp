#pragma once
#include "emv/config/emv_configuration.hpp"
#include "emv/domain.hpp"
#include "emv/emv_tags.hpp"
#include "simulated/simulated_cards.hpp"
#include "simulated/simulation_options.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace emv::simulated {

enum class CardInterface { Contact, Contactless };

// EMV processing shared by the simulated contact and contactless kernels. It loads the terminal and AID
// configuration, plays the card's side from a simulated card, keeps the transaction's TLV data and sets
// TVR, TSI and CVM Results bits as each step runs, following the decision rules of EMV Book 3.
// Only the cryptography is mocked (mock_issuer_crypto.hpp). Each step returns a short detail for display.
class SimulatedEmvEngine {
public:
    SimulatedEmvEngine(std::shared_ptr<const config::EmvConfiguration> configuration, CardInterface cardInterface,
                       CardBehaviour cardBehaviour);

    // Candidate list = card applications whose AID the terminal supports on this interface.
    std::vector<ApplicationCandidate> buildCandidateList(const CardProfile& card);

    std::string initiateApplicationProcessing(const KernelTransaction& transaction);
    std::string readApplicationData();
    std::string checkProcessingRestrictions();
    std::string selectOfflineDataAuthentication();
    std::string authenticateOfflineData();
    std::string verifyCardholder();
    std::string manageTerminalRisk();
    std::string analyseTerminalActions();
    FirstCryptogram generateFirstAc();

    OnlineCompletion completeOnlineTransaction(const HostResponse& response);
    TlvList transactionData() const;

private:
    enum class OdaMethod { None, Sda, Dda, Cda };

    const std::vector<config::AidConfig>& supportedAids() const;
    void setTvr(const BitFlag& flag);
    void setTsi(const BitFlag& flag);
    std::uint8_t aip() const;
    std::string odaName() const;
    void enforceContactlessLimit() const;

    std::string verifyCardholderContact();
    std::string verifyCardholderContactless();
    bool terminalSupportsCvm(std::uint8_t cvmCode) const;
    bool cvmConditionApplies(std::uint8_t condition, std::uint8_t cvmCode, std::uint64_t x, std::uint64_t y) const;
    std::string performCvm(std::uint8_t rule, std::uint8_t condition, bool& failed);

    std::string authenticateIssuer(const HostResponse& response);
    void runIssuerScripts(const TlvList& scripts, Tag templateTag, Bytes& results, std::vector<std::string>& notes);
    Bytes setCryptogram(CryptogramType type);

    std::shared_ptr<const config::EmvConfiguration> configuration_;
    CardInterface interface_;
    CardBehaviour cardBehaviour_;
    std::string log_;

    const SimulatedCard* card_{nullptr};
    const SimulatedCardApplication* cardApplication_{nullptr};
    const config::AidConfig* aidConfig_{nullptr};
    TlvStore data_;
    std::int64_t amountMinor_{0};
    OdaMethod odaMethod_{OdaMethod::None};
    CryptogramType requestedCryptogram_{CryptogramType::AAC};
};

} // namespace emv::simulated
