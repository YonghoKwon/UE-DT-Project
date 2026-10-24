// Original bounded PCD/LZF parser. No network, credentials, or third-party code.
const names = 'x y z intensity ring horizontal_index return_index return_count time_offset_ns validity confidence'.split(' ');
const sizes = [4,4,4,2,2,2,1,1,8,1,4];
const types = 'F F F U U U U U I U F'.split(' ');
const limit = 64 * 1024 * 1024;

export function canonicalSensorId(id) {
  return id === 'Coil.Quay.XT32' ? 'os_lidar_coil_quay' : id;
}

export function decodeLzf(data, expected, materialize = true) {
  if (!Number.isSafeInteger(expected) || expected < 0 || expected > limit) throw Error('LZF size');
  const out = materialize ? Buffer.allocUnsafe(expected) : null;
  let i = 0, written = 0;
  while (i < data.length) {
    const control = data[i++];
    if (control < 32) {
      const n = control + 1;
      if (i+n > data.length || written+n > expected) throw Error('LZF literal');
      if (out) data.copy(out, written, i, i+n);
      i += n; written += n;
    } else {
      let n = control >> 5;
      if (n === 7) { if (i >= data.length) throw Error('LZF length'); n += data[i++]; }
      if (i >= data.length) throw Error('LZF distance');
      const distance = ((control & 31) << 8) + data[i++] + 1;
      n += 2;
      if (distance > written || written+n > expected) throw Error('LZF reference');
      if (out) for (let j = 0; j < n; j++) { out[written] = out[written-distance]; written++; }
      else written += n;
    }
  }
  if (written !== expected) throw Error('LZF size');
  return out;
}

export function parsePcd(body, { materialize = true } = {}) {
  if (!Buffer.isBuffer(body) || body.length > limit) throw Error('PCD size');
  const header = new Map();
  let offset = 0, found = false, metadata = null;
  while (offset < Math.min(body.length, 65536)) {
    const end = body.indexOf(10, offset);
    if (end < 0 || end >= 65536) throw Error('PCD header');
    const line = body.subarray(offset, end).toString('utf8').trim();
    offset = end + 1;
    if (line.startsWith('# MA0T10_META ')) {
      if (metadata !== null) throw Error('duplicate PCD metadata');
      metadata = JSON.parse(line.slice(14));
      if (typeof metadata.sensor_id === 'string') metadata.sensor_id = canonicalSensorId(metadata.sensor_id);
    }
    if (!line || line.startsWith('#')) continue;
    const [key, ...values] = line.split(/\s+/);
    if (header.has(key)) throw Error('duplicate PCD header');
    header.set(key, values);
    if (key === 'DATA') { found = true; break; }
  }
  const single = key => { const a = header.get(key); if (a?.length !== 1) throw Error('PCD '+key); return a[0]; };
  const integer = text => { if (!/^\d+$/.test(text)) throw Error('PCD integer'); const n=Number(text); if (!Number.isSafeInteger(n)) throw Error('PCD integer'); return n; };
  const mode = single('DATA');
  if (!found || single('VERSION') !== '0.7' || !['binary','binary_compressed'].includes(mode)) throw Error('PCD version/encoding');
  const fields=header.get('FIELDS'), size=header.get('SIZE')?.map(integer), type=header.get('TYPE'), count=header.get('COUNT')?.map(integer);
  if (!fields || fields.length < 3 || fields.length > 64 || new Set(fields).size !== fields.length || ![size,type,count].every(a=>a?.length===fields.length) || !['x','y','z'].every(n=>fields.includes(n))) throw Error('PCD fields');
  const offsets=[]; let step=0;
  fields.forEach((name,i)=>{
    if (![1,2,4,8].includes(size[i]) || !['F','I','U'].includes(type[i]) || count[i]<1 || count[i]>1024) throw Error('PCD field type');
    const known=names.indexOf(name);
    if (known>=0 && (size[i]!==sizes[known] || type[i]!==types[known] || count[i]!==1)) throw Error('PCD layout');
    offsets.push(step); step+=size[i]*count[i];
  });
  const points=integer(single('POINTS')), width=integer(single('WIDTH')), height=integer(single('HEIGHT'));
  if (height<1 || width*height!==points || points*step>limit) throw Error('PCD dimensions');
  let records=null;
  if (mode==='binary') {
    if (body.length-offset!==points*step) throw Error('PCD bytes');
    if (materialize) records=body.subarray(offset);
  } else {
    if (body.length-offset<8) throw Error('PCD LZF prefix');
    const packed=body.readUInt32LE(offset), unpacked=body.readUInt32LE(offset+4);
    if (packed!==body.length-offset-8 || unpacked!==points*step) throw Error('PCD LZF size');
    const soa=decodeLzf(body.subarray(offset+8),unpacked,materialize);
    if (materialize) {
      records=Buffer.allocUnsafe(unpacked);let plane=0;
      fields.forEach((_,f)=>{const bytes=size[f]*count[f];for(let p=0;p<points;p++)soa.copy(records,p*step+offsets[f],plane+p*bytes,plane+(p+1)*bytes);plane+=points*bytes;});
    }
  }
  return {points,step,fields,offsets,records,mode,metadata,payloadOffset:offset,headerText:body.subarray(0,offset).toString('utf8')};
}
