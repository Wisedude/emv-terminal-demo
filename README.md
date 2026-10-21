# EMV Terminal Integration Demo (simulation only)

A runnable teaching/demo scaffold aligned to the supplied EMV Kernel Integration Exercise. It contains:

- **React + TypeScript terminal UI** with Tap, Insert, Swipe and Manual Entry buttons that each start a sale, amount input, live step status and result summary.
- **C++17 terminal agent** that runs the EMV flow (application selection, kernel processing, first GENERATE AC, online authorization, issuer response processing, second GENERATE AC) and streams each stage back to the UI.
- **Spring Boot mock authorization API** that the C++ agent calls for online authorization. It returns a deterministic approval plus a *demonstration-only* ARPC field.

> **Important:** This is not a real payment application and does not communicate with physical card readers or an EMV kernel. The C++ kernel/reader implementations and cryptographic outputs are simulations. Do not enter real card data. The returned ARPC is a placeholder string, not a valid issuer-generated EMV ARPC. Never use this scaffold for live payments, PCI data capture, certification, or production decisions.

## Architecture

```text
React UI ──POST /terminal/transactions──► C++ terminal agent ──POST /api/authorize──► Spring Boot mock API
(screen)  ◄──NDJSON stage events + result─ (localhost:9090)    ◄──approve/decline──── (localhost:8080)
                                           reader + kernel +
                                           TransactionService
```

The kernel runs in the terminal agent, not on the server, which matches a real terminal: the UI only displays progress, and the agent talks to the card and the acquirer host. On a physical terminal the agent's local HTTP transport would be replaced by whatever the vendor supports (local IPC, JNI/NDK); the core does not change.

Inside the agent, one sale runs as:

```text
ICardReader -> IEmvKernel candidate list -> IApplicationSelector
  -> IEmvKernel process to first GENERATE AC -> IHostAuthorizer (ARQC only)
  -> IEmvKernel online completion (issuer auth, scripts, second GENERATE AC if any) -> result
```

Every stage is reported through `ITransactionObserver`, which the agent turns into the NDJSON stream the UI renders.

C++ layout (ports and adapters):

```text
cpp/
  config/                   EMV parameter files: TERMINAL.TXT, AIDS_CONTACT.TXT, AIDS_CONTACTLESS.TXT, CAKEYS.TXT
  include/emv/              emv_core public headers (kernel-agnostic, no vendor code)
    domain.hpp                value types: CardProfile, cryptograms, stages, HostRequest/Response, TransactionResult
    tlv.hpp, emv_tags.hpp     BER-TLV, hex, EMV tag names, TVR / TSI / CVM Results bit layouts
    config/                   parameter-file parser and loader (AIDs, CA public keys with SHA-1 check)
    ports/                    ICardReader, IEmvKernel, IApplicationSelector, IHostAuthorizer, ITransactionObserver
    kernel/                   PhasedEmvKernel (base for phase-by-phase SDKs), KernelRegistry
    policy/                   PreferredAidSelector
    transaction_service.hpp
  src/                      emv_core implementation
  adapters/simulated/       emv_simulated: EMV engine shared by the contact + contactless kernels, test cards,
                            mock issuer cryptography, simulated reader, stub host
  adapters/http/            emv_http: HttpHostAuthorizer (calls the Spring Boot API)
  app/common/               kernel catalogue shared by both composition roots
  app/terminal_agent/       emv_terminal_agent: local HTTP API for the UI (agent_protocol.hpp documents the wire format)
  app/cli/                  emv_demo: one sale from the command line, with the stub host
  tests/                    one test executable per unit, sharing fakes in test_support.hpp
```

## Run the full stack

Requirements: Node.js 20+, Java 17+, Maven 3.9+, CMake 3.25+ and a C++17 compiler (see [Build the C++ code](#build-the-c-code)).

Terminal 1, the mock authorization API:
```bash
cd backend
mvn spring-boot:run
```
Health check: `GET http://localhost:8080/api/health`

Terminal 2, the C++ terminal agent (after building it):
```powershell
cd cpp
.\build\Debug\emv_terminal_agent.exe
```
At start-up the agent loads the EMV parameter files from `cpp/config` and prints how many AIDs and CA keys it accepted, plus a warning for every record it rejected. Health check: `GET http://localhost:9090/terminal/health` (includes the configuration summary). Options: `--port 9090`, `--bind 127.0.0.1`, `--host-url http://localhost:8080`, `--currency ZAR`, `--config-dir <folder>`, `--step-delay-ms 250` (pauses at each stage so the UI animation is visible; use `0` for full speed).

Terminal 3, the UI:
```bash
cd frontend
npm install
npm run dev
```
Open the Vite URL printed by the command. Vite's development proxy forwards `/terminal/*` to the agent on `localhost:9090` (and `/api/*` to Spring Boot), avoiding browser CORS restrictions.

Pick a **test card** and an amount, then a card-entry button. **Tap** uses the simulated contactless kernel; the other buttons use the simulated contact kernel. The outcome is decided by the EMV rules and the configuration, and the result panel shows the TVR, TSI and CVM Results (with every set bit explained), the DE 55 data sent to the host, the issuer authentication data and scripts, and all EMV tags. Scenarios worth showing:

| Card / amount | What happens |
|---|---|
| Visa, Insert R125 | Online PIN (CVM 420300), floor limit R0 exceeded → ARQC → host 00 → ARPC verified → TC → issuer script applied |
| Mastercard, Insert R50 | Under the R100 floor limit, signature CVM, clean TVR → offline TC, no host call |
| Visa expired | TVR "Expired application" → online → issuer declines with 54 → AAC |
| Mastercard (CA key F5) | Key F5 was rejected at load (bad checksum) → TVR "CDA failed" → online |
| JCB | AID not in either AID file → application selection fails |
| Visa, Tap R125 / R600 / R800 | Offline TC under the R200 contactless floor / online PIN over the R500 CVM limit / refused over the R700 contactless limit |
| Any card, R20,000 | Issuer declines with 05; the ARPC is still verified and the card returns AAC |

The **card behaviour** selector makes the simulated card answer the first GENERATE AC with ARQC instead of TC, or decline with AAC. If Spring Boot is down, the sale fails with `HOST_UNAVAILABLE`; it is never approved.

### Terminal agent API

`POST /terminal/transactions`
```json
{ "amountMinor": 12500, "entryMode": "Insert", "transactionId": "SIM-1",
  "kernel": "simulated-contact", "simulatedCard": "VISA", "simulatedCardBehaviour": "NORMAL" }
```
`kernel`, `simulatedCard` (`VISA`, `MASTERCARD`, `VISA_EXPIRED`, `MASTERCARD_UNKNOWN_CA_KEY`, `JCB_UNSUPPORTED`) and `simulatedCardBehaviour` (`NORMAL`, `FORCE_ONLINE`, `DECLINE`) are optional. The result also carries `onlineIccData` (DE 55 hex) and `emv` (`tvr`, `tsi`, `cvmResults` with decoded bits, and every tag); [agent_protocol.hpp](cpp/app/terminal_agent/agent_protocol.hpp) documents the format. The response is `application/x-ndjson`, one JSON event per line, as stages happen:
```json
{"type":"stage","stage":"OFFLINE_DATA_AUTHENTICATION","status":"COMPLETED","detail":""}
{"type":"result","result":{"approved":true,"kernel":"simulated-contact","selectedAid":"A0000000031010","firstCryptogram":{"type":"ARQC","value":"..."},"finalCryptogram":{"type":"TC","value":"..."},"host":{"responseCode":"00","...":"..."},"...":"..."}}
```
The stream ends with exactly one `result` or `error` event (`{"type":"error","code":"HOST_UNAVAILABLE|TRANSACTION_FAILED","message":"..."}`). Invalid requests get HTTP 400; a second sale while one is running gets HTTP 409, because a terminal has one card reader.

### Authorization API (called by the agent)

```json
{
  "amountMinor": 1250,
  "currency": "ZAR",
  "aid": "A0000000031010",
  "transactionId": "demo-123",
  "cryptogramType": "ARQC",
  "panToken": "SIMULATED-VISA-01",
  "iccData": "9F2608...9F410400000001"
}
```
`iccData` is the DE 55 BER-TLV built by the kernel. The mock issuer rejects it (HTTP 400) if a mandatory tag is missing (9F26, 9F27, 9F10, 9F37, 9F36, 95, 9A, 9C, 9F02, 5F2A, 82, 9F1A), if 9F02 differs from `amountMinor`, or if 9F27 disagrees with `cryptogramType`. It approves ARQCs up to R10,000.00, declines larger amounts with 05, and declines with 54 when the TVR says the card is expired.

The response includes `approved`, `responseCode`, `authorizationCode`, `issuerAuthenticationData` (tag 91 hex: ARPC + ARC), `issuerScripts` (tag 72 hex, on approvals), `receivedEmvTags` and `message`. The ARPC uses a keyless demo formula, ARQC XOR (ARC ‖ 000000000000), which the simulated kernel checks the same way. It is not a real issuer cryptogram.

## EMV configuration

The kernel is loaded from four parameter files in [cpp/config](cpp/config), all in the same TLV text format: one `TAG=VALUE;` per line (hex, spaces allowed, `#` comments), records separated by a blank line. In the AID files `9F06` may carry a leading length byte (`9F06= 07 A0 00 00 00 03 10 10;`).

| File | Content |
|---|---|
| `TERMINAL.TXT` | One record of terminal data: 9F35 terminal type, 5F2A currency, 9F1A country, 9F33 capabilities, 9F40 additional capabilities, terminal and merchant IDs. Mandatory: 9F35, 5F2A, 9F1A, 9F33 |
| `AIDS_CONTACT.TXT` | Supported contact AIDs: 9F06, 9F1A, 5F2A, 5F36, TAC default/denial/online (9F918709/0A/0B), 9F09 version, 9F1B floor limit |
| `AIDS_CONTACTLESS.TXT` | Supported contactless AIDs, converted from `CLESSCUST.TXT`, including the reader limits 9F92810D (transaction), 9F92810E (CVM required) and 9F92810F (floor) |
| `CAKEYS.TXT` | CA public keys: 9F06 RID, 9F22 index, 9F918704 modulus, 9F918703 exponent, 9F918702 SHA-1 checksum |

Terminal data is applied first and each AID record overrides it. A card application is a candidate only if its AID matches a supported AID exactly or by prefix (partial selection); otherwise application selection fails. Like a real kernel, the loader verifies each CA key's SHA-1 checksum and rejects keys that fail. The shipped `CAKEYS.TXT` has **7 such keys**: A000000003/52, A000000004/F4, A000000004/F5, A000000025/67, A000000025/68, A000000152/5C and A000000152/5D. Two of them (25/67, 152/5D) also have an odd number of hex digits in the modulus, so the source data looks truncated. Check them against the original key source.

## Build the C++ code

Requirements: CMake 3.25+ (3.16+ if you configure with `-DEMV_BUILD_TERMINAL_AGENT=OFF`), C++17 compiler. The first configure downloads two pinned, hash-checked header libraries: cpp-httplib 0.22.0 and nlohmann/json 3.12.0.

On Windows, install CMake and the Visual Studio C++ build tools from PowerShell:
```powershell
winget install --id Kitware.CMake --exact --source winget
winget install --id Microsoft.VisualStudio.2022.BuildTools --exact --source winget --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```
The Visual Studio Build Tools install does not install CMake; both commands are required. After installation, close and reopen PowerShell, then verify CMake is available with `cmake --version`. If it is still not recognized, check whether it is installed with `winget list --id Kitware.CMake --exact --source winget`; if installed, add its `bin` directory (usually `C:\Program Files\CMake\bin`) to `PATH` or restart Windows. If the C++ build tools are not detected, run the build commands below from **Developer PowerShell for VS 2022**.

From the `cpp` directory:
```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build -C Debug --output-on-failure
.\build\Debug\emv_terminal_agent.exe
```

The command-line demo runs one sale without the UI or Spring Boot (it uses the stub host):
```powershell
.\build\Debug\emv_demo.exe
.\build\Debug\emv_demo.exe --list-kernels
.\build\Debug\emv_demo.exe --entry Tap
.\build\Debug\emv_demo.exe --card MASTERCARD --amount 5000
.\build\Debug\emv_demo.exe --card JCB_UNSUPPORTED
.\build\Debug\emv_demo.exe --card VISA --card-behaviour DECLINE
```

On macOS or Linux, use:
```bash
cd cpp
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/emv_terminal_agent
```


## EMV flow shown

The terminal UI and C++ orchestration now show all 13 requested phases: application selection; initiate application processing; read application data; processing restrictions; offline data authentication; certificate checks; cardholder verification; terminal risk management; terminal action analysis; first card action analysis; online transaction authorization when required; second card action analysis; and issuer script processing.

The UI's test-only kernel outcome selector exercises all branches. ARQC calls the mock authorization API and proceeds to final action analysis; TC completes offline as approved; AAC completes offline as declined. Offline outcomes explicitly skip online authorization, issuer response processing, second GENERATE AC, and issuer scripts. Certificate checks are part of ODA, and the vendor kernel owns issuer-script timing around final cryptogram generation. ATM-mandatory-online rules are outside this POS sale demo.

## Adding a kernel

The core never names a concrete kernel, so a new one is added without touching `emv_core`:

1. Create an adapter library (e.g. `cpp/adapters/vendor_x/`) linking `emv_core` and the vendor SDK.
2. Implement the kernel:
   - SDK exposes each EMV phase → derive from `emv::PhasedEmvKernel` and map each protected phase hook to the SDK call (the phase order is fixed by the base class).
   - SDK runs the transaction in one call / via callbacks, or is a contactless kernel → implement `emv::IEmvKernel` directly. Return an empty `finalCryptogram` from `completeOnlineTransaction` if the kernel has no second GENERATE AC.
3. Provide `registerKernels(emv::KernelRegistry&)` and call it from `buildKernelRegistry()` in `cpp/app/common/kernel_catalog.cpp`, which both the agent and the CLI use.
4. Select it at runtime: `"kernel": "<name>"` in the agent request, or `emv_demo --kernel <name>`.

## SOLID notes

- **SRP:** reader, kernel adapter, application-selection policy, host authorizer, transaction orchestrator, composition root and UI are separate units. Terminal configuration (currency) is injected, not hard-coded.
- **OCP:** new kernels and host transports are new adapter classes registered in the composition root; `TransactionService` and `emv_core` stay closed for modification.
- **LSP:** the service makes no assumption about kernel internals. Phased and single-call kernels, with or without a second GENERATE AC, are interchangeable (both simulated kernels run through the same service).
- **ISP:** `IEmvKernel` has three operations, which is the minimum the orchestrator needs. The fine-grained Book 3 phases are a protected extension surface of `PhasedEmvKernel`, used only by adapters that need them.
- **DIP:** `TransactionService` depends only on the ports in `include/emv/ports`; concrete types are wired only in the composition roots under `app/`.
- Errors are explicit results; a host timeout is not silently converted into approval. A production system must follow scheme/kernel rules for online failure, reversal, completion and recovery.

## Production integration gaps

- Vendor-certified kernel SDK and its exact callback/event lifecycle.
- Hardware reader abstraction and secure PIN entry device integration.
- The simulated engine follows the EMV Book 3 decision rules (AID matching, processing restrictions, ODA method choice, CVM list, floor limits and velocity, TAC/IAC analysis, issuer authentication and scripts), but it is not a certified kernel: certificate recovery and every cryptogram are mocked, and there are no APDUs to a real chip.
- HSM/issuer-side ARQC verification and ARPC generation; the demo ARPC formula has no key and must never be used for real.
- TLS/mTLS, authentication, idempotency, durable transaction state, reversals, audit controls, secrets management and PCI DSS controls.
- Automated unit/integration tests and scheme/kernel certification.
