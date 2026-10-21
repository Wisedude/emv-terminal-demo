#pragma once
#include "emv/tlv.hpp"
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Terminal parameter file format shared by TERMINAL.TXT, AIDS_*.TXT and CAKEYS.TXT:
//
//   # comment
//   9F06= 07 A0 00 00 00 03 10 10;   # trailing comment
//   9F1A= 07 10;
//                                     <- a blank line ends the record
//   9F06= 07 A0 00 00 00 04 10 10;
//
// Every statement is TAG=VALUE; with the tag and value in hex (spaces allowed in the value).
namespace emv::config {

class ConfigError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct ConfigRecord {
    TlvStore tags;
    int firstLine{0};
    std::optional<std::string> error;  // set when a statement in the record could not be parsed
};

// Never throws: a malformed statement marks its record with an error (file and line included).
std::vector<ConfigRecord> parseConfigRecords(std::string_view text, const std::string& sourceName);

} // namespace emv::config
