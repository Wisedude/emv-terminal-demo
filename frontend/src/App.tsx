import { useState } from 'react';

type Stage = { title: string; detail: string; state: 'done' | 'active' | 'pending' | 'error' | 'skipped' };
type AuthResponse = {
  transactionId: string;
  approved: boolean;
  responseCode: string;
  authorizationCode: string | null;
  issuerAuthenticationData: string | null;
  message: string;
  firstCryptogram: 'ARQC' | 'TC' | 'AAC';
  finalCryptogram: 'TC' | 'AAC' | null;
};
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
const kernelStepDetails = [
  'Selected AID A0000000031010 (Visa test profile)',
  'Kernel initialized for AID A0000000031010 (simulated)',
  'Application records read by kernel stub (simulated)',
  'Processing restrictions checked by kernel stub',
  'Offline data authentication performed by kernel stub',
  'Certificate checks performed as part of ODA (simulated)',
  'Cardholder verification completed by kernel stub (simulated)',
  'Terminal risk management completed by kernel stub',
  'Terminal action analysis completed by kernel stub',
];
const entryModes = [
  { name: 'Tap', icon: ')))', subtitle: 'Contactless' },
  { name: 'Insert', icon: '▤', subtitle: 'Chip card' },
  { name: 'Swipe', icon: '⇆', subtitle: 'Magstripe demo' },
  { name: 'Manual Entry', icon: '⌨', subtitle: 'Simulated only' },
];

export default function App() {
  const [amount, setAmount] = useState('125.00');
  const [entryMode, setEntryMode] = useState('Insert');
  const [kernelOutcome, setKernelOutcome] = useState<'ARQC' | 'TC' | 'AAC'>('ARQC');
  const [busy, setBusy] = useState(false);
  const [stages, setStages] = useState<Stage[]>(initialStages);
  const [result, setResult] = useState<AuthResponse | null>(null);
  const [error, setError] = useState('');
  const [transactionId, setTransactionId] = useState('—');

  const updateStage = (index: number, state: Stage['state'], detail?: string) => setStages(old => old.map((s, i) => i === index ? { ...s, state, detail: detail ?? s.detail } : s));
  const wait = (ms: number) => new Promise(resolve => setTimeout(resolve, ms));

  async function startSale(mode = entryMode) {
    setEntryMode(mode);
    const value = Number(amount);
    if (!Number.isFinite(value) || value <= 0 || !/^\d+(\.\d{1,2})?$/.test(amount)) {
      setError('Enter a valid amount greater than R0.00 (maximum 2 decimal places).');
      setResult(null); setStages(initialStages); setTransactionId('—');
      return;
    }
    setError(''); setBusy(true); setResult(null); const txId = `SIM-${Date.now().toString().slice(-8)}`; setTransactionId(txId);
    setStages(initialStages.map((s, i) => ({ ...s, state: i === 0 ? 'active' : 'pending' })));
    try {
      await wait(250);
      for (let i = 0; i < kernelStepDetails.length; i++) {
        updateStage(i, 'active');
        await wait(250);
        updateStage(i, 'done', kernelStepDetails[i]);
      }
      updateStage(9, 'active');
      await wait(350);
      updateStage(9, 'done', `Kernel stub returned ${kernelOutcome} (simulated; not a real cryptogram)`);
      if (kernelOutcome !== 'ARQC') {
        updateStage(10, 'skipped', 'Skipped: first GENERATE AC did not request online authorization');
        updateStage(11, 'skipped', 'Skipped: no issuer response or second GENERATE AC for this offline outcome');
        updateStage(12, 'skipped', 'Skipped: no online issuer scripts were supplied');
        setResult({
          transactionId: txId,
          approved: kernelOutcome === 'TC',
          responseCode: 'N/A',
          authorizationCode: null,
          issuerAuthenticationData: null,
          message: kernelOutcome === 'TC' ? 'Kernel approved offline' : 'Kernel declined offline',
          firstCryptogram: kernelOutcome,
          finalCryptogram: null,
        });
        return;
      }

      updateStage(10, 'active');
      const auth = await fetch('/api/authorize', {
        method: 'POST', headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ amountMinor: Math.round(value * 100), currency: 'ZAR', aid: 'A0000000031010', transactionId: txId, cryptogramType: 'ARQC', panToken: 'SIMULATED-CARD-01' })
      });
      if (!auth.ok) {
        const body = await auth.json().catch(() => null) as { message?: string; detail?: string } | null;
        throw new Error(body?.message ?? body?.detail ?? `Authorization server returned HTTP ${auth.status}`);
      }
      const response: Omit<AuthResponse, 'firstCryptogram' | 'finalCryptogram'> = await auth.json();
      if (response.transactionId !== txId || typeof response.approved !== 'boolean' || !response.responseCode || !response.message) {
        throw new Error('Authorization server returned an invalid response');
      }
      updateStage(10, 'done', `Host response ${response.responseCode}: ${response.message}`);
      updateStage(11, 'active');
      await wait(350);
      const finalCryptogram = response.approved ? 'TC' : 'AAC';
      updateStage(11, 'done', `Kernel stub returned ${finalCryptogram} (simulated final outcome)`);
      updateStage(12, 'active');
      await wait(350);
      updateStage(12, 'done', 'Issuer scripts handled by the kernel; none supplied by the mock issuer');
      setResult({ ...response, firstCryptogram: 'ARQC', finalCryptogram });
    } catch (e) {
      const message = e instanceof Error ? e.message : 'Unexpected error';
      setError(e instanceof TypeError ? `${message}. Check that the Spring Boot API is running at http://localhost:8080.` : message);
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
          <label className="scenario-select">KERNEL TEST OUTCOME
            <select value={kernelOutcome} onChange={event => {
              const outcome = event.target.value;
              if (outcome === 'ARQC' || outcome === 'TC' || outcome === 'AAC') setKernelOutcome(outcome);
            }} disabled={busy}>
              <option value="ARQC">ARQC · request online authorization</option>
              <option value="TC">TC · approve offline</option>
              <option value="AAC">AAC · decline offline</option>
            </select>
          </label>
          <div className="terminal-preview"><div className="terminal-top"><span>PAYMENT TERMINAL</span><span>● {busy ? 'BUSY' : 'READY'}</span></div><div className="terminal-screen"><div className="contactless-symbol">)))</div><div className="terminal-prompt">{busy ? 'PROCESSING TRANSACTION' : 'PLEASE PRESENT CARD'}</div><div className="terminal-amount">R {Number(amount || 0).toLocaleString('en-ZA', {minimumFractionDigits:2, maximumFractionDigits:2})}</div><div className="terminal-foot">{busy ? 'PLEASE WAIT · DO NOT REMOVE CARD' : `MODE: ${entryMode.toUpperCase()}`}</div></div><div className="terminal-base"><span/><span/><span/></div></div>
          <button className="reset-button" disabled={busy} onClick={reset}>Reset transaction</button>{error && <div className="error-box" role="alert">{error}</div>}
        </section>
        <div className="disclaimer"><span>ⓘ</span><p>Training environment only. All card interactions, cryptograms and issuer authentication data are simulated. Never enter real payment-card details.</p></div>
      </section>
      <section className="right-column"><div className="eyebrow">LIVE MONITOR / 02</div><div className="monitor-heading"><div><h2>Transaction flow</h2><p className="muted">EMV kernel integration lifecycle</p></div><span className={`status-pill ${busy ? 'working' : result ? (result.approved ? 'approved' : 'declined') : ''}`}><span className="status-dot"/>{busy ? 'IN PROGRESS' : result ? (result.approved ? 'APPROVED' : 'DECLINED') : 'STANDBY'}</span></div>
        <div className="timeline">{stages.map((s, i) => <div className={`stage ${s.state}`} key={s.title}><div className="stage-rail"><div className="stage-marker">{s.state === 'done' ? '✓' : s.state === 'error' ? '!' : s.state === 'skipped' ? '–' : String(i + 1).padStart(2,'0')}</div>{i < stages.length - 1 && <div className="stage-line"/>}</div><div className="stage-copy"><div className="stage-title">{s.title}<span>{s.state === 'done' ? 'COMPLETE' : s.state === 'active' ? 'RUNNING' : s.state === 'error' ? 'ERROR' : s.state === 'skipped' ? 'SKIPPED' : 'WAITING'}</span></div><p>{s.detail}</p></div></div>)}</div>
        <div className="result-card"><div className="result-header"><div><div className="panel-label">TRANSACTION RESULT</div><h3>{result ? (result.approved ? 'Payment approved' : 'Payment declined') : 'Awaiting transaction'}</h3></div><div className={`result-icon ${result ? (result.approved ? 'good' : 'bad') : ''}`}>{result ? (result.approved ? '✓' : '×') : '—'}</div></div><div className="result-rows"><div><span>Amount</span><strong>R {Number(amount || 0).toFixed(2)}</strong></div><div><span>Entry mode</span><strong>{entryMode}</strong></div><div><span>Transaction ID</span><strong className="mono">{transactionId}</strong></div><div><span>Selected AID</span><strong className="mono">{result ? 'A0000000031010' : '—'}</strong></div><div><span>First cryptogram</span><strong className="mono">{result ? `${result.firstCryptogram} (simulated)` : '—'}</strong></div><div><span>Final cryptogram</span><strong className="mono">{result ? (result.finalCryptogram ? `${result.finalCryptogram} (simulated)` : `${result.firstCryptogram} (offline outcome)`) : '—'}</strong></div><div><span>Response code</span><strong className="mono">{result?.responseCode ?? '—'}</strong></div><div><span>Authorization code</span><strong className="mono">{result?.authorizationCode ?? '—'}</strong></div></div>{result?.issuerAuthenticationData && <div className="arpc"><div><span className="arpc-label">ISSUER AUTHENTICATION DATA · DEMO ARPC</span><strong className="mono">{result.issuerAuthenticationData}</strong></div><p>Placeholder only — real ARPC is generated by the issuer using issuer keys and validated/processed by the approved kernel.</p></div>}</div>
        <div className="api-note"><div className="api-icon">{ '{ }' }</div><div><strong>Mock issuer API</strong><p>POST <code>/api/authorize</code> · JSON request/response</p></div><span className="api-live">HTTP</span></div>
      </section>
    </div>
    <footer><span>EMV TERMINAL LAB <b>·</b> INTEGRATION EXERCISE</span><span>NO REAL CARD DATA · NO LIVE PAYMENT PROCESSING</span></footer>
  </main>;
}
