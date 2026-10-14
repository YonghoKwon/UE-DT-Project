// Local STOMP 1.2 fault harness. Never log headers, credentials or body bytes.
// Protocol: https://stomp.github.io/stomp-specification-1.2.html
import net from 'node:net';
import { pathToFileURL } from 'node:url';

const escape = s => s.replace(/\\/g, '\\\\').replace(/\r/g, '\\r').replace(/\n/g, '\\n').replace(/:/g, '\\c');
const unescape = s => s.replace(/\\([\\rnc])/g, (_, c) => ({ '\\': '\\', r: '\r', n: '\n', c: ':' })[c]);
export function frame(command, headers = {}, body = Buffer.alloc(0)) {
  return Buffer.concat([Buffer.from(command + '\n' + Object.entries(headers).map(([k,v]) => escape(k) + ':' + escape(String(v)) + '\n').join('') + '\n'), body, Buffer.from([0])]);
}

export function startServer(options = {}) {
  const o = { host: '127.0.0.1', port: 61618, heartbeatMs: 1000, requireHeartbeatMs: 1000,
    pauseAfterMs: 0, pauseMs: 0, readBytesPerSecond: 0, maxReadMbps: 0, rttMs: 0, receiptDelayMs: 0,
    errorAfterMs: 0, closeAfterMs: 0, silentAfterMs: 0, maxBodyBytes: 64 * 1024 * 1024,
    echo: false, enforceHeartbeat: true, log: value => process.stdout.write(JSON.stringify(value) + '\n'), ...options };
  // MB/s is decimal megabytes (not megabits). Bound the burst to 20ms;
  // per-chunk sleeps otherwise turn Windows timer granularity into link speed.
  const readRate = o.maxReadMbps ? o.maxReadMbps * 1e6 : o.readBytesPerSecond;
  const sockets = new Set();
  let connectionId = 0;
  const server = net.createServer(socket => {
    sockets.add(socket);
    socket.setNoDelay(true);
    const id = ++connectionId;
    const started = performance.now();
    const timers = new Set();
    const subscriptions = new Map();
    let connected = false, header = Buffer.alloc(0), current = null;
    let lastRx = performance.now(), lastTx = performance.now(), txInterval = 0, rxInterval = 0;
    let sends = 0, bytes = 0, receipts = 0, rxHeartbeats = 0, txHeartbeats = 0, reason = 'peer-close';
    let readPaused = false, wireBytes = 0, tokens = 0, tokenTime = performance.now();
    let sampledBytes = 0, sampleTime = started;
    const log = (event, data = {}) => o.log({ utc: new Date().toISOString(), connection: id,
      elapsedMs: Math.round(performance.now() - started), event, ...data });
    const later = (callback, ms) => { const timer = setTimeout(() => { timers.delete(timer); if (!socket.destroyed) callback(); }, ms); timers.add(timer); };
    const write = data => { if (socket.destroyed) return; lastTx = performance.now(); socket.write(data); };
    const fail = code => { reason = code; log('error-injected', { code }); write(frame('ERROR', { message: code, 'content-length': Buffer.byteLength(code) }, Buffer.from(code))); socket.end(); };
    const receipt = value => { if (!value) return; later(() => { ++receipts; write(frame('RECEIPT', { 'receipt-id': value })); }, o.receiptDelayMs + o.rttMs); };
    const complete = f => {
      if (f.command === 'CONNECT' || f.command === 'STOMP') {
        if (connected) return fail('duplicate-connect');
        connected = true;
        const [cx,cy] = (f.headers['heart-beat'] || '0,0').split(',').map(Number);
        if (![cx,cy].every(n => Number.isSafeInteger(n) && n >= 0)) return fail('invalid-heartbeat');
        txInterval = o.heartbeatMs && cy ? Math.max(o.heartbeatMs, cy) : 0;
        rxInterval = o.requireHeartbeatMs && cx ? Math.max(o.requireHeartbeatMs, cx) : 0;
        write(frame('CONNECTED', { version: '1.2', 'heart-beat': `${o.heartbeatMs},${o.requireHeartbeatMs}` }));
        log('connected', { txIntervalMs: txInterval, rxIntervalMs: rxInterval, readBytesPerSecond: readRate, rttMs: o.rttMs });
        if (o.pauseMs) later(() => { readPaused = true; log('read-paused'); socket.pause(); later(() => { readPaused = false; tokenTime = performance.now(); tokens = 0; log('read-resumed'); if (!readRate) socket.resume(); }, o.pauseMs); }, o.pauseAfterMs);
        if (o.errorAfterMs) later(() => fail('injected-broker-error'), o.errorAfterMs);
        if (o.closeAfterMs) later(() => { reason = 'injected-reset'; log(reason); socket.resetAndDestroy(); }, o.closeAfterMs);
        return;
      }
      if (!connected) return fail('connect-required');
      if (f.command === 'SEND') {
        ++sends; bytes += f.length;
        if (o.echo) for (const [subscription,destination] of subscriptions) if (destination === f.headers.destination) {
          const h = { ...f.headers, subscription, 'message-id': `${id}-${sends}`, 'content-length': f.length };
          delete h.receipt;
          write(frame('MESSAGE', h, Buffer.concat(f.body)));
        }
      } else if (f.command === 'SUBSCRIBE') subscriptions.set(f.headers.id, f.headers.destination);
      else if (f.command === 'UNSUBSCRIBE') subscriptions.delete(f.headers.id);
      receipt(f.headers.receipt);
      if (f.command === 'DISCONNECT') { reason = 'disconnect'; later(() => socket.end(), o.receiptDelayMs + 10); }
    };
    const parse = chunk => {
      let offset = 0;
      while (offset < chunk.length && !socket.destroyed) {
        if (!current) {
          if (!header.length && (chunk[offset] === 10 || chunk[offset] === 13)) { ++rxHeartbeats; ++offset; continue; }
          const lf = chunk.indexOf(10, offset);
          const end = lf < 0 ? chunk.length : lf + 1;
          header = Buffer.concat([header, chunk.subarray(offset, end)]); offset = end;
          if (header.length > 64 * 1024) return fail('header-size-limit');
          if (lf < 0 || !(header.at(-2) === 10 || (header.at(-2) === 13 && header.at(-3) === 10))) continue;
          const lines = header.toString('utf8').split(/\r?\n/);
          const command = lines.shift(), headers = {};
          for (const line of lines) {
            if (!line) continue;
            const colon = line.indexOf(':');
            if (colon < 1) return fail('invalid-header');
            const decode = command === 'CONNECT' || command === 'STOMP' ? v => v : unescape;
            const key = decode(line.slice(0,colon));
            if (!(key in headers)) headers[key] = decode(line.slice(colon+1));
          }
          const length = headers['content-length'] === undefined ? null : Number(headers['content-length']);
          if (length !== null && (!Number.isSafeInteger(length) || length < 0 || length > o.maxBodyBytes)) return fail('body-size-limit');
          current = { command, headers, length: length ?? 0, remaining: length, body: [] };
          header = Buffer.alloc(0);
        }
        if (offset === chunk.length) continue;
        const f = current;
        if (f.remaining === null) {
          const nul = chunk.indexOf(0,offset), end = nul < 0 ? chunk.length : nul;
          const body = chunk.subarray(offset,end); f.length += body.length;
          if (f.length > o.maxBodyBytes) return fail('body-size-limit');
          if (o.echo) f.body.push(body);
          offset = end;
          if (nul < 0) continue;
        } else if (f.remaining > 0) {
          const size = Math.min(f.remaining,chunk.length-offset);
          if (o.echo) f.body.push(chunk.subarray(offset,offset+size));
          offset += size; f.remaining -= size;
          if (f.remaining || offset === chunk.length) continue;
        }
        if (chunk[offset++] !== 0) return fail('missing-terminator');
        current = null; complete(f);
      }
    };
    const receive = chunk => {
      wireBytes += chunk.length;
      lastRx = performance.now();
      parse(chunk);
    };
    let readTick;
    if (readRate) {
      // Read only the earned bytes, so chunk sizes and coalescing cannot change
      // the cap. readable-mode keeps Node's buffering bounded under TCP load.
      const drain = () => {
        const now = performance.now();
        tokens = Math.min(readRate * .02, tokens + (now - tokenTime) / 1000 * readRate);
        tokenTime = now;
        if (readPaused || socket.destroyed) return;
        while (tokens >= 1 && socket.readableLength) {
          const chunk = socket.read(Math.min(Math.floor(tokens), socket.readableLength));
          if (!chunk) break;
          tokens -= chunk.length; receive(chunk);
          if (readPaused || socket.destroyed) break;
        }
      };
      readTick = setInterval(drain, 5);
      socket.on('readable', drain);
    } else socket.on('data', receive);
    const tick = setInterval(() => {
      const now = performance.now();
      if (!connected) return;
      if (txInterval && now - lastTx >= txInterval && (!o.silentAfterMs || now-started < o.silentAfterMs)) { ++txHeartbeats; write(Buffer.from('\n')); }
      // A deliberately paused reader cannot use its own missing reads as evidence of peer death.
      if (o.enforceHeartbeat && rxInterval && !readPaused && now-lastRx > rxInterval*3) {
        reason = 'client-heartbeat-timeout'; log(reason, { silenceMs: Math.round(now-lastRx) }); socket.end();
      }
    }, 20);
    const summary = setInterval(() => {
      const now = performance.now();
      log('sample', { sends, bytes, receipts, rxHeartbeats, txHeartbeats, wireBytes,
        readMBps: (wireBytes - sampledBytes) / (now - sampleTime) / 1000,
        bufferedReadBytes: socket.readableLength, partialBodyBytes: current ? current.length - (current.remaining ?? 0) : 0 });
      sampledBytes = wireBytes; sampleTime = now;
    }, 1000);
    socket.on('error', error => { reason = error.code || 'socket-error'; log('socket-error', { code: reason }); });
    socket.on('close', () => { sockets.delete(socket); clearInterval(readTick); clearInterval(tick); clearInterval(summary); for (const timer of timers) clearTimeout(timer); log('closed', { reason, sends, bytes, receipts, rxHeartbeats, txHeartbeats, wireBytes }); });
    log('accepted');
  });
  server.on('listening', () => o.log({ utc: new Date().toISOString(), event: 'listening', ...server.address() }));
  return { server, ready: new Promise((resolve,reject) => { server.once('listening',resolve); server.once('error',reject); server.listen(o.port,o.host); }),
    close: () => new Promise(resolve => { for (const socket of sockets) socket.destroy(); server.close(resolve); }) };
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  const options = {};
  const names = { port:'port', 'heartbeat-ms':'heartbeatMs', 'require-heartbeat-ms':'requireHeartbeatMs', 'pause-after-ms':'pauseAfterMs', 'pause-ms':'pauseMs', 'read-bytes-per-second':'readBytesPerSecond', 'max-read-mbps':'maxReadMbps', 'rtt-ms':'rttMs', 'receipt-delay-ms':'receiptDelayMs', 'error-after-ms':'errorAfterMs', 'close-after-ms':'closeAfterMs', 'silent-after-ms':'silentAfterMs', 'max-body-bytes':'maxBodyBytes' };
  for (const arg of process.argv.slice(2)) {
    if (arg === '--echo') options.echo = true;
    else if (arg === '--no-enforce-heartbeat') options.enforceHeartbeat = false;
    else {
      const match = /^--([^=]+)=(\d+(?:\.\d+)?)$/.exec(arg);
      if (!match || !names[match[1]]) throw new Error('Unknown option; see README.md');
      if (match[1] !== 'max-read-mbps' && !Number.isSafeInteger(Number(match[2]))) throw new Error('Integer option required');
      options[names[match[1]]] = Number(match[2]);
    }
  }
  const running = startServer(options);
  await running.ready;
  for (const signal of ['SIGINT','SIGTERM']) process.on(signal, async () => { await running.close(); process.exit(0); });
}
