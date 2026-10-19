package com.example.emvauth;

import jakarta.validation.constraints.Min;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.Pattern;

public record AuthorizationRequest(
        @Min(1) long amountMinor,
        @NotBlank @Pattern(regexp = "[A-Z]{3}") String currency,
        @NotBlank String aid,
        @NotBlank String transactionId,
        @NotBlank @Pattern(regexp = "ARQC|AAC|TC") String cryptogramType,
        @NotBlank String panToken) { }
