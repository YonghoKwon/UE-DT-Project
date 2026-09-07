#!/usr/bin/env node

import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';

const args = new Map();
for (let i = 2; i < process.argv.length; i += 2) args.set(process.argv[i], process.argv[i + 1]);
const url = args.get('--url') ?? 'ws://127.0.0.1:61616';
const user = args.get('--user') ?? 'artemis';
const password = args.get('--password') ?? 'artemis';
const topics = (args.get('--topics') ?? 'topic.virtual.sensor.lidar.0,topic.virtual.sensor.camera.0,topic.virtual.sensor.export.0').split(',').filter(Boolean);
const ackTopic = args.get('--ack-topic') ?? '';
const timeoutSeconds = Number(args.get('--timeout') ?? '30');
const expectedPerTopic = Number(args.get('--count') ?? '1');
const durationSeconds = Number(args.get('--duration') ?? '0');
const warmupSeconds = Number(args.get('--warmup') ?? '0');
const requireContiguousPcd = (args.get('--require-contiguous-pcd') ?? 'false').toLowerCase() === 'true';
const requiredSlabRuns = Number(args.get('--slab-runs') ?? '0');
const slabRuns = new Map();
let firstPcdSaved = false;
const selfTest = (args.get('--self-test') ?? 'false').toLowerCase() === 'true';
const selfTestPoints = Math.max(1, Number(args.get('--self-test-points') ?? '1'));
const output = args.get('--output') ?? path.resolve('Saved', 'Reports', `artemis_probe_${new Date().toISOString().replaceAll(/[:.]/g, '-')}.json`);
const messages = [];
const counts = new Map(topics.map(topic => [topic, 0]));
const topicMetrics = new Map(topics.map(topic => [topic, {
  validCount: 0,
  invalidCount: 0,
  totalBytes: 0,
  firstReceivedMs: 0,
  lastReceivedMs: 0,
  firstFrameId: null,
  lastFrameId: null,
  frameGaps: 0,
  duplicates: 0,
}]));
let buffer = Buffer.alloc(0);
let connected = false;
let measurementStartedMs = 0;
let measurementTimer = null;
let warmupTimer = null;
let warmupStarted = false;
let finished = false;
let heartbeatTimer = null;

function frame(command, headers = {}, body = '') {
  const lines = [command, ...Object.entries(headers).map(([key, value]) => `${key}:${String(value).replaceAll('\\', '\\\\').replaceAll(':', '\\c').replaceAll('\n', '\\n')}`), '', body];
  return `${lines.join('\n')}\0`;
}

function binaryFrame(command, headers, body) {
  const normalized = { ...headers, 'content-length': body.length };
  const headerText = [command, ...Object.entries(normalized).map(([key, value]) => `${key}:${String(value).replaceAll('\\', '\\\\').replaceAll(':', '\\c').replaceAll('\n', '\\n')}`), '', ''].join('\n');
  return Buffer.concat([Buffer.from(headerText, 'utf8'), body, Buffer.from([0])]);
}

function buildSelfTestBinaryPcd(pointCount = 1) {
  const header = Buffer.from([
    '# .PCD v0.7 - Point Cloud Data file format',
    'VERSION 0.7',
    'FIELDS x y z intensity ring horizontal_index return_index return_count time_offset_ns validity confidence',
    'SIZE 4 4 4 2 2 2 1 1 8 1 4',
    'TYPE F F F U U U U U I U F',
    'COUNT 1 1 1 1 1 1 1 1 1 1 1',
    `WIDTH ${pointCount}`,
    'HEIGHT 1',
    'VIEWPOINT 0 0 0 1 0 0 0',
    `POINTS ${pointCount}`,
    'DATA binary',
    '',
  ].join('\n'), 'ascii');
  const points = Buffer.alloc(pointCount * 33);
  for (let index = 0; index < pointCount; ++index) {
    const offset = index * 33;
    points.writeFloatLE(1.0 + index * 0.001, offset);
    points.writeFloatLE(2.0, offset + 4);
    points.writeFloatLE(3.0, offset + 8);
    points.writeUInt16LE(32768, offset + 12);
    points.writeUInt16LE(index % 56, offset + 14);
    points.writeUInt16LE(index % 576, offset + 16);
    points.writeUInt8(0, offset + 18);
    points.writeUInt8(1, offset + 19);
    points.writeBigInt64LE(BigInt(index * 1000), offset + 20);
    points.writeUInt8(1, offset + 28);
    points.writeFloatLE(0.9, offset + 29);
  }
  return Buffer.concat([header, points]);
}

function parseFrame(rawHeader, body) {
  const headerText = rawHeader.toString('utf8');
  const lines = headerText.replaceAll('\r', '').split('\n');
  const command = lines.shift() ?? '';
  const headers = {};
  for (const line of lines) {
    const separator = line.indexOf(':');
    if (separator > 0) headers[line.slice(0, separator)] = line.slice(separator + 1).replaceAll('\\n', '\n').replaceAll('\\c', ':').replaceAll('\\\\', '\\');
  }
  return { command, headers, body };
}

function takeNextFrame() {
  while (buffer.length > 0 && (buffer[0] === 0 || buffer[0] === 10 || buffer[0] === 13)) buffer = buffer.subarray(1);
  const delimiter = Buffer.from('\n\n');
  const headerEnd = buffer.indexOf(delimiter);
  if (headerEnd < 0) return null;
  const header = buffer.subarray(0, headerEnd);
  const headerLines = header.toString('utf8').replaceAll('\r', '').split('\n');
  const headers = {};
  for (const line of headerLines.slice(1)) {
    const separator = line.indexOf(':');
    if (separator > 0) headers[line.slice(0, separator)] = line.slice(separator + 1).replaceAll('\\n', '\n').replaceAll('\\c', ':').replaceAll('\\\\', '\\');
  }
  const bodyStart = headerEnd + delimiter.length;
  const declaredLength = headers['content-length'] == null ? null : Number(headers['content-length']);
  let bodyEnd;
  if (Number.isFinite(declaredLength)) {
    bodyEnd = bodyStart + declaredLength;
    if (buffer.length < bodyEnd + 1) return null;
  } else {
    bodyEnd = buffer.indexOf(0, bodyStart);
    if (bodyEnd < 0) return null;
  }
  const body = Buffer.from(buffer.subarray(bodyStart, bodyEnd));
  const terminatorIndex = bodyEnd;
  if (buffer[terminatorIndex] !== 0) throw new Error('STOMP frame is missing its NUL terminator');
  buffer = buffer.subarray(terminatorIndex + 1);
  return parseFrame(header, body);
}

function validateBinaryPcd(headers, body) {
  const marker = Buffer.from('DATA binary\n');
  const payloadOffset = body.indexOf(marker) + marker.length;
  const headerText = payloadOffset >= marker.length ? body.subarray(0, payloadOffset).toString('utf8') : '';
  const pointCount = Number(headers['x-point-count'] ?? '-1');
  const checksum = crypto.createHash('sha1').update(body).digest('hex');
  const checks = {
    schema: headers.schema === 'virtual-pointcloud.pcd.v1',
    contentType: headers['content-type'] === 'application/vnd.pcd',
    signature: body.subarray(0, 11).toString('ascii') === '# .PCD v0.7',
    fields: headerText.includes('FIELDS x y z intensity ring horizontal_index return_index return_count time_offset_ns validity confidence'),
    pointCount: Number.isInteger(pointCount) && pointCount >= 0 && headerText.includes(`POINTS ${pointCount}`),
    marker: payloadOffset >= marker.length,
    bodyLength: body.length === payloadOffset + pointCount * 33,
    checksum: checksum.toLowerCase() === String(headers['x-checksum-sha1'] ?? headers.checksum ?? '').toLowerCase(),
  };
  const metadataLines = headerText.split('\n').filter(line => line.startsWith('# MA0T10_META '));
  if (metadataLines.length || requiredSlabRuns > 0) {
    try {
      const meta = JSON.parse(metadataLines[0]?.slice(14));
      checks.embeddedMetadata = metadataLines.length === 1 && meta.schema === 'virtual-pointcloud.context.v1' &&
        meta.sensor_id === (headers['sensor-id'] ?? headers['x-sensor-id']) &&
        meta.sensor_frame_id === (headers['frame-id'] ?? headers['x-frame-id']);
      if (requiredSlabRuns > 0) checks.embeddedSlab = meta.run_uuid === headers['x-run-uuid'] &&
        meta.mtl_no === headers['x-mtl-no'] && meta.frame_no === headers['x-slab-frame-no'] &&
        Number(meta.elapsed_sec) === Number(headers['x-slab-elapsed-sec']);
    } catch { checks.embeddedMetadata = false; }
  }
  const failedChecks = Object.entries(checks).filter(([, passed]) => !passed).map(([name]) => name);
  return { valid: failedChecks.length === 0, pointCount, checksum, payloadOffset, failedChecks };
}

function report(success, reason) {
  if (finished) return;
  finished = true;
  if (measurementTimer) clearTimeout(measurementTimer);
  if (warmupTimer) clearTimeout(warmupTimer);
  if (heartbeatTimer) clearInterval(heartbeatTimer);
  const finishedMs = Date.now();
  const measuredSeconds = measurementStartedMs > 0 ? Math.max(0.001, (finishedMs - measurementStartedMs) / 1000) : 0;
  const metrics = Object.fromEntries([...topicMetrics.entries()].map(([topic, value]) => [topic, {
    ...value,
    receiveHz: measuredSeconds > 0 ? value.validCount / measuredSeconds : 0,
    megabytesPerSecond: measuredSeconds > 0 ? value.totalBytes / measuredSeconds / 1024 / 1024 : 0,
  }]));
  const result = {
    generatedUtc: new Date().toISOString(),
    success,
    reason,
    brokerUrl: url,
    topics,
    expectedPerTopic,
    durationSeconds,
    warmupSeconds,
    measuredSeconds,
    requireContiguousPcd,
    counts: Object.fromEntries(counts),
    metrics,
    slabRuns: Object.fromEntries(slabRuns),
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

function finishDurationMeasurement(socket) {
  const pointCloudTopic = topics.find(topic => topic.includes('export')) ?? topics.at(-1);
  const pointCloudMetrics = pointCloudTopic ? topicMetrics.get(pointCloudTopic) : null;
  const hasData = [...topicMetrics.values()].every(value => value.validCount > 0);
  const contiguous = !requireContiguousPcd || !pointCloudMetrics ||
    (pointCloudMetrics.frameGaps === 0 && pointCloudMetrics.duplicates === 0 && pointCloudMetrics.invalidCount === 0);
  const slabValid = requiredSlabRuns === 0 || (slabRuns.size === requiredSlabRuns && [...slabRuns.values()].every(run => topics.every(topic => {
    const m = run[topic]; return m && m.invalid === 0 && m.gaps === 0 && m.duplicates === 0 && m.count / 30 >= (topic.includes('camera') ? 29 : 19);
  })));
  report(hasData && contiguous && slabValid,
    !hasData ? 'duration elapsed before every topic produced a valid message'
      : !slabValid ? 'Slab session correlation/rate validation failed'
      : !contiguous ? 'binary PCD FrameId continuity validation failed'
        : `duration measurement completed (${durationSeconds}s)`);
  socket.close();
}

function beginDurationMeasurement(socket) {
  measurementStartedMs = Date.now();
  measurementTimer = setTimeout(() => finishDurationMeasurement(socket), durationSeconds * 1000);
}

function updateMetrics(destination, entry) {
  const metric = topicMetrics.get(destination);
  if (!metric) return;
  const now = Date.now();
  if (metric.firstReceivedMs === 0) metric.firstReceivedMs = now;
  metric.lastReceivedMs = now;
  metric.totalBytes += entry.bytes;
  if (!entry.valid) {
    metric.invalidCount += 1;
    return;
  }
  metric.validCount += 1;
  const frameId = Number(entry.frameId);
  if (entry.runId && metric.lastRun !== entry.runId) { metric.lastFrameId = null; metric.lastRun = entry.runId; }
  if (Number.isSafeInteger(frameId)) {
    if (metric.firstFrameId == null) metric.firstFrameId = frameId;
    if (metric.lastFrameId != null) {
      if (frameId <= metric.lastFrameId) metric.duplicates += 1;
      else if (frameId > metric.lastFrameId + 1) metric.frameGaps += frameId - metric.lastFrameId - 1;
    }
    metric.lastFrameId = Math.max(metric.lastFrameId ?? frameId, frameId);
  }
}

const socket = new WebSocket(url, ['v12.stomp']);
socket.addEventListener('open', () => {
  socket.send(frame('CONNECT', { 'accept-version': '1.2', host: 'localhost', login: user, passcode: password, 'heart-beat': '10000,10000' }));
});

socket.addEventListener('message', async event => {
	const chunk = typeof event.data === 'string'
		? Buffer.from(event.data, 'utf8')
		: event.data instanceof Blob
			? Buffer.from(await event.data.arrayBuffer())
			: Buffer.from(event.data);
  buffer = Buffer.concat([buffer, chunk]);
  while (true) {
	const parsed = takeNextFrame();
	if (!parsed) break;
    if (parsed.command === 'CONNECTED') {
      connected = true;
      if (!heartbeatTimer) heartbeatTimer = setInterval(() => {
        if (socket.readyState === WebSocket.OPEN) socket.send('\n');
      }, 10000);
      topics.forEach((topic, index) => socket.send(frame('SUBSCRIBE', { id: `probe-${index}`, destination: topic, ack: 'auto', 'subscription-type': 'MULTICAST' })));
	  if (selfTest) {
		setTimeout(() => topics.forEach((topic, index) => {
		 for (let messageIndex = 0; messageIndex < expectedPerTopic; ++messageIndex) {
		  const requestId = `probe-${Date.now()}-${index}-${messageIndex}`;
		  const kind = index === 0 ? 'lidar-stream' : index === 1 ? 'camera-stream' : 'pointcloud-stream';
		  if (index === 2) {
			const pcd = buildSelfTestBinaryPcd(selfTestPoints);
			const checksum = crypto.createHash('sha1').update(pcd).digest('hex');
			socket.send(binaryFrame('SEND', {
			  destination: topic,
			  'destination-type': 'MULTICAST',
			  schema: 'virtual-pointcloud.pcd.v1',
			  'content-type': 'application/vnd.pcd',
			  'x-request-id': requestId,
			  'x-sensor-id': 'PROBE-2',
			  'x-sensor-type': 'lidar',
			  'x-data-kind': kind,
			  'x-frame-id': messageIndex + 1,
			  'x-utc': new Date().toISOString(),
			  'x-point-count': selfTestPoints,
			  'x-source-point-count': selfTestPoints,
			  'x-filter-revision': 0,
			  'x-acquisition-profile': 'iyobot-mlx80-native',
			  'x-checksum-sha1': checksum,
			}, pcd));
			continue;
		  }
		  socket.send(frame('SEND', {
			destination: topic,
			'destination-type': 'MULTICAST',
			'content-type': 'application/json',
			'x-request-id': requestId,
			'x-sensor-id': `PROBE-${index}`,
			'x-sensor-type': index === 1 ? 'camera' : 'lidar',
			'x-data-kind': kind,
			'x-frame-id': messageIndex + 1,
		  }, JSON.stringify({ schema: index === 0 ? 'virtual-lidar.v1' : index === 1 ? 'virtual-camera.v1' : 'virtual-pointcloud.v1', probe: true })));
		 }
		}), 250);
	  }
      continue;
    }
    if (parsed.command === 'MESSAGE') {
      const destination = parsed.headers.destination ?? '';
      const isBinaryPcd = parsed.headers.schema === 'virtual-pointcloud.pcd.v1' || parsed.headers['content-type'] === 'application/vnd.pcd';
	  const isCameraJpeg = parsed.headers.schema === 'virtual-camera.jpeg.v1' || parsed.headers['content-type'] === 'image/jpeg';
	  const isLidarTelemetry = parsed.headers.schema === 'virtual-lidar.telemetry.v1';
      const pcdValidation = isBinaryPcd ? validateBinaryPcd(parsed.headers, parsed.body) : null;
      let schema = '';
	  if (isBinaryPcd || isCameraJpeg || isLidarTelemetry) schema = parsed.headers.schema ?? '';
	  else {
		try { const json = JSON.parse(parsed.body.toString('utf8')); schema = json.schema ?? json.schemaVersion ?? ''; } catch {}
	  }
	  const declaredChecksum = parsed.headers.checksum ?? parsed.headers['x-checksum-sha1'] ?? '';
	  const actualChecksum = crypto.createHash('sha1').update(parsed.body).digest('hex');
	  const cameraValid = isCameraJpeg && parsed.body.length >= 4 && parsed.body[0] === 0xff && parsed.body[1] === 0xd8 &&
		parsed.body[parsed.body.length - 2] === 0xff && parsed.body[parsed.body.length - 1] === 0xd9 &&
		(!declaredChecksum || declaredChecksum.toLowerCase() === actualChecksum);
	  let lidarTelemetryValid = false;
	  if (isLidarTelemetry) {
		try { lidarTelemetryValid = JSON.parse(parsed.body.toString('utf8')).schema === 'virtual-lidar.telemetry.v1'; } catch {}
	  }
	  const messageValid = isBinaryPcd ? pcdValidation.valid : isCameraJpeg ? cameraValid : isLidarTelemetry ? lidarTelemetryValid : schema.startsWith('virtual-');
      if (isBinaryPcd && messageValid && !firstPcdSaved && args.has('--save-first-pcd')) {
        fs.writeFileSync(args.get('--save-first-pcd'), parsed.body);
        firstPcdSaved = true;
      }
      const entry = {
        receivedUtc: new Date().toISOString(),
        destination,
        requestId: parsed.headers['x-request-id'] ?? parsed.headers['request-id'] ?? '',
        sensorId: parsed.headers['x-sensor-id'] ?? parsed.headers['sensor-id'] ?? '',
        sensorType: parsed.headers['x-sensor-type'] ?? '',
        dataKind: parsed.headers['x-data-kind'] ?? '',
        frameId: parsed.headers['x-frame-id'] ?? parsed.headers['frame-id'] ?? '',
        runId: parsed.headers['x-run-uuid'] ?? '',
        mtlNo: parsed.headers['x-mtl-no'] ?? '',
        slabFrameNo: Number(parsed.headers['x-slab-frame-no'] ?? '-1'),
        slabElapsedSec: Number(parsed.headers['x-slab-elapsed-sec'] ?? '-1'),
        contentType: parsed.headers['content-type'] ?? '',
		bytes: parsed.body.length,
        schema,
		valid: messageValid,
		pointCount: pcdValidation?.pointCount ?? null,
		checksum: pcdValidation?.checksum ?? actualChecksum,
		validationErrors: pcdValidation?.failedChecks ?? [],
		bodyPreview: isBinaryPcd ? parsed.body.subarray(0, 32).toString('hex') : parsed.body.subarray(0, 180).toString('utf8'),
      };
      if (requiredSlabRuns > 0) {
        const correlationValid = /^[0-9a-f-]{36}$/i.test(entry.runId) && entry.mtlNo === 'SQ83521 047' &&
          Number.isInteger(entry.slabFrameNo) && entry.slabFrameNo >= 0 && entry.slabFrameNo < 600 && Math.abs(entry.slabElapsedSec - entry.slabFrameNo * 0.05) < 0.00001;
        entry.valid &&= correlationValid;
        if (!correlationValid) entry.validationErrors.push('invalid slab correlation');
        const run = slabRuns.get(entry.runId) ?? {};
        const m = run[destination] ?? { count: 0, invalid: 0, gaps: 0, duplicates: 0, lastSensorFrame: null, firstSlabFrame: entry.slabFrameNo, lastSlabFrame: -1 };
        if (entry.valid) ++m.count; else ++m.invalid;
        const sensorFrame = Number(entry.frameId);
        if (m.lastSensorFrame != null) { if (sensorFrame <= m.lastSensorFrame) ++m.duplicates; else m.gaps += Math.max(0, sensorFrame - m.lastSensorFrame - 1); }
        if (entry.slabFrameNo < m.lastSlabFrame) ++m.invalid;
        m.lastSensorFrame = sensorFrame; m.lastSlabFrame = entry.slabFrameNo;
        run[destination] = m; slabRuns.set(entry.runId, run);
      }
      const shouldMeasure = requiredSlabRuns > 0 || durationSeconds <= 0 || measurementStartedMs > 0;
      if (shouldMeasure) {
        if (counts.has(destination) && messageValid) counts.set(destination, counts.get(destination) + 1);
        updateMetrics(destination, entry);
        messages.push(entry);
        if (messages.length > 200) messages.shift();
      }
      console.error(`[MESSAGE] ${destination} request=${entry.requestId} sensor=${entry.sensorId} frame=${entry.frameId} bytes=${entry.bytes} schema=${entry.schema}`);
      if (ackTopic && entry.requestId) {
		socket.send(frame('SEND', { destination: ackTopic, 'destination-type': 'MULTICAST', 'content-type': 'application/json', 'x-request-id': entry.requestId }, JSON.stringify({ requestId: entry.requestId, processed: true, source: 'ma0t10-stomp-probe' })));
      }
      if (durationSeconds > 0 && messageValid && !warmupStarted) {
        warmupStarted = true;
        if (warmupSeconds > 0) warmupTimer = setTimeout(() => beginDurationMeasurement(socket), warmupSeconds * 1000);
        else beginDurationMeasurement(socket);
      }
      if (durationSeconds <= 0 && hasAllExpected()) {
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
  if (!finished && process.exitCode == null) {
    report(false, connected ? 'socket closed before expected messages arrived' : 'socket closed before STOMP CONNECTED');
    process.exitCode = 4;
  }
});

setTimeout(() => {
  if (!finished && (durationSeconds > 0 || !hasAllExpected())) {
    report(false, `timeout after ${timeoutSeconds}s`);
    process.exitCode = 5;
    socket.close();
  }
}, timeoutSeconds * 1000).unref();
