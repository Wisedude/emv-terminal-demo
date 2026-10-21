#include "emv/config/config_file.hpp"
#include <utility>

namespace emv::config {
namespace {

std::string_view trim(std::string_view text) {
    const auto first = text.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) return {};
    const auto last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1);
}

void parseStatement(std::string_view statement, ConfigRecord& record) {
    const auto equals = statement.find('=');
    if (equals == std::string_view::npos) throw std::invalid_argument("expected TAG=VALUE");
    const std::string_view tagText = trim(statement.substr(0, equals));
    const std::string_view valueText = trim(statement.substr(equals + 1));
    Tag tag = 0;
    try {
        tag = tagFromHex(tagText);
    } catch (const std::invalid_argument& error) {
        throw std::invalid_argument("bad tag '" + std::string(tagText) + "': " + error.what());
    }
    try {
        record.tags.set(tag, hexToBytes(valueText));
    } catch (const std::invalid_argument& error) {
        throw std::invalid_argument("bad value for tag " + tagToHex(tag) + ": " + error.what());
    }
}

void parseLine(std::string_view line, ConfigRecord& record) {
    while (!line.empty()) {
        const auto semicolon = line.find(';');
        if (semicolon == std::string_view::npos) throw std::invalid_argument("statement must end with ';'");
        const std::string_view statement = trim(line.substr(0, semicolon));
        if (!statement.empty()) parseStatement(statement, record);
        line = trim(line.substr(semicolon + 1));
    }
}

} // namespace

std::vector<ConfigRecord> parseConfigRecords(std::string_view text, const std::string& sourceName) {
    std::vector<ConfigRecord> records;
    ConfigRecord current;
    bool inRecord = false;
    const auto finishRecord = [&] {
        if (inRecord) records.push_back(std::move(current));
        current = ConfigRecord{};
        inRecord = false;
    };

    int lineNumber = 0;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find('\n', start);
        std::string_view line = text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
        start = end == std::string_view::npos ? text.size() + 1 : end + 1;
        ++lineNumber;

        const auto comment = line.find('#');
        const bool commentOnly = comment != std::string_view::npos && trim(line.substr(0, comment)).empty();
        line = trim(comment == std::string_view::npos ? line : line.substr(0, comment));
        if (line.empty()) {
            if (!commentOnly) finishRecord();  // a blank line ends the record; a comment line does not
            continue;
        }
        if (!inRecord) {
            inRecord = true;
            current.firstLine = lineNumber;
        }
        if (current.error) continue;
        try {
            parseLine(line, current);
        } catch (const std::invalid_argument& error) {
            current.error = sourceName + ":" + std::to_string(lineNumber) + ": " + error.what();
        }
    }
    finishRecord();
    return records;
}

} // namespace emv::config
