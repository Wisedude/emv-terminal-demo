#include "emv/tlv.hpp"
#include <stdexcept>
#include <utility>

namespace emv {
namespace {

int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

Bytes tagBytes(Tag tag) {
    Bytes bytes;
    for (int shift = 24; shift >= 0; shift -= 8) {
        const auto byte = static_cast<std::uint8_t>((tag >> shift) & 0xFF);
        if (!bytes.empty() || byte != 0 || shift == 0) bytes.push_back(byte);
    }
    return bytes;
}

void appendLength(Bytes& out, std::size_t length) {
    if (length < 0x80) {
        out.push_back(static_cast<std::uint8_t>(length));
    } else if (length <= 0xFF) {
        out.push_back(0x81);
        out.push_back(static_cast<std::uint8_t>(length));
    } else if (length <= 0xFFFF) {
        out.push_back(0x82);
        out.push_back(static_cast<std::uint8_t>(length >> 8));
        out.push_back(static_cast<std::uint8_t>(length & 0xFF));
    } else {
        throw std::invalid_argument("TLV value too long");
    }
}

} // namespace

Bytes hexToBytes(std::string_view hex) {
    Bytes bytes;
    int high = -1;
    for (const char c : hex) {
        if (c == ' ' || c == '\t') continue;
        const int digit = hexDigit(c);
        if (digit < 0) throw std::invalid_argument(std::string("Invalid hex character '") + c + "'");
        if (high < 0) {
            high = digit;
        } else {
            bytes.push_back(static_cast<std::uint8_t>((high << 4) | digit));
            high = -1;
        }
    }
    if (high >= 0) throw std::invalid_argument("Hex value has an odd number of digits");
    return bytes;
}

std::string toHex(const Bytes& bytes) {
    static constexpr char digits[] = "0123456789ABCDEF";
    std::string text;
    text.reserve(bytes.size() * 2);
    for (const auto byte : bytes) {
        text.push_back(digits[byte >> 4]);
        text.push_back(digits[byte & 0x0F]);
    }
    return text;
}

std::string tagToHex(Tag tag) {
    return toHex(tagBytes(tag));
}

Tag tagFromHex(std::string_view hex) {
    const Bytes bytes = hexToBytes(hex);
    if (bytes.empty() || bytes.size() > 4) throw std::invalid_argument("A tag is 1 to 4 bytes");
    Tag tag = 0;
    for (const auto byte : bytes) tag = (tag << 8) | byte;
    return tag;
}

Bytes encodeTlv(Tag tag, const Bytes& value) {
    Bytes out = tagBytes(tag);
    appendLength(out, value.size());
    out.insert(out.end(), value.begin(), value.end());
    return out;
}

Bytes encodeTlv(const TlvList& fields) {
    Bytes out;
    for (const auto& field : fields) {
        const Bytes encoded = encodeTlv(field.tag, field.value);
        out.insert(out.end(), encoded.begin(), encoded.end());
    }
    return out;
}

TlvList parseTlv(const Bytes& data) {
    TlvList fields;
    std::size_t pos = 0;
    const auto need = [&](std::size_t count) {
        if (pos + count > data.size()) throw std::invalid_argument("Truncated TLV data");
    };
    while (pos < data.size()) {
        if (data[pos] == 0x00 || data[pos] == 0xFF) {  // inter-object padding
            ++pos;
            continue;
        }
        Tag tag = data[pos++];
        if ((tag & 0x1F) == 0x1F) {
            int extraBytes = 0;
            do {
                need(1);
                if (++extraBytes > 3) throw std::invalid_argument("TLV tag longer than 4 bytes");
                tag = (tag << 8) | data[pos];
            } while (data[pos++] & 0x80);
        }
        need(1);
        std::size_t length = data[pos++];
        if (length & 0x80) {
            const std::size_t lengthBytes = length & 0x7F;
            if (lengthBytes == 0 || lengthBytes > 2) throw std::invalid_argument("Unsupported TLV length form");
            need(lengthBytes);
            length = 0;
            for (std::size_t i = 0; i < lengthBytes; ++i) length = (length << 8) | data[pos++];
        }
        need(length);
        const auto begin = data.begin() + static_cast<std::ptrdiff_t>(pos);
        fields.push_back({tag, Bytes(begin, begin + static_cast<std::ptrdiff_t>(length))});
        pos += length;
    }
    return fields;
}

void TlvStore::set(Tag tag, Bytes value) {
    for (auto& field : fields_) {
        if (field.tag == tag) {
            field.value = std::move(value);
            return;
        }
    }
    fields_.push_back({tag, std::move(value)});
}

void TlvStore::merge(const TlvStore& overrides) {
    for (const auto& field : overrides.fields_) set(field.tag, field.value);
}

void TlvStore::clear() {
    fields_.clear();
}

bool TlvStore::contains(Tag tag) const {
    return find(tag) != nullptr;
}

const Bytes* TlvStore::find(Tag tag) const {
    for (const auto& field : fields_) {
        if (field.tag == tag) return &field.value;
    }
    return nullptr;
}

const Bytes& TlvStore::at(Tag tag) const {
    const Bytes* value = find(tag);
    if (!value) throw std::out_of_range("Tag " + tagToHex(tag) + " is not present");
    return *value;
}

Bytes TlvStore::valueOr(Tag tag, Bytes fallback) const {
    const Bytes* value = find(tag);
    return value ? *value : std::move(fallback);
}

} // namespace emv
