package com.example.emvauth;

import org.springframework.stereotype.Service;

@Service
public class AuthorizationService {
    public AuthorizationResponse authorize(AuthorizationRequest request) {
        // Deterministic training rule: amount <= R 10,000.00 and ARQC => approved.
        // This is deliberately NOT issuer risk logic and must never be used in production.
        boolean approved = "ARQC".equals(request.cryptogramType()) && request.amountMinor() <= 1_000_000;
        return new AuthorizationResponse(
                request.transactionId(), approved,
                approved ? "00" : "05",
                approved ? "D3M042" : null,
                approved ? "DEMO-ARPC-9F10-00000000" : null,
                approved ? "Approved by mock issuer" : "Declined by mock issuer rule");
    }
}
