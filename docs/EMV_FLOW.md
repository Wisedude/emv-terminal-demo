# EMV flow and interface assumptions

## 13-step callback and orchestration map

| Stage | Application responsibility | Kernel responsibility / boundary |
|---|---|---|
| 1. Application selection | Expose candidate-list and selection callbacks; apply configured selection policy | Enforce EMV application selection rules |
| 2. Initiate application processing | Start the selected application through the kernel API | Initialize processing and return the processing options |
| 3. Read application data | Supply requested records to the kernel | Read and interpret application records |
| 4. Processing restrictions | Provide transaction context | Check application version, effective/expiration dates, and usage controls |
| 5. Offline data authentication | Invoke the configured ODA operation | Perform SDA/DDA/CDA as configured |
| 6. Certificates | Keep certificate parsing and trust data inside the approved kernel boundary | Validate relevant CA, issuer, and ICC certificate chain as part of ODA |
| 7. Cardholder verification | Provide secure CVM integration where required | Select and perform the configured CVM |
| 8. Terminal risk management | Provide terminal parameters and transaction context | Apply floor-limit, random-selection, and velocity checks as configured |
| 9. Terminal action analysis | Supply configured action codes | Compare action codes with TVR and determine the requested action |
| 10. First card action analysis | Request first GENERATE AC and inspect its outcome | Return AAC, TC, or ARQC; ARQC is the online-required outcome in this demo |
| 11. Online transaction authorization | Send tokenized request only when required; ATM policy may require online authorization | Return issuer response and issuer-authentication data |
| 12. Second card action analysis | Pass issuer response to the kernel and report the final result | Perform issuer authentication and second GENERATE AC/final action analysis |
| 13. Issuer script processing | Pass scripts opaquely to the kernel; do not interpret or execute them in application code | Apply scripts at the scheme/kernel-required point(s), which can be before and/or after second GENERATE AC |

The list separates certificate checks from ODA for visibility; certificate validation is an internal part of ODA, not an independent EMV cryptographic phase. Likewise, issuer script processing can straddle the second GENERATE AC. `IEmvKernel::completeOnlineTransaction` deliberately keeps issuer authentication, script timing, and final action analysis inside the vendor-kernel adapter rather than promising an incorrect universal ordering.

Online authorization is conditional: `TransactionService` calls the host only when the first GENERATE AC returns ARQC. TC/AAC complete or decline offline and skip online authorization, issuer response handling, second GENERATE AC, and issuer scripts. ATM-specific mandatory-online policy is not modeled by this POS sale demo; a deployed ATM integration must enforce its applicable terminal and scheme rules.

## Stub assumptions

- `ICardReader` presents one of five simulated test cards (Visa, Mastercard, expired Visa, Mastercard pointing at a rejected CA key, JCB with an unsupported AID). Each card's chip data (AIP, PAN, dates, CVM list, IACs, CA key index, counters) lives in the simulated adapter; only a test token leaves the terminal.
- The simulated kernels are loaded from `cpp/config` (terminal data, contact and contactless AIDs, CA public keys) and set TVR, TSI and CVM Results as each EMV step runs. The PAN never leaves the kernel: `transactionData()` and DE 55 exclude it.
- `IEmvKernel` is deliberately coarse (candidate list → process to first cryptogram → online completion) so any kernel shape fits behind it. Stages 2–10 are expressed by `PhasedEmvKernel`, a base class for SDKs that expose each phase; `SimulatedContactKernel` uses it, while `SimulatedContactlessKernel` implements `IEmvKernel` directly and has no second GENERATE AC. Stub outcomes are fixed simulation values; cryptogram strings are labels only, not cryptographic values.
- Whether a sale goes online is decided by terminal action analysis (TVR against the TACs and IACs) and the card's answer, not by a switch. The UI's test-only card behaviour selector can still make the simulated card answer ARQC instead of TC, or AAC. Cryptograms, certificate recovery and the ARPC are mocked; the decision logic follows EMV Book 3.
- `IHostAuthorizer` is the host boundary. The terminal agent uses `HttpHostAuthorizer` to call the Spring Boot JSON endpoint; the `emv_demo` CLI uses a deterministic local stub.
- `issuerAuthenticationData` is a deliberately obvious placeholder. In real EMV processing, ARPC/issuer authentication data must be produced by the issuer/HSM using the appropriate keys and inputs and passed through the approved kernel API. Do not construct it in the POS application.
- Application selection policy lives in `IApplicationSelector`, not in the kernel. The sample's `PreferredAidSelector` chooses Visa automatically if present. Real application selection must respect kernel configuration, supported AIDs, cardholder selection requirements and the exact vendor callback contract.
- A real online failure path cannot be decided generically here: follow the scheme, terminal configuration, kernel result and acquirer host contract, and ensure uncertain outcomes can be recovered/reversed safely.

## Suggested next adapter

Create `VendorEmvKernelAdapter` in its own adapter library — deriving from `PhasedEmvKernel` if the SDK exposes individual phases, otherwise implementing `IEmvKernel` directly — and translate the vendor SDK's callback/event objects into these domain types. Register it in `buildKernelRegistry()` (`cpp/app/common/kernel_catalog.cpp`) so the agent and CLI can select it by name. Keep vendor SDK classes out of `TransactionService` and `emv_core`. `HttpHostAuthorizer` (`cpp/adapters/http`) already calls the mock API over plain HTTP with timeouts, response validation and an explicit `HostUnavailableError` for unknown outcomes; production still needs TLS/mTLS, authentication, idempotent retries and reversals. On a physical terminal, expose native functionality to the UI through the vendor-supported bridge (for example JNI/NDK, a local IPC API, or a platform-specific SDK); browser React code cannot directly access secure reader hardware.
