import test from 'node:test';
import assert from 'node:assert/strict';
import net from 'node:net';
import { startServer, frame } from './server.mjs';

async function client(t, options = {}) {
  const events = [];
  const running = startServer({ port: 0, log: v => events.push(v), ...options });
  await running.ready;
  const socket = net.connect(running.server.address().port, '127.0.0.1');
  await new Promise(resolve => socket.once('connect',resolve));
  let wire = Buffer.alloc(0);
  socket.on('data', chunk => { wire = Buffer.concat([wire,chunk]); });
  socket.on('error', () => {});
  t.after(async () => { socket.destroy(); await running.close(); });
  return { socket, events, wire: () => wire, async until(predicate, ms = 1500) {
    const end = Date.now()+ms;
    while (!predicate() && Date.now()<end) await new Promise(resolve => setTimeout(resolve,10));
    assert.ok(predicate(), 'expected server event or wire frame within deadline');
  } };
}

test('fragmented/coalesced binary content-length, receipts, no sensitive logging', async t => {
  const c = await client(t);
  const connect = Buffer.from('CONNECT\naccept-version:1.2\nhost:test\nlogin:private-user\npasscode:private-value\nheart-beat:0,0\n\n\0');
  for (const byte of connect) c.socket.write(Buffer.from([byte]));
  await c.until(() => c.wire().includes(Buffer.from('CONNECTED')));
  const body = Buffer.from([1,0,2,10,0,3]);
  const send = frame('SEND', { 'content-length': body.length, receipt:'test-1' },body);
  c.socket.write(send.subarray(0,send.indexOf('\n\n')+2));
  await new Promise(resolve => setTimeout(resolve,20));
  c.socket.write(Buffer.concat([send.subarray(send.indexOf('\n\n')+2),frame('SEND', {receipt:'test-2'},Buffer.from('abc'))]));
  await c.until(() => c.wire().includes(Buffer.from('receipt-id:test-2')));
  c.socket.end();
  await c.until(() => c.events.some(e => e.event==='closed'));
  const closed = c.events.find(e => e.event==='closed');
  assert.equal(closed.sends,2); assert.equal(closed.bytes,9); assert.equal(closed.receipts,2);
  assert.ok(!JSON.stringify(c.events).includes('private'));
});
test('heartbeat negotiation and incoming heartbeat count', async t => {
  const c = await client(t,{ heartbeatMs:20, requireHeartbeatMs:0 });
  c.socket.write(frame('CONNECT', { 'heart-beat':'0,40' }));
  await c.until(() => c.events.some(e => e.event==='connected'));
  assert.equal(c.events.find(e => e.event==='connected').txIntervalMs,40);
  const before = c.wire().length;
  await c.until(() => c.wire().length > before);
});
test('fixed ERROR injection and reconnect capability', async t => {
  const c = await client(t,{errorAfterMs:30});
  c.socket.write(frame('CONNECT'));
  await c.until(() => c.wire().includes(Buffer.from('injected-broker-error')));
  await c.until(() => c.events.some(e => e.event==='closed'));
});
test('forced reset', async t => {
  const c = await client(t,{closeAfterMs:30});
  c.socket.write(frame('CONNECT'));
  await c.until(() => c.events.some(e => e.event==='closed'));
  assert.ok(c.events.some(e => e.event==='injected-reset'));
});
test('pause/resume and delayed receipt preserve a complete binary frame', async t => {
  const c = await client(t,{pauseMs:100,receiptDelayMs:70});
  c.socket.write(frame('CONNECT'));
  await c.until(() => c.events.some(e => e.event==='read-paused'));
  c.socket.write(frame('SEND',{'content-length':1024,receipt:'delayed'},Buffer.alloc(1024)));
  await c.until(() => c.events.some(e => e.event==='read-resumed'));
  await c.until(() => c.wire().includes(Buffer.from('receipt-id:delayed')));
});
test('size error before body allocation', async t => {
  const c = await client(t,{maxBodyBytes:10});
  c.socket.write(frame('CONNECT'));
  c.socket.write(Buffer.from('SEND\ncontent-length:11\n\n'));
  await c.until(() => c.wire().includes(Buffer.from('body-size-limit')));
});
test('subscription echo preserves embedded NUL body', async t => {
  const c = await client(t,{echo:true});
  c.socket.write(frame('CONNECT'));
  c.socket.write(frame('SUBSCRIBE',{id:'echo',destination:'topic.test'}));
  const body = Buffer.from([7,0,8]);
  c.socket.write(frame('SEND',{destination:'topic.test','content-length':3,receipt:'r'},body));
  await c.until(() => c.wire().includes(Buffer.from('MESSAGE')) && c.wire().includes(Buffer.from('receipt-id:r')));
  assert.ok(c.wire().includes(body));
});
test('paced reads cap sustained bytes independently of TCP chunks and record throughput', async t => {
  const rate = 1e6;
  const c = await client(t,{maxReadMbps:1, rttMs:20, heartbeatMs:0, requireHeartbeatMs:0});
  c.socket.write(frame('CONNECT'));
  await c.until(() => c.wire().includes(Buffer.from('CONNECTED')));
  const started = performance.now();
  c.socket.write(frame('SEND',{'content-length':2e6,receipt:'paced'},Buffer.alloc(2e6)));
  await c.until(() => c.wire().includes(Buffer.from('receipt-id:paced')),5000);
  const elapsed = (performance.now()-started)/1000;
  assert.ok(elapsed >= 1.95 && elapsed < 3.5, `2MB at 1MB/s plus RTT: ${elapsed}s`);
  const samples = c.events.filter(e => e.event==='sample');
  assert.ok(samples.length >= 2);
  assert.ok(samples.every(e => e.readMBps <= rate/1e6*1.03));
  assert.ok(samples.some(e => e.readMBps > .8));
});
test('paced read pause cannot be resumed by a rate timer; RTT is additive', async t => {
  const c = await client(t,{maxReadMbps:2,pauseMs:200,rttMs:80,receiptDelayMs:20});
  c.socket.write(frame('CONNECT'));
  await c.until(() => c.events.some(e => e.event==='read-paused'));
  const started = performance.now();
  c.socket.write(frame('SEND',{'content-length':1000,receipt:'after-pause'},Buffer.alloc(1000)));
  await new Promise(resolve => setTimeout(resolve,100));
  assert.ok(!c.wire().includes(Buffer.from('after-pause')));
  await c.until(() => c.wire().includes(Buffer.from('receipt-id:after-pause')));
  assert.ok(performance.now()-started >= 270);
});
test('18.5MB/s cap reaches configured production speed on large binary frames', async t => {
  const c = await client(t,{maxReadMbps:18.5,heartbeatMs:0,requireHeartbeatMs:0});
  c.socket.write(frame('CONNECT'));
  await c.until(() => c.wire().includes(Buffer.from('CONNECTED')));
  const started=performance.now();
  c.socket.write(frame('SEND',{'content-length':50e6,receipt:'large'},Buffer.alloc(50e6)));
  await c.until(() => c.wire().includes(Buffer.from('receipt-id:large')),6000);
  const rate=50e6/((performance.now()-started)/1000)/1e6;
  assert.ok(rate>17.5 && rate<18.8, `measured MB/s=${rate}`);
});
test('recurrent read pauses repeat on one live connection',async t=>{
  const c=await client(t,{pauseMs:60,pauseEveryMs:120,heartbeatMs:0,requireHeartbeatMs:0});
  c.socket.write(frame('CONNECT'));
  await c.until(()=>c.events.filter(e=>e.event==='read-resumed').length>=3,1200);
  assert.equal(c.events.filter(e=>e.event==='connected').length,1);
  assert.ok(c.events.filter(e=>e.event==='read-paused').length>=3);
});
