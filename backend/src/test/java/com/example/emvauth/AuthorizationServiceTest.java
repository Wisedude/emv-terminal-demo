package com.example.emvauth;

import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

class AuthorizationServiceTest {
    private final AuthorizationService service = new AuthorizationService();

    @Test void approvesArqcBelowDemoLimit() {
        var response = service.authorize(new AuthorizationRequest(12500, "ZAR", "A0000000031010", "SIM-1", "ARQC", "SIMULATED-CARD-01"));
        assertTrue(response.approved());
        assertEquals("00", response.responseCode());
        assertNotNull(response.issuerAuthenticationData());
    }

    @Test void declinesOverDemoLimit() {
        var response = service.authorize(new AuthorizationRequest(1000001, "ZAR", "A0000000031010", "SIM-2", "ARQC", "SIMULATED-CARD-01"));
        assertFalse(response.approved());
        assertEquals("05", response.responseCode());
    }
}
