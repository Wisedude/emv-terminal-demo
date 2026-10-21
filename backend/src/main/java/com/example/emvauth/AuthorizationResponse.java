package com.example.emvauth;

import java.util.List;

public record AuthorizationResponse(
        String transactionId,
        boolean approved,
        String responseCode,
        String authorizationCode,
        // Tag 91 value as hex: ARPC (8 bytes) + ARC (2 bytes). Demo formula, not a real issuer MAC.
        String issuerAuthenticationData,
        String message,
        // Tag 71/72 issuer script templates as hex, or null.
        String issuerScripts,
        // The DE 55 tags the issuer received, to show the request was an EMV chip transaction.
        List<String> receivedEmvTags) { }
