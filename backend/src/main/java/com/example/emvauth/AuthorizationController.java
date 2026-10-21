package com.example.emvauth;

import jakarta.validation.Valid;
import org.springframework.http.HttpStatus;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.server.ResponseStatusException;

import java.util.Set;

@RestController
@RequestMapping("/api")
public class AuthorizationController {
    // Registered application provider IDs this demo acquirer accepts: Visa, Mastercard, Amex, Discover.
    private static final Set<String> SUPPORTED_RIDS = Set.of("A000000003", "A000000004", "A000000025", "A000000152");

    private final AuthorizationService authorizationService;
    public AuthorizationController(AuthorizationService authorizationService) { this.authorizationService = authorizationService; }

    @GetMapping("/health")
    public HealthResponse health() { return new HealthResponse("UP", "MOCK_ONLY"); }

    @PostMapping("/authorize")
    public AuthorizationResponse authorize(@Valid @RequestBody AuthorizationRequest request) {
        if (!SUPPORTED_RIDS.contains(request.aid().substring(0, 10))) {
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "AID is not from a scheme this acquirer supports");
        }
        return authorizationService.authorize(request);
    }

    public record HealthResponse(String status, String mode) { }
}
