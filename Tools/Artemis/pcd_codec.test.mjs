import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { parsePcd, decodeLzf, canonicalSensorId } from './pcd_codec.mjs';

test('legacy coil metadata resolves to role ID without changing archived PCD bytes',()=>{
  const original=fs.readFileSync(new URL('../../Source/ma0t10_dt/MA0T10/Sensor/Tests/Fixtures/ArchitectureWire/with-context.pcd',import.meta.url));
  const parsed=parsePcd(original), lines=parsed.headerText.split('\n');
  const index=lines.findIndex(line=>line.startsWith('# MA0T10_META '));
  const meta=JSON.parse(lines[index].slice(14));meta.sensor_id='Coil.Quay.XT32';
  lines[index]='# MA0T10_META '+JSON.stringify(meta);
  const archived=Buffer.concat([Buffer.from(lines.join('\n')),original.subarray(parsed.payloadOffset)]), before=Buffer.from(archived);
  const loaded=parsePcd(archived);
  assert.equal(loaded.metadata.sensor_id,'os_lidar_coil_quay');
  assert.deepEqual(archived,before);assert.deepEqual(loaded.records,parsed.records);
  assert.equal(canonicalSensorId('Coil.Quay.XT32.custom'),'Coil.Quay.XT32.custom');
});

test('existing Full golden records remain bit exact',()=>{
  const body=fs.readFileSync(new URL('../../Source/ma0t10_dt/MA0T10/Sensor/Tests/Fixtures/ArchitectureWire/with-context.pcd',import.meta.url));
  const parsed=parsePcd(body);assert.equal(parsed.step,33);assert.equal(parsed.points,2);
  assert.deepEqual(parsed.records,body.subarray(body.indexOf('DATA binary\n')+12));
});
test('Full Compact Minimal Custom: binary/compressed and validation-only agree',()=>{
  const allNames='x y z intensity ring horizontal_index return_index return_count time_offset_ns validity confidence'.split(' ');
  const allSizes=[4,4,4,2,2,2,1,1,8,1,4],allTypes='F F F U U U U U I U F'.split(' ');
  for(const fields of [allNames,'x y z intensity ring time_offset_ns'.split(' '),['x','y','z'],['x','y','z','confidence']]) {
    const sizes=fields.map(n=>allSizes[allNames.indexOf(n)]),types=fields.map(n=>allTypes[allNames.indexOf(n)]),step=sizes.reduce((a,b)=>a+b,0);
    const raw=Buffer.alloc(step*2);for(let i=0;i<raw.length;i++)raw[i]=i;
    for(const compressed of [false,true]) {
      let payload=raw;
      if(compressed) {
        const planes=[];let off=0;
        sizes.forEach(s=>{for(let p=0;p<2;p++)planes.push(raw.subarray(p*step+off,p*step+off+s));off+=s;});
        const soa=Buffer.concat(planes),chunks=[];for(let i=0;i<soa.length;i+=32){const chunk=soa.subarray(i,i+32);chunks.push(Buffer.from([chunk.length-1]),chunk);}
        const packed=Buffer.concat(chunks),prefix=Buffer.alloc(8);prefix.writeUInt32LE(packed.length);prefix.writeUInt32LE(soa.length,4);payload=Buffer.concat([prefix,packed]);
      }
      const header=Buffer.from(`# .PCD v0.7\nVERSION 0.7\nFIELDS ${fields.join(' ')}\nSIZE ${sizes.join(' ')}\nTYPE ${types.join(' ')}\nCOUNT ${fields.map(()=>1).join(' ')}\nWIDTH 2\nHEIGHT 1\nPOINTS 2\nDATA ${compressed?'binary_compressed':'binary'}\n`);
      const body=Buffer.concat([header,payload]);assert.deepEqual(parsePcd(body).records,raw);assert.equal(parsePcd(body,{materialize:false}).step,step);
      assert.throws(()=>parsePcd(body.subarray(0,-1)));
    }
  }
});
test('PCL fixed LZF overlap and malformed stream bounds',()=>{
  assert.deepEqual(decodeLzf(Buffer.from([0,65,224,0,0]),10),Buffer.from('AAAAAAAAAA'));
  for(const materialize of [false,true])assert.throws(()=>decodeLzf(Buffer.from([224,0,0]),10,materialize));
});
