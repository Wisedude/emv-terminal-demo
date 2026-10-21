#include "terminal_agent/agent_protocol.hpp"
#include "test_support.hpp"

namespace {

using namespace emv;
using namespace emv::agent;
using namespace emv::test;

void parsesValidCommand() {
    const auto command = parseSaleCommand(
        R"({"amountMinor":12500,"entryMode":"Tap","transactionId":"SIM-1","kernel":"simulated-contactless",)"
        R"("simulatedCard":"MASTERCARD","simulatedCardBehaviour":"DECLINE"})");

    require(command.amountMinor == 12500, "amount must be parsed");
    require(command.entryMode == EntryMode::Tap, "entry mode must be parsed");
    require(command.transactionId == "SIM-1", "transaction ID must be parsed");
    require(command.kernel == std::optional<std::string>("simulated-contactless"), "kernel must be parsed");
    require(command.simulation.card == simulated::SimulatedCardId::Mastercard, "test card must be parsed");
    require(command.simulation.cardBehaviour == simulated::CardBehaviour::Decline, "card behaviour must be parsed");
}

void appliesDefaultsForOptionalFields() {
    const auto command = parseSaleCommand(R"({"amountMinor":1,"entryMode":"Insert","transactionId":"T"})");
    require(!command.kernel.has_value(), "kernel defaults to none (chosen by entry mode)");
    require(command.simulation.card == simulated::SimulatedCardId::Visa, "test card defaults to Visa");
    require(command.simulation.cardBehaviour == simulated::CardBehaviour::Normal, "card behaviour defaults to normal");
}

void rejectsInvalidCommands() {
    const std::vector<std::string> invalid{
        "not json",
        "[]",
        R"({"entryMode":"Insert","transactionId":"T"})",
        R"({"amountMinor":0,"entryMode":"Insert","transactionId":"T"})",
        R"({"amountMinor":12.5,"entryMode":"Insert","transactionId":"T"})",
        R"({"amountMinor":1,"entryMode":"Wave","transactionId":"T"})",
        R"({"amountMinor":1,"entryMode":"Insert","transactionId":""})",
        R"({"amountMinor":1,"entryMode":"Insert","transactionId":"T","simulatedCard":"AMEX_GOLD"})",
        R"({"amountMinor":1,"entryMode":"Insert","transactionId":"T","simulatedCardBehaviour":"MAYBE"})",
    };
    for (const auto& body : invalid) {
        requireThrows<std::invalid_argument>([&] { parseSaleCommand(body); }, "must reject: " + body);
    }
}

void serializesOnlineResult() {
    SaleCommand command;
    command.transactionId = "SIM-1";
    command.amountMinor = 12500;
    TransactionResult result;
    result.approved = true;
    result.selectedAid = "A0000000031010";
    result.first = {CryptogramType::ARQC, "ARQC-VALUE"};
    result.final = FinalCryptogram{CryptogramType::TC, "TC-VALUE"};
    result.host = HostResponse{true, "00", "D3M042", std::nullopt, "Approved"};

    const auto event = resultEvent(command, "simulated-contact", result);
    const auto& body = event.at("result");
    require(event.at("type") == "result", "event type must be result");
    require(body.at("kernel") == "simulated-contact", "kernel name must be included");
    require(body.at("firstCryptogram").at("type") == "ARQC", "first cryptogram must be included");
    require(body.at("finalCryptogram").at("type") == "TC", "final cryptogram must be included");
    require(body.at("host").at("responseCode") == "00", "host response must be included");
    require(body.at("host").at("issuerAuthenticationData").is_null(), "absent issuer data must be null");
}

void serializesOfflineResultWithNulls() {
    TransactionResult result;
    result.first = {CryptogramType::AAC, "AAC-VALUE"};
    const auto body = resultEvent(SaleCommand{}, "k", result).at("result");
    require(body.at("finalCryptogram").is_null() && body.at("host").is_null(), "offline result must have null online fields");
}

void serializesEmvData() {
    TransactionResult result;
    result.host = HostResponse{true, "00", "A1", Bytes{0x01, 0x02}, "Approved", Bytes{0x72, 0x00}};
    result.first.iccData = Bytes{0x9F, 0x26, 0x01, 0xAA};
    result.emvData = {{0x95, Bytes{0x00, 0x00, 0x04, 0x80, 0x00}},
                      {0x9B, Bytes{0xE8, 0x00}},
                      {0x9F34, Bytes{0x42, 0x03, 0x00}}};

    const auto body = resultEvent(SaleCommand{}, "k", result).at("result");
    require(body.at("onlineIccData") == "9F2601AA", "DE 55 sent to the host must be included as hex");
    require(body.at("host").at("issuerAuthenticationData") == "0102", "issuer authentication data must be hex");
    require(body.at("host").at("issuerScripts") == "7200", "issuer scripts must be hex");

    const auto& emv = body.at("emv");
    require(emv.at("tvr").at("hex") == "0000048000", "TVR hex must be included");
    require((emv.at("tvr").at("flags") == nlohmann::json{"Online PIN entered", "Transaction exceeds floor limit"}),
            "TVR bits must be decoded");
    require(emv.at("tsi").at("flags").size() == 4, "TSI bits must be decoded");
    require(emv.at("cvmResults").at("description").get<std::string>().rfind("Enciphered PIN verified online", 0) == 0,
            "CVM Results must be described");
    require(emv.at("tags")[0].at("name") == "Terminal Verification Results", "tags must carry their names");
}

void writesOneEventPerLine() {
    const std::string line = toLine(stageEvent(TransactionStage::Certificates, StageStatus::Skipped, "multi\nline"));
    require(line.back() == '\n', "event must end with a newline");
    require(line.find('\n') == line.size() - 1, "event must not contain raw newlines");
}

} // namespace

int main() {
    return runTests("AgentProtocol", {
        {"parsesValidCommand", parsesValidCommand},
        {"appliesDefaultsForOptionalFields", appliesDefaultsForOptionalFields},
        {"rejectsInvalidCommands", rejectsInvalidCommands},
        {"serializesOnlineResult", serializesOnlineResult},
        {"serializesOfflineResultWithNulls", serializesOfflineResultWithNulls},
        {"serializesEmvData", serializesEmvData},
        {"writesOneEventPerLine", writesOneEventPerLine},
    });
}
