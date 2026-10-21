#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Byte strings, hex text and BER-TLV (EMV Book 3, Annex B).
namespace emv {

using Bytes = std::vector<std::uint8_t>;
using Tag = std::uint32_t;  // up to four tag bytes, e.g. 0x9F26 or 0x9F918704

struct TlvField {
    Tag tag{0};
    Bytes value;
};
using TlvList = std::vector<TlvField>;

// Spaces are ignored; any other non-hex character or an odd digit count throws std::invalid_argument.
Bytes hexToBytes(std::string_view hex);
std::string toHex(const Bytes& bytes);
std::string tagToHex(Tag tag);
Tag tagFromHex(std::string_view hex);

Bytes encodeTlv(Tag tag, const Bytes& value);
Bytes encodeTlv(const TlvList& fields);

// Decodes consecutive top-level objects (constructed values are returned undecoded), skipping 00/FF
// padding between them. Throws std::invalid_argument on truncated or malformed input.
TlvList parseTlv(const Bytes& data);

// Tag -> value store that keeps insertion order; set() replaces an existing value in place.
class TlvStore {
public:
    void set(Tag tag, Bytes value);
    void merge(const TlvStore& overrides);
    void clear();

    bool contains(Tag tag) const;
    const Bytes* find(Tag tag) const;
    const Bytes& at(Tag tag) const;  // throws std::out_of_range naming the tag
    Bytes valueOr(Tag tag, Bytes fallback) const;

    const TlvList& fields() const { return fields_; }
    bool empty() const { return fields_.empty(); }

private:
    TlvList fields_;
};

} // namespace emv
