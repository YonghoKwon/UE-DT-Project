# PCD-20HZ / PCD-RESUME STOMP fault server (Node 22)

Run `node Tools/StompFaultServer/server.mjs`. The TCP listener binds only to
127.0.0.1:61618; it does not authenticate or modify an installed broker.
CONNECT/STOMP negotiates STOMP 1.2 heartbeats; SEND accepts binary content-length
bodies and returns requested receipts. Bodies and authentication headers are
never logged. JSONL samples record bytes, sends, receipts, heartbeats and closure.

Options use `--name=integer` (milliseconds unless specified):

| Option | Default | Purpose |
| --- | ---: | --- |
| port | 61618 | TCP port; 0 chooses a free port |
| heartbeat-ms | 1000 | Server heartbeat capability; 0 disables |
| require-heartbeat-ms | 1000 | Server requested client heartbeat; 0 disables |
| pause-after-ms / pause-ms | 0 / 0 | Pause TCP reads once after CONNECTED |
| read-bytes-per-second | 0 | Throttle TCP reads; 0 unlimited |
| max-read-mbps | 0 | Decimal **MB/s**, accepts decimals (18.5 = 148 Mbit/s); overrides read-bytes-per-second |
| rtt-ms | 0 | Additional receipt round-trip latency; does not serialize the receipt window |
| receipt-delay-ms | 0 | Delay receipts without stopping reads |
| error-after-ms | 0 | Send fixed, non-secret ERROR and close |
| close-after-ms | 0 | Force TCP reset |
| silent-after-ms | 0 | Stop server heartbeats |
| max-body-bytes | 67108864 | Reject larger content-length |

`--echo` additionally sends MESSAGE to matching subscriptions for loopback
validation (retains one bounded body); ordinary receipt-only mode discards body
chunks as they arrive. `--no-enforce-heartbeat` disables client silence checks.
Injected faults recur on each connection, allowing reconnect testing.

PCD-OPT production-link reproduction: `--max-read-mbps=18.5 --rtt-ms=10`.
Reads use elapsed-time byte credit with a 20ms maximum burst and readable events,
so TCP chunk sizes and Windows timer resolution do not define throughput.
Read pauses remain independent of the rate timer. Each JSONL sample includes
wire bytes, measured decimal `readMBps` and bounded socket read-buffer size.
The RTT option delays receipts in both directions' aggregate; it does not model
packet-level latency, loss, Artemis persistence or a shared multi-connection cap.

The harness models TCP backpressure and protocol faults, not Artemis paging,
durability or business acknowledgements. A receipt is broker acceptance only.

After an Editor Development build, run the opt-in worker regressions:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/StompFaultServer/run-faults.ps1
```

Each case owns its local server and enables `MA0T10_PCD20HZ_CASE` only in the
test process. Healthy throughput, bounded skips under an eight-second pause,
heartbeat, server silence, ERROR and reset/reconnect are tested separately.
PCD-RESUME adds an eighth case, `overload-resume`: a 20-second read pause with
45 seconds of synthetic 576x56 PCD input drives the actual publisher, async PCD
codec and shared worker. It requires an explicit overload suspension, automatic
recovery after drain, a receipt for a new frame beyond the suspension boundary,
no connection replacement and zero outstanding bytes after shutdown. Separate
`PointCloudAutoResume` automation covers cooldown, capped exponential backoff,
receipt-progress reset, user stop/reconfiguration, disconnected recovery,
option-off behavior, stale serialization, accounting, routing and UI wording.
NullRHI is used only for these socket tests. DLL hashes and the automation result
are saved with the server JSONL; Editor/build overlap invalidates a run.

For actual ML-X(80) acquisition, PCD encoding, receipts and loopback validation:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/StompFaultServer/run-native.ps1 -Broker Fake -Interval .05 -Lidars 1 -Seconds 300
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/StompFaultServer/run-native.ps1 -Broker Artemis -Interval .05 -Lidars 1 -Seconds 300
```

For rendered recovery acceptance and direct UI checks, use Fake with
`-RecoveryFault` and `-ServerOptions @('--pause-after-ms=30000','--pause-ms=20000')`
(invoke from PowerShell so the array is preserved). This mode expects explicit
overload loss and ledger gaps, verifies recovery, receipts and valid loopback
PCD, and records client viewport, effective UI DPI, font scale and status text.
Normal native throughput mode continues to require complete contiguous output.
Fault mode reports socket submission attempts, applied receipts, validated
consumer frames and dropped diagnostic receive events separately: the existing
five-second Raw receipt retry and bounded diagnostic receive queue can make
these totals differ during a long pause. Recovery acceptance requires a new
post-suspension receipt and valid consumer frame, explicit loss and drained
reservations; it does not claim every diagnostic event was retained.

The native test uses D3D12, the committed SensorTestMap snapshot and 1920x1080
PIE, with ten seconds of warmup. Test cameras remain in preview mode; only LiDAR
PCD is transmitted. Intervals .1/.2 and two LiDARs are also supported. An already
running approved Artemis instance is required; its settings are never changed.
Credentials are read from existing local configuration or smoke-runner defaults,
passed in child environment variables and restored afterward, never printed.

PCD-OPT diagnosis uses `run-native.ps1 -OptProbe` with the same broker, interval
and duration parameters. This probe records publisher preparation queues,
reservation frames/bytes, receipt and serialization latency, revisions, discarded
work and wall pacing. It requires the **accepted set** to reach receipt and valid
loopback consumption, but permits acquisition gaps caused by the existing explicit
overload policy. Ordinary native acceptance still requires contiguous full output.
Add `-SwitchIntervals` for an actual production Slab JSON/DataSync/motion run
(180-second transient fixture, ObservationAllowed with PCD output) and eight
0.1/0.05 changes through the actor API. This is diagnostic reproduction, not a
claim that the reported production three-second freeze has been fixed.
`python Tools/StompFaultServer/summarize-opt.py <run-directory> ...` writes each
run's `timeseries.csv` and `summary.json` without loading credentials or bodies.

PCD-OPT saturation and adaptive admission use `run-package.ps1`. Despite the
historical name, `-EditorPie` selects actual Editor PIE; omitting it selects an
already staged Development or Shipping package. Examples after an Editor build:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/StompFaultServer/run-package.ps1 -EditorPie -Case Limited -Seconds 300 -SendBufferBytes 16777216 -KeepDevices -AcceptRateControl -RequireNoHitch
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/StompFaultServer/run-package.ps1 -EditorPie -Case Pressure -Seconds 120 -CpuMask 3 -SwitchIntervals -Preview -KeepDevices -RequireNoHitch
```

`Limited` reads at 18.5 decimal MB/s and delays receipts by 10ms. `Pressure`
stops reading for six seconds every ten seconds (`--pause-every-ms=10000`),
with a 64KiB sender buffer by default. A six-second read pause necessarily
prevents a one-second receipt-gap guarantee; this case checks frame hitches,
bounded reservations and recovery. `-CpuMask 3` restricts the owned Editor to
two **logical processors**; `15` selects four. No system affinity is changed.
`-SwitchIntervals` submits an actual transient Slab scenario and attempts twelve
0.1/0.05 changes. `-RequireData` preserves its existing setting lock and failure
policy. `-WorktreeMap` reads a copy of the user's current map; otherwise the
291b4f9 map is extracted. Neither path saves the original map.

The fixture explicitly selects ML-X(80) FullSpec 576x56, GPU depth acquisition,
and PCD output. `-LegacyJson` additionally enables the old compatibility output.
`-LegacyAdmission` disables adaptive admission for comparison.
`-LegacyPolling` restores the previous worker waits **only** when a PCD-OPT
report is requested, to permit a controlled CPU comparison in the same binary.
It is a diagnostic switch, not a suggested production setting.

Reports contain source/binary fingerprints, one-second queues/reservations and
latencies, process CPU samples, receipt gaps, omission/overload counts, and
frames over 150ms with sensor/transport/Slab/Editor/GC section times. New runs
also write `sections.csv` (call counts and maximum section duration).
`run-faults.ps1` includes a ninth `bandwidth-admission` PIE case in addition to
the eight raw-worker/recovery cases. Adaptive PCD omissions are recorded as
explicit output failures for Slab reliability; accepted FIFO frames and all
wire bytes remain unchanged.

For field timing without changing actors, streams or scenario state, start
Editor with `-PcdOptReport="<absolute report directory>" -PcdOptObserveExisting
-PcdOptSeconds=300`, then start PIE normally. This opt-in mode records timing
after ten seconds and leaves Editor running. It never writes packet bodies or
credentials. It does not reproduce or prove the cause of an unavailable field
freeze by itself.
