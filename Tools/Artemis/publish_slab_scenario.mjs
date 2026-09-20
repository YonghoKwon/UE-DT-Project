#!/usr/bin/env node
// Synthetic test publisher. Receipts mean broker acceptance, not playback completion.
import net from 'node:net';
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';

const args = new Map();
for (let i = 2; i < process.argv.length; i += 2) args.set(process.argv[i], process.argv[i + 1]);
const topic = args.get('--topic') ?? 'topic.cep.output.0';
const id = args.get('--uuid') ?? crypto.randomUUID();
const anchors = [
  { frame_no: 0, left: -.2, right: .2, pos: 0, y: .1, mov: 1210 },
  { frame_no: 1, left: -.185, right: .185, pos: 1.55, y: .085, mov: 1212 },
  { frame_no: 580, left: -5, right: 5, pos: 909, y: 9.9, mov: 2120 },
];
const rows = Array.from({ length: 600 }, (_, i) => {
  const a = i <= 1 ? anchors[0] : anchors[1];
  const b = i <= 1 ? anchors[1] : anchors[2];
  const t = Math.min(1, (i - a.frame_no) / (b.frame_no - a.frame_no));
  const v = key => a[key] + (b[key] - a[key]) * t;
  return { frame_no: i, mtl_no: 'SQ83521 047', slab_thk: 250, slab_wth: 1100,
    slab_len: 10830, slab_wt: 23290, left_skew_angle: v('left'), right_skew_angle: v('right'),
    mov_pos: v('mov'), center_x: v('pos'), center_y: v('y'), elapsed_sec: i * .05,
    is_meandering: i >= 580 };
});
const meta = { scenario: 'Continuous Casting Right Meandering - 30s', duration_sec: 30, sample_period_sec: .05 };
if (args.get('--without-uuid') !== 'true') meta.UUID = id;
const generated = { CREATE_TIMESTAMP: '20260727174504137', MESSAGE_ID: 'IFactory-agent', DATA_MAP: rows, _meta: meta };
const body = args.has('--input') ? fs.readFileSync(args.get('--input')) : Buffer.from(JSON.stringify(generated));
const output = args.get('--output') ?? path.resolve('Saved/Reports/Slab/synthetic_30s.json');
fs.mkdirSync(path.dirname(output), { recursive: true });
fs.writeFileSync(output, body);
if (args.get('--generate-only') === 'true') {
  console.log(JSON.stringify({ file: output, bytes: body.length, uuid: meta.UUID ?? null, frames: 600, end: 30 }));
} else {
  const socket = net.createConnection({ host: args.get('--host') ?? '127.0.0.1', port: Number(args.get('--port') ?? 61616) });
  const receipt = `slab-${crypto.randomUUID()}`;
  let buffer = Buffer.alloc(0), sent = false, finished = false;
  const timeout = setTimeout(() => finish(new Error('STOMP connection/receipt timeout (15s)')), 15000);
  function finish(error) {
    if (finished) return;
    finished = true; clearTimeout(timeout); socket.destroy();
    if (error) { console.error(error.message); process.exitCode = 1; }
    else console.log(JSON.stringify({ brokerAccepted: true, topic, bytes: body.length, receipt, file: output,
      note: 'Confirm TC/Slab progress separately in Unreal.' }));
  }
  function escape(value) { return String(value).replaceAll('\\', '\\\\').replaceAll('\r', '\\r').replaceAll('\n', '\\n').replaceAll(':', '\\c'); }
  function send(command, headers, bytes = Buffer.alloc(0)) {
    const header = Buffer.from([command, ...Object.entries(headers).map(([k, v]) => `${k}:${escape(v)}`), '', ''].join('\n'));
    socket.write(header); if (bytes.length) socket.write(bytes); socket.write(Buffer.from([0]));
  }
  socket.on('connect', () => send('STOMP', { 'accept-version': '1.2', host: 'localhost',
    login: process.env.ARTEMIS_USER ?? 'artemis', passcode: process.env.ARTEMIS_PASSWORD ?? 'artemis', 'heart-beat': '0,0' }));
  socket.on('error', finish);
  socket.on('data', bytes => {
    buffer = Buffer.concat([buffer, bytes]);
    for (;;) {
      const end = buffer.indexOf(0); if (end < 0) break;
      const frame = buffer.subarray(0, end).toString('utf8').trimStart(); buffer = buffer.subarray(end + 1);
      if (frame.startsWith('CONNECTED\n') && !sent) {
        sent = true;
        send('SEND', { destination: topic, 'destination-type': 'MULTICAST', 'content-type': 'application/json',
          'content-length': body.length, persistent: 'false', receipt }, body);
      } else if (frame.startsWith('RECEIPT\n') && frame.includes(`receipt-id:${receipt}`)) finish();
      else if (frame.startsWith('ERROR\n')) finish(new Error('Artemis rejected synthetic scenario; inspect broker log'));
    }
  });
}
