package com.example.emvauth;

import org.springframework.stereotype.Service;

import java.nio.charset.StandardCharsets;
import java.util.HexFormat;
import java.util.List;
import java.util.Map;

@Service
public class AuthorizationService {
    // Tags an issuer needs to validate an ARQC (EMV Book 4 / scheme minimum DE 55 data).
    static final List<String> REQUIRED_TAGS = List.of(
            "9F26", "9F27", "9F10", "9F37", "9F36", "95", "9A", "9C", "9F02", "5F2A", "82", "9F1A");
    static final long DEMO_LIMIT_MINOR = 1_000_000;
    // One PUT DATA command (new lower consecutive offline limit, mock MAC) in an issuer script template 72.
    static final String DEMO_ISSUER_SCRIPT = "72139F180400000001860A04DA9F58050A11223344";

    private static final HexFormat HEX = HexFormat.of().withUpperCase();

    public AuthorizationResponse authorize(AuthorizationRequest request) {
        Map<String, byte[]> icc = BerTlv.parse(HEX.parseHex(request.iccData()));
        for (String tag : REQUIRED_TAGS) {
            if (!icc.containsKey(tag)) throw new InvalidIccDataException("ICC data is missing tag " + tag);
        }
        if (bcd(icc.get("9F02")) != request.amountMinor()) {
            throw new InvalidIccDataException("ICC data amount (9F02) does not match amountMinor");
        }
        boolean arqc = (icc.get("9F27")[0] & 0xC0) == 0x80;
        if (arqc != "ARQC".equals(request.cryptogramType())) {
            throw new InvalidIccDataException("Cryptogram Information Data (9F27) does not match cryptogramType");
        }

        // Deterministic training rules - deliberately NOT issuer risk logic, never use in production.
        boolean expired = (icc.get("95")[1] & 0x40) != 0;  // TVR: expired application
        String responseCode = !arqc ? "05" : expired ? "54" : request.amountMinor() > DEMO_LIMIT_MINOR ? "05" : "00";
        boolean approved = responseCode.equals("00");
        String message = switch (responseCode) {
            case "00" -> "Approved by mock issuer";
            case "54" -> "Declined by mock issuer: expired card";
            default -> "Declined by mock issuer rule";
        };

        return new AuthorizationResponse(
                request.transactionId(), approved, responseCode,
                approved ? "D3M042" : null,
                HEX.formatHex(issuerAuthenticationData(icc.get("9F26"), responseCode)),
                message,
                approved ? DEMO_ISSUER_SCRIPT : null,
                List.copyOf(icc.keySet()));
    }

    /**
     * Tag 91: ARPC || ARC. The ARPC has the shape of EMV ARPC method 1 without the issuer key:
     * ARQC XOR (ARC || 00 00 00 00 00 00). The C++ simulated kernel checks the same formula.
     */
    static byte[] issuerAuthenticationData(byte[] arqc, String responseCode) {
        byte[] arc = responseCode.getBytes(StandardCharsets.US_ASCII);
        byte[] data = new byte[10];
        System.arraycopy(arqc, 0, data, 0, Math.min(8, arqc.length));
        data[0] ^= arc[0];
        data[1] ^= arc[1];
        data[8] = arc[0];
        data[9] = arc[1];
        return data;
    }

    private static long bcd(byte[] value) {
        long result = 0;
        for (byte b : value) result = result * 100 + ((b >> 4) & 0x0F) * 10 + (b & 0x0F);
        return result;
    }
}
