package com.example.emvauth;

import jakarta.validation.Valid;
import org.springframework.http.HttpStatus;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.server.ResponseStatusException;

@RestController
@RequestMapping("/api")
public class AuthorizationController {
    private final AuthorizationService authorizationService;
    public AuthorizationController(AuthorizationService authorizationService) { this.authorizationService = authorizationService; }

    @GetMapping("/health")
    public HealthResponse health() { return new HealthResponse("UP", "MOCK_ONLY"); }

    @PostMapping("/authorize")
    public AuthorizationResponse authorize(@Valid @RequestBody AuthorizationRequest request) {
        if (!"A0000000031010".equals(request.aid()) && !"A0000000041010".equals(request.aid())) {
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "AID is not in the demo allowlist");
        }
        if (!"SIMULATED-CARD-01".equals(request.panToken())) {
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "Only the simulated card token is accepted");
        }
        return authorizationService.authorize(request);
    }

    public record HealthResponse(String status, String mode) { }
}
