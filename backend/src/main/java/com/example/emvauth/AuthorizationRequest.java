package com.example.emvauth;

import jakarta.validation.constraints.Min;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.Pattern;

public record AuthorizationRequest(
        @Min(1) long amountMinor,
        @NotBlank @Pattern(regexp = "[A-Z]{3}") String currency,
        @NotBlank @Pattern(regexp = "[0-9A-F]{10,32}") String aid,
        @NotBlank String transactionId,
        @NotBlank @Pattern(regexp = "ARQC|AAC|TC") String cryptogramType,
        @NotBlank @Pattern(regexp = "SIMULATED-[A-Z0-9-]+", message = "only simulated card tokens are accepted") String panToken,
        // ICC data (ISO 8583 DE 55) built by the terminal's EMV kernel, as BER-TLV hex.
        @NotBlank @Pattern(regexp = "([0-9A-Fa-f]{2})+", message = "must be hex") String iccData) { }
