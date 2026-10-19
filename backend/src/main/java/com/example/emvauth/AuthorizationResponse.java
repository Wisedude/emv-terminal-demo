package com.example.emvauth;

public record AuthorizationResponse(
        String transactionId,
        boolean approved,
        String responseCode,
        String authorizationCode,
        String issuerAuthenticationData,
        String message) { }
