#!/usr/bin/env node

import fs from 'node:fs';
import path from 'node:path';

const args = new Map();
for (let i = 2; i < process.argv.length; i += 2) args.set(process.argv[i], process.argv[i + 1]);
const url = args.get('--url') ?? 'ws://127.0.0.1:61616';
const user = args.get('--user') ?? 'artemis';
const password = args.get('--password') ?? 'artemis';
const topics = (args.get('--topics') ?? 'topic.virtual.sensor.lidar.0,topic.virtual.sensor.camera.0,topic.virtual.sensor.export.0').split(',').filter(Boolean);
const ackTopic = args.get('--ack-topic') ?? '';
const timeoutSeconds = Number(args.get('--timeout') ?? '30');
const expectedPerTopic = Number(args.get('--count') ?? '1');
const selfTest = (args.get('--self-test') ?? 'false').toLowerCase() === 'true';
const output = args.get('--output') ?? path.resolve('Saved', 'Reports', `artemis_probe_${new Date().toISOString().replaceAll(/[:.]/g, '-')}.json`);
const messages = [];
const counts = new Map(topics.map(topic => [topic, 0]));
let buffer = '';
let connected = false;

function frame(command, headers = {}, body = '') {
  const lines = [command, ...Object.entries(headers).map(([key, value]) => `${key}:${String(value).replaceAll('\\', '\\\\').replaceAll(':', '\\c').replaceAll('\n', '\\n')}`), '', body];
  return `${lines.join('\n')}\0`;
}

function parseFrame(raw) {
  const split = raw.indexOf('\n\n');
  const headerText = split >= 0 ? raw.slice(0, split) : raw;
  const body = split >= 0 ? raw.slice(split + 2) : '';
  const lines = headerText.replaceAll('\r', '').split('\n');
  const command = lines.shift() ?? '';
  const headers = {};
  for (const line of lines) {
    const separator = line.indexOf(':');
    if (separator > 0) headers[line.slice(0, separator)] = line.slice(separator + 1).replaceAll('\\n', '\n').replaceAll('\\c', ':').replaceAll('\\\\', '\\');
  }
  return { command, headers, body };
}

function report(success, reason) {
  const result = {
    generatedUtc: new Date().toISOString(),
    success,
    reason,
    brokerUrl: url,
    topics,
    expectedPerTopic,
    counts: Object.fromEntries(counts),
    messages,
  };
  fs.mkdirSync(path.dirname(output), { recursive: true });
  fs.writeFileSync(output, JSON.stringify(result, null, 2), 'utf8');
  console.log(JSON.stringify(result, null, 2));
  console.error(`report=${output}`);
}

function hasAllExpected() {
  return [...counts.values()].every(count => count >= expectedPerTopic);
}

const socket = new WebSocket(url, ['v12.stomp']);
socket.addEventListener('open', () => {
  socket.send(frame('CONNECT', { 'accept-version': '1.2', host: 'localhost', login: user, passcode: password, 'heart-beat': '0,0' }));
});

socket.addEventListener('message', async event => {
  const chunk = typeof event.data === 'string' ? event.data : event.data instanceof Blob ? await event.data.text() : Buffer.from(event.data).toString('utf8');
  buffer += chunk;
  while (buffer.includes('\0')) {
    const end = buffer.indexOf('\0');
    const raw = buffer.slice(0, end).replace(/^\n+/, '');
    buffer = buffer.slice(end + 1);
    if (!raw.trim()) continue;
    const parsed = parseFrame(raw);
    if (parsed.command === 'CONNECTED') {
      connected = true;
      topics.forEach((topic, index) => socket.send(frame('SUBSCRIBE', { id: `probe-${index}`, destination: topic, ack: 'auto', 'subscription-type': 'MULTICAST' })));
	  if (selfTest) {
		setTimeout(() => topics.forEach((topic, index) => {
		  const requestId = `probe-${Date.now()}-${index}`;
		  const kind = index === 0 ? 'lidar-stream' : index === 1 ? 'camera-stream' : 'pointcloud-stream';
		  socket.send(frame('SEND', {
			destination: topic,
			'destination-type': 'MULTICAST',
			'content-type': 'application/json',
			'x-request-id': requestId,
			'x-sensor-id': `PROBE-${index}`,
			'x-sensor-type': index === 1 ? 'camera' : 'lidar',
			'x-data-kind': kind,
			'x-frame-id': index + 1,
		  }, JSON.stringify({ schema: index === 0 ? 'virtual-lidar.v1' : index === 1 ? 'virtual-camera.v1' : 'virtual-pointcloud.v1', probe: true })));
		}), 250);
	  }
      continue;
    }
    if (parsed.command === 'MESSAGE') {
      const destination = parsed.headers.destination ?? '';
      if (counts.has(destination)) counts.set(destination, counts.get(destination) + 1);
      let schema = '';
      try { schema = JSON.parse(parsed.body).schema ?? JSON.parse(parsed.body).schemaVersion ?? ''; } catch {}
      const entry = {
        receivedUtc: new Date().toISOString(),
        destination,
        requestId: parsed.headers['x-request-id'] ?? '',
        sensorId: parsed.headers['x-sensor-id'] ?? '',
        sensorType: parsed.headers['x-sensor-type'] ?? '',
        dataKind: parsed.headers['x-data-kind'] ?? '',
        frameId: parsed.headers['x-frame-id'] ?? '',
        contentType: parsed.headers['content-type'] ?? '',
        bytes: Buffer.byteLength(parsed.body, 'utf8'),
        schema,
        bodyPreview: parsed.body.slice(0, 180),
      };
      messages.push(entry);
      console.error(`[MESSAGE] ${destination} request=${entry.requestId} sensor=${entry.sensorId} frame=${entry.frameId} bytes=${entry.bytes} schema=${entry.schema}`);
      if (ackTopic && entry.requestId) {
        socket.send(frame('SEND', { destination: ackTopic, 'destination-type': 'MULTICAST', 'content-type': 'application/json', 'x-request-id': entry.requestId }, JSON.stringify({ requestId: entry.requestId, processed: true, source: 'ma0t10-stomp-probe' })));
      }
      if (hasAllExpected()) {
        report(true, 'expected messages received from every topic');
        socket.close();
      }
      continue;
    }
    if (parsed.command === 'ERROR') {
      report(false, `broker error: ${parsed.headers.message ?? parsed.body}`);
      process.exitCode = 2;
      socket.close();
    }
  }
});

socket.addEventListener('error', event => {
  if (!connected) {
    report(false, `websocket connection error: ${event.message ?? 'unknown'}`);
    process.exitCode = 3;
  }
});

socket.addEventListener('close', () => {
  if (!hasAllExpected() && process.exitCode == null) {
    report(false, connected ? 'socket closed before expected messages arrived' : 'socket closed before STOMP CONNECTED');
    process.exitCode = 4;
  }
});

setTimeout(() => {
  if (!hasAllExpected()) {
    report(false, `timeout after ${timeoutSeconds}s`);
    process.exitCode = 5;
    socket.close();
  }
}, timeoutSeconds * 1000).unref();
