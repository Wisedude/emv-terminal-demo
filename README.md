# EMV Terminal Integration Demo (simulation only)

A runnable teaching/demo scaffold aligned to the supplied EMV Kernel Integration Exercise. It contains:

- **React + TypeScript terminal UI** with Tap, Insert, Swipe and Manual Entry buttons that each start a simulated sale, amount input, live step status and result summary.
- **C++17 integration layer** showing application-selection callbacks, first GENERATE AC, online authorization, issuer response processing, second GENERATE AC and error paths.
- **Spring Boot mock authorization API** accepting JSON and returning a deterministic approval plus a *demonstration-only* ARPC field.

> **Important:** This is not a real payment application and does not communicate with physical card readers or an EMV kernel. The C++ kernel/reader interfaces and cryptographic outputs are stubs. The UI uses simulated card profiles; do not enter real card data. The returned ARPC is a placeholder string, not a valid issuer-generated EMV ARPC. Never use this scaffold for live payments, PCI data capture, certification, or production decisions.

## Architecture

```text
React terminal UI --HTTP JSON--> Spring Boot mock auth API
        |                              |
        | simulated UI flow            | APPROVED / DECLINED + demo ARPC
        v                              v
     UI step model                authorization DTO

C++ integration demo (standalone executable):
ICardReader -> IEmvKernel candidate list -> IApplicationSelector
  -> IEmvKernel process to first GENERATE AC -> IHostAuthorizer (ARQC only)
  -> IEmvKernel online completion (issuer auth, scripts, second GENERATE AC if any) -> result
```

C++ layout (ports and adapters):

```text
cpp/
  include/emv/          emv_core public headers (kernel-agnostic, no vendor code)
    domain.hpp            value types: CardProfile, cryptograms, HostRequest/Response, TransactionResult
    ports/                ICardReader, IEmvKernel, IApplicationSelector, IHostAuthorizer
    kernel/               PhasedEmvKernel (base for phase-by-phase SDKs), KernelRegistry
    policy/               PreferredAidSelector
    transaction_service.hpp
  src/                  emv_core implementation
  adapters/simulated/   emv_simulated: simulated reader, contact + contactless kernels, stub host
  app/main.cpp          composition root: the only code that names concrete adapters
  tests/                one test executable per unit, sharing fakes in test_support.hpp
```

The UI and C++ executable are intentionally separate demonstration surfaces. The Java server demonstrates the online authorization contract; the C++ sample uses an `IHostAuthorizer` interface with a deterministic local stub so it builds without HTTP dependencies. A real integration would implement that interface using an approved transport/client and connect the UI to a native bridge/SDK supplied by the terminal vendor.

## Run the UI + mock API

Requirements: Node.js 20+, Java 17+, Maven 3.9+.

Terminal 1:
```bash
cd backend
mvn spring-boot:run
```
Health check: `GET http://localhost:8080/api/health`

Terminal 2:
```bash
cd frontend
npm install
npm run dev
```
Open the Vite URL printed by the command. The UI sends `POST /api/authorize` through Vite's development proxy to `http://localhost:8080`, avoiding browser CORS restrictions.

Enter an amount and select any card-entry button to run the full simulated flow. The mock issuer approves ARQC requests up to R10,000.00 and declines larger amounts, so both outcomes can be exercised. Each result shows the selected AID, first/final simulated cryptogram, authorization response and demo issuer-authentication data. No button reads physical cards or collects PAN data.

Mock authorization request:
```json
{
  "amountMinor": 1250,
  "currency": "ZAR",
  "aid": "A0000000031010",
  "transactionId": "demo-123",
  "cryptogramType": "ARQC",
  "panToken": "SIMULATED-CARD-01"
}
```
The response includes `approved`, `responseCode`, `authorizationCode`, `issuerAuthenticationData`, and `message`. `issuerAuthenticationData` is explicitly demo data, not a real ARPC.

## Build/run the C++ flow

Requirements: CMake 3.16+, C++17 compiler.

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
.\build\Debug\emv_demo.exe
.\build\Debug\emv_demo.exe --list-kernels
.\build\Debug\emv_demo.exe --kernel simulated-contactless
```

On macOS or Linux, use:
```bash
cd cpp
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/emv_demo
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
3. Provide `registerKernels(emv::KernelRegistry&)` and call it from `buildKernelRegistry()` in `cpp/app/main.cpp`.
4. Select it at runtime: `emv_demo --kernel <name>`.

## SOLID notes

- **SRP:** reader, kernel adapter, application-selection policy, host authorizer, transaction orchestrator, composition root and UI are separate units. Terminal configuration (currency) is injected, not hard-coded.
- **OCP:** new kernels and host transports are new adapter classes registered in the composition root; `TransactionService` and `emv_core` stay closed for modification.
- **LSP:** the service makes no assumption about kernel internals. Phased and single-call kernels, with or without a second GENERATE AC, are interchangeable (both simulated kernels run through the same service).
- **ISP:** `IEmvKernel` has three operations, which is the minimum the orchestrator needs. The fine-grained Book 3 phases are a protected extension surface of `PhasedEmvKernel`, used only by adapters that need them.
- **DIP:** `TransactionService` depends only on the ports in `include/emv/ports`; concrete types are wired only in `app/main.cpp`.
- Errors are explicit results; a host timeout is not silently converted into approval. A production system must follow scheme/kernel rules for online failure, reversal, completion and recovery.

## Production integration gaps

- Vendor-certified kernel SDK and its exact callback/event lifecycle.
- Hardware reader abstraction and secure PIN entry device integration.
- Correct EMV TLV/BER-TLV parsing, terminal/card data, CVM, risk management, TVR/TSI and issuer authentication/script processing inside the approved kernel.
- HSM/issuer-side ARPC generation and validation; ARPC must not be fabricated by the POS app.
- TLS/mTLS, authentication, idempotency, durable transaction state, reversals, audit controls, secrets management and PCI DSS controls.
- Automated unit/integration tests and scheme/kernel certification.
