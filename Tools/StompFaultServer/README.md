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
| receipt-delay-ms | 0 | Delay receipts without stopping reads |
| error-after-ms | 0 | Send fixed, non-secret ERROR and close |
| close-after-ms | 0 | Force TCP reset |
| silent-after-ms | 0 | Stop server heartbeats |
| max-body-bytes | 67108864 | Reject larger content-length |

`--echo` additionally sends MESSAGE to matching subscriptions for loopback
validation (retains one bounded body); ordinary receipt-only mode discards body
chunks as they arrive. `--no-enforce-heartbeat` disables client silence checks.
Injected faults recur on each connection, allowing reconnect testing.

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
