package com.example.emvauth;

import org.junit.jupiter.api.Test;

import java.util.HexFormat;

import static org.junit.jupiter.api.Assertions.*;

class AuthorizationServiceTest {
    private final AuthorizationService service = new AuthorizationService();

    // DE 55 as the C++ kernel builds it: ARQC 1122334455667788, amount R125.00, clean TVR.
    private static String iccData(String amountBcd, String cid, String tvr) {
        return "9F2608112233445566778" + "8" + "9F2701" + cid + "9F100706010A03A00000" + "9F37040BFA73D8"
                + "9F36020015" + "9505" + tvr + "9A03261020" + "9C0100" + "9F0206" + amountBcd
                + "5F2A020710" + "82023C00" + "9F1A020710";
    }

    private static AuthorizationRequest request(long amount, String iccData) {
        return new AuthorizationRequest(amount, "ZAR", "A0000000031010", "SIM-1", "ARQC", "SIMULATED-VISA-01", iccData);
    }

    @Test void approvesArqcBelowDemoLimitWithArpcAndScript() {
        var response = service.authorize(request(12500, iccData("000000012500", "80", "0000048000")));
        assertTrue(response.approved());
        assertEquals("00", response.responseCode());
        // ARPC = ARQC XOR ("00" || 000000000000), then the ARC "00" = 3030
        assertEquals("2112334455667788" + "3030", response.issuerAuthenticationData());
        assertEquals(AuthorizationService.DEMO_ISSUER_SCRIPT, response.issuerScripts());
        assertTrue(response.receivedEmvTags().containsAll(AuthorizationService.REQUIRED_TAGS));
    }

    @Test void declinesOverDemoLimitButStillSendsArpc() {
        var response = service.authorize(request(1000001, iccData("000001000001", "80", "0000000000")));
        assertFalse(response.approved());
        assertEquals("05", response.responseCode());
        assertTrue(response.issuerAuthenticationData().endsWith("3035"));
        assertNull(response.issuerScripts());
    }

    @Test void declinesExpiredCardFromTvr() {
        var response = service.authorize(request(12500, iccData("000000012500", "80", "0040000000")));
        assertEquals("54", response.responseCode());
    }

    @Test void rejectsIccDataWithoutRequiredTags() {
        assertThrows(InvalidIccDataException.class, () -> service.authorize(request(12500, "9F260811223344556677889F270180")));
    }

    @Test void rejectsAmountMismatchBetweenRequestAndIccData() {
        assertThrows(InvalidIccDataException.class,
                () -> service.authorize(request(99, iccData("000000012500", "80", "0000000000"))));
    }

    @Test void rejectsMalformedTlv() {
        assertThrows(InvalidIccDataException.class, () -> BerTlv.parse(HexFormat.of().parseHex("9F2608AABB")));
    }
}
