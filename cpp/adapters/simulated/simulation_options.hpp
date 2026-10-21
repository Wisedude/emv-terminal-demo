#pragma once
#include <optional>
#include <string>
#include <vector>

namespace emv::simulated {

// The simulated test cards (simulated_cards.cpp). None carries real cardholder data.
enum class SimulatedCardId { Visa, Mastercard, VisaExpired, MastercardUnknownCaKey, JcbUnsupported };

// How the simulated card answers the first GENERATE AC. Normal follows the terminal's request; a card
// may only ever answer with the same or a lower cryptogram (TC > ARQC > AAC).
enum class CardBehaviour { Normal, ForceOnline, Decline };

// Test knobs for the simulated reader and kernels. A real card decides its own behaviour.
struct SimulationOptions {
    SimulatedCardId card{SimulatedCardId::Visa};
    CardBehaviour cardBehaviour{CardBehaviour::Normal};
};

std::string toString(SimulatedCardId card);
std::string toString(CardBehaviour behaviour);
std::optional<SimulatedCardId> simulatedCardFromString(const std::string& text);
std::optional<CardBehaviour> cardBehaviourFromString(const std::string& text);
std::vector<SimulatedCardId> allSimulatedCards();

} // namespace emv::simulated
