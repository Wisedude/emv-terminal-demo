package com.example.emvauth;

import java.util.HexFormat;
import java.util.LinkedHashMap;
import java.util.Map;

/** Minimal BER-TLV decoder for the ICC data (DE 55) an EMV terminal sends with an authorisation. */
final class BerTlv {
    private BerTlv() { }

    /** Top-level tags (upper-case hex) to their values, in order. Throws InvalidIccDataException if malformed. */
    static Map<String, byte[]> parse(byte[] data) {
        Map<String, byte[]> fields = new LinkedHashMap<>();
        int pos = 0;
        while (pos < data.length) {
            int first = data[pos] & 0xFF;
            if (first == 0x00 || first == 0xFF) { pos++; continue; }
            int tagStart = pos++;
            if ((first & 0x1F) == 0x1F) {
                do {
                    require(pos < data.length, "truncated tag");
                } while ((data[pos++] & 0x80) != 0);
            }
            String tag = HexFormat.of().withUpperCase().formatHex(data, tagStart, pos);
            require(pos < data.length, "missing length for tag " + tag);
            int length = data[pos++] & 0xFF;
            if (length == 0x81 || length == 0x82) {
                int lengthBytes = length & 0x7F;
                require(pos + lengthBytes <= data.length, "truncated length for tag " + tag);
                length = 0;
                for (int i = 0; i < lengthBytes; i++) length = (length << 8) | (data[pos++] & 0xFF);
            } else {
                require(length < 0x80, "unsupported length form for tag " + tag);
            }
            require(pos + length <= data.length, "truncated value for tag " + tag);
            byte[] value = new byte[length];
            System.arraycopy(data, pos, value, 0, length);
            fields.put(tag, value);
            pos += length;
        }
        return fields;
    }

    private static void require(boolean condition, String message) {
        if (!condition) throw new InvalidIccDataException("Malformed ICC data: " + message);
    }
}
