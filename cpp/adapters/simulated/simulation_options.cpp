#include "simulated/simulation_options.hpp"
#include <stdexcept>

namespace emv::simulated {

std::vector<SimulatedCardId> allSimulatedCards() {
    return {SimulatedCardId::Visa, SimulatedCardId::Mastercard, SimulatedCardId::VisaExpired,
            SimulatedCardId::MastercardUnknownCaKey, SimulatedCardId::JcbUnsupported};
}

std::string toString(SimulatedCardId card) {
    switch (card) {
        case SimulatedCardId::Visa: return "VISA";
        case SimulatedCardId::Mastercard: return "MASTERCARD";
        case SimulatedCardId::VisaExpired: return "VISA_EXPIRED";
        case SimulatedCardId::MastercardUnknownCaKey: return "MASTERCARD_UNKNOWN_CA_KEY";
        case SimulatedCardId::JcbUnsupported: return "JCB_UNSUPPORTED";
    }
    throw std::logic_error("Unknown simulated card");
}

std::string toString(CardBehaviour behaviour) {
    switch (behaviour) {
        case CardBehaviour::Normal: return "NORMAL";
        case CardBehaviour::ForceOnline: return "FORCE_ONLINE";
        case CardBehaviour::Decline: return "DECLINE";
    }
    throw std::logic_error("Unknown card behaviour");
}

std::optional<SimulatedCardId> simulatedCardFromString(const std::string& text) {
    for (const auto card : allSimulatedCards()) {
        if (toString(card) == text) return card;
    }
    return std::nullopt;
}

std::optional<CardBehaviour> cardBehaviourFromString(const std::string& text) {
    for (const auto behaviour : {CardBehaviour::Normal, CardBehaviour::ForceOnline, CardBehaviour::Decline}) {
        if (toString(behaviour) == text) return behaviour;
    }
    return std::nullopt;
}

} // namespace emv::simulated
