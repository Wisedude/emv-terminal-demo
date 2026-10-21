import { useState } from 'react';

type Stage = { title: string; detail: string; state: 'done' | 'active' | 'pending' | 'error' | 'skipped' };
type CryptogramType = 'ARQC' | 'TC' | 'AAC';
type Cryptogram = { type: CryptogramType; value: string };
type HostResponse = {
  approved: boolean;
  responseCode: string;
  authorizationCode: string | null;
  issuerAuthenticationData: string | null;
  issuerScripts: string | null;
  message: string;
};
type FlagField = { hex: string; flags: string[] };
type EmvData = {
  tvr: FlagField | null;
  tsi: FlagField | null;
  cvmResults: { hex: string; description: string } | null;
  tags: { tag: string; name: string; value: string }[];
};
// Result of a sale as reported by the C++ terminal agent.
type TerminalResult = {
  transactionId: string;
  kernel: string;
  approved: boolean;
  message: string;
  selectedAid: string;
  firstCryptogram: Cryptogram;
  finalCryptogram: Cryptogram | null;
  host: HostResponse | null;
  issuerScriptsProcessed: boolean;
  issuerScriptStatus: string;
  card: string;
  onlineIccData: string | null;
  emv: EmvData;
};
type AgentEvent =
  | { type: 'stage'; stage: string; status: 'STARTED' | 'COMPLETED' | 'SKIPPED'; detail: string }
  | { type: 'result'; result: TerminalResult }
  | { type: 'error'; code: string; message: string };

const initialStages: Stage[] = [
  { title: 'Application selection', detail: 'Build candidate list and select an AID', state: 'pending' },
  { title: 'Initiate application processing', detail: 'Start processing for the selected AID', state: 'pending' },
  { title: 'Read application data', detail: 'Read the application records requested by the kernel', state: 'pending' },
  { title: 'Processing restrictions', detail: 'Check application version, dates, and usage controls', state: 'pending' },
  { title: 'Offline data authentication', detail: 'Perform the configured ODA method', state: 'pending' },
  { title: 'Certificates', detail: 'Validate the certificate chain as part of ODA', state: 'pending' },
  { title: 'Cardholder verification', detail: 'Perform the kernel-selected CVM', state: 'pending' },
  { title: 'Terminal risk management', detail: 'Apply terminal floor-limit and risk checks', state: 'pending' },
  { title: 'Terminal action analysis', detail: 'Apply terminal action codes to terminal results', state: 'pending' },
  { title: 'First card action analysis', detail: 'Request first GENERATE AC; result determines whether online is needed', state: 'pending' },
  { title: 'Online transaction authorization', detail: 'Send tokenized JSON only when the kernel returns ARQC', state: 'pending' },
  { title: 'Second card action analysis', detail: 'Pass issuer response to the kernel for final action analysis', state: 'pending' },
  { title: 'Issuer script processing', detail: 'Kernel processes issuer scripts at scheme/kernel-defined points', state: 'pending' },
];
// Stage ids sent by the terminal agent, in the same order as initialStages.
const stageIds = [
  'APPLICATION_SELECTION', 'INITIATE_APPLICATION_PROCESSING', 'READ_APPLICATION_DATA', 'PROCESSING_RESTRICTIONS',
  'OFFLINE_DATA_AUTHENTICATION', 'CERTIFICATES', 'CARDHOLDER_VERIFICATION', 'TERMINAL_RISK_MANAGEMENT',
  'TERMINAL_ACTION_ANALYSIS', 'FIRST_CARD_ACTION_ANALYSIS', 'ONLINE_AUTHORIZATION', 'SECOND_CARD_ACTION_ANALYSIS',
  'ISSUER_SCRIPT_PROCESSING',
];
const stageStates: Record<string, Stage['state']> = { STARTED: 'active', COMPLETED: 'done', SKIPPED: 'skipped' };
const entryModes = [
  { name: 'Tap', agentMode: 'Tap', icon: ')))', subtitle: 'Contactless' },
  { name: 'Insert', agentMode: 'Insert', icon: '▤', subtitle: 'Chip card' },
  { name: 'Swipe', agentMode: 'Swipe', icon: '⇆', subtitle: 'Magstripe demo' },
  { name: 'Manual Entry', agentMode: 'Manual', icon: '⌨', subtitle: 'Simulated only' },
];
const testCards = [
  { id: 'VISA', label: 'Visa credit · A0000000031010' },
  { id: 'MASTERCARD', label: 'Mastercard · A0000000041010' },
  { id: 'VISA_EXPIRED', label: 'Visa · expired card' },
  { id: 'MASTERCARD_UNKNOWN_CA_KEY', label: 'Mastercard · CA key F5 (rejected)' },
  { id: 'JCB_UNSUPPORTED', label: 'JCB · AID not supported' },
] as const;
type CardId = typeof testCards[number]['id'];
type CardBehaviour = 'NORMAL' | 'FORCE_ONLINE' | 'DECLINE';
const agentUnavailable = 'Check that the C++ terminal agent (emv_terminal_agent) is running on http://localhost:9090.';

// Yields each NDJSON event from the agent's streamed response as it arrives.
async function* readEvents(body: ReadableStream<Uint8Array>): AsyncGenerator<AgentEvent> {
  const reader = body.getReader();
  const decoder = new TextDecoder();
  let buffer = '';
  for (;;) {
    const { value, done } = await reader.read();
    buffer += decoder.decode(value, { stream: !done });
    let newline;
    while ((newline = buffer.indexOf('\n')) >= 0) {
      const line = buffer.slice(0, newline).trim();
      buffer = buffer.slice(newline + 1);
      if (line) yield JSON.parse(line) as AgentEvent;
    }
    if (done) return;
  }
}

function FlagBlock({ title, field, empty }: { title: string; field: FlagField | null; empty: string }) {
  return <div className="emv-block">
    <div className="emv-block-head"><span className="panel-label">{title}</span><strong className="mono">{field?.hex ?? '—'}</strong></div>
    {field && field.flags.length > 0 ? <ul>{field.flags.map(flag => <li key={flag}>{flag}</li>)}</ul> : <p className="emv-none">{empty}</p>}
  </div>;
}

// TVR, TSI and CVM Results as the kernel left them, plus the DE 55 data that went to the host.
function EmvPanel({ result }: { result: TerminalResult }) {
  const { emv } = result;
  return <div className="emv-card">
    <div className="panel-label">EMV KERNEL DATA <span>{result.kernel}</span></div>
    <FlagBlock title="TVR · TERMINAL VERIFICATION RESULTS (95)" field={emv.tvr} empty="No bits set: every terminal check passed" />
    <FlagBlock title="TSI · TRANSACTION STATUS INFORMATION (9B)" field={emv.tsi} empty="No processing steps recorded" />
    <div className="emv-block">
      <div className="emv-block-head"><span className="panel-label">CVM RESULTS (9F34)</span><strong className="mono">{emv.cvmResults?.hex ?? '—'}</strong></div>
      <p className="emv-none">{emv.cvmResults?.description ?? 'Not available'}</p>
    </div>
    {result.onlineIccData && <div className="arpc"><span className="arpc-label">DE 55 · ICC DATA SENT TO HOST ({result.onlineIccData.length / 2} BYTES)</span><strong className="mono">{result.onlineIccData}</strong></div>}
    <details className="emv-tags"><summary>ALL EMV TAGS ({emv.tags.length})</summary>
      <table><tbody>{emv.tags.map(tag => <tr key={tag.tag}><td className="mono">{tag.tag}</td><td>{tag.name}</td><td className="mono">{tag.value}</td></tr>)}</tbody></table>
    </details>
  </div>;
}

export default function App() {
  const [amount, setAmount] = useState('125.00');
  const [entryMode, setEntryMode] = useState('Insert');
  const [card, setCard] = useState<CardId>('VISA');
  const [cardBehaviour, setCardBehaviour] = useState<CardBehaviour>('NORMAL');
  const [busy, setBusy] = useState(false);
  const [stages, setStages] = useState<Stage[]>(initialStages);
  const [result, setResult] = useState<TerminalResult | null>(null);
  const [error, setError] = useState('');
  const [transactionId, setTransactionId] = useState('—');

  const updateStage = (index: number, state: Stage['state'], detail?: string) => setStages(old => old.map((s, i) => i === index ? { ...s, state, detail: detail ?? s.detail } : s));

  async function startSale(mode = entryMode) {
    setEntryMode(mode);
    const value = Number(amount);
    if (!Number.isFinite(value) || value <= 0 || !/^\d+(\.\d{1,2})?$/.test(amount)) {
      setError('Enter a valid amount greater than R0.00 (maximum 2 decimal places).');
      setResult(null); setStages(initialStages); setTransactionId('—');
      return;
    }
    setError(''); setBusy(true); setResult(null); const txId = `SIM-${Date.now().toString().slice(-8)}`; setTransactionId(txId);
    setStages(initialStages);
    try {
      const agentMode = entryModes.find(m => m.name === mode)?.agentMode ?? mode;
      const response = await fetch('/terminal/transactions', {
        method: 'POST', headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ amountMinor: Math.round(value * 100), entryMode: agentMode, transactionId: txId, simulatedCard: card, simulatedCardBehaviour: cardBehaviour })
      });
      if (!response.ok || !response.body) {
        const body = await response.json().catch(() => null) as { message?: string } | null;
        throw new Error(body?.message ?? `Terminal agent returned HTTP ${response.status}. ${agentUnavailable}`);
      }

      for await (const event of readEvents(response.body)) {
        if (event.type === 'stage') {
          const index = stageIds.indexOf(event.stage);
          if (index >= 0) updateStage(index, stageStates[event.status] ?? 'pending', event.detail || undefined);
        } else if (event.type === 'result') {
          setResult(event.result);
          return;
        } else {
          throw new Error(event.message);
        }
      }
      throw new Error('Terminal agent closed the connection before the transaction finished');
    } catch (e) {
      const message = e instanceof Error ? e.message : 'Unexpected error';
      setError(e instanceof TypeError ? `${message}. ${agentUnavailable}` : message);
      setStages(old => old.map(s => s.state === 'active' ? { ...s, state: 'error', detail: message } : s));
    } finally { setBusy(false); }
  }

  function reset() { setStages(initialStages); setResult(null); setError(''); setTransactionId('—'); }

  return <main className="shell">
    <header className="topbar"><div className="brand"><div className="brand-mark">EMV</div><div><div className="brand-name">TERMINAL<span>LAB</span></div><div className="brand-sub">PAYMENT APPLICATION SIMULATOR</div></div></div><div className="top-right"><span className="dot"/> LOCAL DEMO <span className="version">v1.0</span></div></header>
    <div className="workspace">
      <section className="left-column">
        <div className="eyebrow">TRANSACTION WORKSPACE / 01</div><h1>Sale transaction</h1><p className="muted">Run a simulated EMV transaction from card entry through online authorization.</p>
        <section className="amount-panel"><div className="panel-label">TRANSACTION AMOUNT <span>ZAR · SOUTH AFRICAN RAND</span></div><div className="amount-input"><span>R</span><input aria-label="Transaction amount" value={amount} onChange={e => setAmount(e.target.value)} disabled={busy} inputMode="decimal"/><span className="currency">ZAR</span></div><div className="quick-amounts">{['25.00','125.00','250.00','999.99'].map(v => <button key={v} disabled={busy} onClick={() => setAmount(v)}>R {v}</button>)}</div></section>
        <section className="card-panel">
          <div className="section-heading"><div><div className="panel-label">CARD ENTRY</div><h2>Present card to terminal</h2></div><span className="sim-badge">SIMULATED</span></div>
          <div className="entry-grid">{entryModes.map(m => <button key={m.name} className={`entry-button ${entryMode === m.name ? 'selected' : ''}`} onClick={() => void startSale(m.name)} disabled={busy}><span className="entry-icon">{m.icon}</span><span className="entry-name">{m.name}</span><span className="entry-subtitle">{m.subtitle}</span></button>)}</div>
          <div className="scenario-grid">
            <label className="scenario-select">TEST CARD
              <select value={card} onChange={event => setCard(event.target.value as CardId)} disabled={busy}>
                {testCards.map(testCard => <option key={testCard.id} value={testCard.id}>{testCard.label}</option>)}
              </select>
            </label>
            <label className="scenario-select">CARD BEHAVIOUR
              <select value={cardBehaviour} onChange={event => setCardBehaviour(event.target.value as CardBehaviour)} disabled={busy}>
                <option value="NORMAL">Normal · card follows the terminal</option>
                <option value="FORCE_ONLINE">Force online · ARQC instead of TC</option>
                <option value="DECLINE">Decline · card returns AAC</option>
              </select>
            </label>
          </div>
          <div className="terminal-preview"><div className="terminal-top"><span>PAYMENT TERMINAL</span><span>● {busy ? 'BUSY' : 'READY'}</span></div><div className="terminal-screen"><div className="contactless-symbol">)))</div><div className="terminal-prompt">{busy ? 'PROCESSING TRANSACTION' : 'PLEASE PRESENT CARD'}</div><div className="terminal-amount">R {Number(amount || 0).toLocaleString('en-ZA', {minimumFractionDigits:2, maximumFractionDigits:2})}</div><div className="terminal-foot">{busy ? 'PLEASE WAIT · DO NOT REMOVE CARD' : `MODE: ${entryMode.toUpperCase()}`}</div></div><div className="terminal-base"><span/><span/><span/></div></div>
          <button className="reset-button" disabled={busy} onClick={reset}>Reset transaction</button>{error && <div className="error-box" role="alert">{error}</div>}
        </section>
        <div className="disclaimer"><span>ⓘ</span><p>Training environment only. All card interactions, cryptograms and issuer authentication data are simulated. Never enter real payment-card details.</p></div>
      </section>
      <section className="right-column"><div className="eyebrow">LIVE MONITOR / 02</div><div className="monitor-heading"><div><h2>Transaction flow</h2><p className="muted">EMV kernel integration lifecycle</p></div><span className={`status-pill ${busy ? 'working' : result ? (result.approved ? 'approved' : 'declined') : ''}`}><span className="status-dot"/>{busy ? 'IN PROGRESS' : result ? (result.approved ? 'APPROVED' : 'DECLINED') : 'STANDBY'}</span></div>
        <div className="timeline">{stages.map((s, i) => <div className={`stage ${s.state}`} key={s.title}><div className="stage-rail"><div className="stage-marker">{s.state === 'done' ? '✓' : s.state === 'error' ? '!' : s.state === 'skipped' ? '–' : String(i + 1).padStart(2,'0')}</div>{i < stages.length - 1 && <div className="stage-line"/>}</div><div className="stage-copy"><div className="stage-title">{s.title}<span>{s.state === 'done' ? 'COMPLETE' : s.state === 'active' ? 'RUNNING' : s.state === 'error' ? 'ERROR' : s.state === 'skipped' ? 'SKIPPED' : 'WAITING'}</span></div><p>{s.detail}</p></div></div>)}</div>
        <div className="result-card"><div className="result-header"><div><div className="panel-label">TRANSACTION RESULT</div><h3>{result ? (result.approved ? 'Payment approved' : 'Payment declined') : 'Awaiting transaction'}</h3></div><div className={`result-icon ${result ? (result.approved ? 'good' : 'bad') : ''}`}>{result ? (result.approved ? '✓' : '×') : '—'}</div></div><div className="result-rows"><div><span>Amount</span><strong>R {Number(amount || 0).toFixed(2)}</strong></div><div><span>Entry mode</span><strong>{entryMode}</strong></div><div><span>Transaction ID</span><strong className="mono">{transactionId}</strong></div><div><span>Test card</span><strong className="mono">{result?.card ?? '—'}</strong></div><div><span>Kernel</span><strong className="mono">{result?.kernel ?? '—'}</strong></div><div><span>Selected AID</span><strong className="mono">{result?.selectedAid ?? '—'}</strong></div><div><span>First cryptogram</span><strong className="mono">{result ? `${result.firstCryptogram.type} (simulated)` : '—'}</strong></div><div><span>Final cryptogram</span><strong className="mono">{result ? (result.finalCryptogram ? `${result.finalCryptogram.type} (simulated)` : (result.host ? 'None (no second GENERATE AC)' : `${result.firstCryptogram.type} (offline outcome)`)) : '—'}</strong></div><div><span>Response code</span><strong className="mono">{result ? (result.host?.responseCode ?? 'N/A') : '—'}</strong></div><div><span>Authorization code</span><strong className="mono">{result?.host?.authorizationCode ?? '—'}</strong></div></div>{result?.host?.issuerAuthenticationData && <div className="arpc"><div><span className="arpc-label">ISSUER AUTHENTICATION DATA · TAG 91 (ARPC + ARC)</span><strong className="mono">{result.host.issuerAuthenticationData}</strong></div>{result.host.issuerScripts && <div className="arpc-script"><span className="arpc-label">ISSUER SCRIPT · TAG 71/72</span><strong className="mono">{result.host.issuerScripts}</strong></div>}<p>Mock values: a real ARPC is computed by the issuer's HSM and verified by the certified kernel. {result.issuerScriptStatus}</p></div>}{result && <EmvPanel result={result} />}</div>
        <div className="api-note"><div className="api-icon">{ '{ }' }</div><div><strong>C++ terminal agent</strong><p>POST <code>/terminal/transactions</code> · kernel authorizes via <code>/api/authorize</code></p></div><span className="api-live">NDJSON</span></div>
      </section>
    </div>
    <footer><span>EMV TERMINAL LAB <b>·</b> INTEGRATION EXERCISE</span><span>NO REAL CARD DATA · NO LIVE PAYMENT PROCESSING</span></footer>
  </main>;
}
