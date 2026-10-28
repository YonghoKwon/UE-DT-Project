"""Join native LiDAR ledger and DRED within one process log; never prove GPU safety."""
import argparse
import hashlib
import json
import re
from pathlib import Path


def analyze(text):
    lines = text.splitlines()
    if sum('LogInit: Command Line:' in s and 'LogAutomationController:' not in s for s in lines) > 1:
        raise ValueError('Multiple process sessions: analyze separate logs, never join their VAs')
    active, history, errors, faults, allocations = {}, [], [], {}, []
    builds = submits = links = 0
    blas_triangles, bindings, submissions = {}, {}, []
    removed = False
    dred_deleted = None
    for line_no, line in enumerate(lines, 1):
        if 'LogAutomationController:' in line:
            continue  # Reprinted events are not second creates/releases.
        removed |= ('DXGI_ERROR_DEVICE_REMOVED' in line or
                    '[Aftermath] Status: PageFault' in line or
                    'GPU Crashed or D3D Device Removed' in line)
        m = re.search(r'SensorNativeLedger\] create VA=0x([\da-f]+) bytes=(\d+).*role=(\S+)', line, re.I)
        if m:
            va, size = int(m[1], 16), int(m[2])
            if va in active:
                errors.append({'line': line_no, 'kind': 'create_over_live_va', 'va': hex(va)})
            row = {'va': va, 'bytes': size, 'role': m[3], 'created_line': line_no,
                   'released_line': None, 'generation': len(history) + 1}
            active[va] = row
            history.append(row)
        m = re.search(r'SensorNativeLedger\] release VA=0x([\da-f]+).*last_use=(\d+) completed=(\d+) device=0x([\da-f]+)', line, re.I)
        if m:
            va, used, done, device = int(m[1], 16), int(m[2]), int(m[3]), int(m[4], 16)
            if va not in active:
                errors.append({'line': line_no, 'kind': 'release_unknown_va', 'va': hex(va)})
            else:
                active.pop(va).update(released_line=line_no, last_use_fence=used,
                                      recorded_completed_fence=done, device_status=hex(device))
            if device == 0 and used > done:
                errors.append({'line': line_no, 'kind': 'release_before_recorded_fence',
                               'va': hex(va), 'last_use': used, 'completed': done})
        keys = []
        if '[SensorNativeBlasBuild]' in line:
            builds += 1
            keys = ['vertices', 'surfaces', 'result', 'scratch']
            count = re.search(r'vertex_count=(\d+)', line)
            result = re.search(r'result=0x([\da-f]+)', line, re.I)
            if count and result:
                blas_triangles[int(result[1], 16)] = int(count[1]) // 3
        elif '[SensorNativeTlasInstance]' in line:
            links += 1
            keys = ['BLAS', 'vertices', 'surfaces']
        elif '[SensorNativeTlasBuild]' in line:
            keys = ['instances', 'result', 'scratch']
        elif '[SensorNativeSubmit]' in line:
            submits += 1
            keys = ['TLAS', 'constants', 'rays', 'results']
            m = re.search(r'fence=(\d+) completed=(\d+)', line)
            if m:
                submissions.append({'fence': int(m[1]), 'completed': int(m[2]), 'line': line_no})
        elif '[SensorNativeBinding]' in line:
            keys = ['surface', 'blas']
            m = re.search(r'fence=(\d+) mesh=(\d+) surface=0x([\da-f]+) blas=0x([\da-f]+)', line, re.I)
            if m:
                bindings.setdefault(int(m[1]), []).append({'mesh': int(m[2]), 'surface': int(m[3], 16), 'blas': int(m[4], 16)})
        for key in keys:
            m = re.search(r'\b' + key + r'=0x([\da-f]+)', line, re.I)
            if m:
                va = int(m[1], 16)
                if not any(x['va'] <= va < x['va'] + x['bytes'] for x in active.values()):
                    errors.append({'line': line_no, 'kind': 'recorded_reference_not_live',
                                   'field': key, 'va': hex(va)})
        m = re.search(r'(?:PageFault at VA GPUAddress\s*"?|Faulting address:\s*|faultVA=)0x([\da-f]+)', line, re.I)
        if m and int(m[1], 16):
            va = int(m[1], 16)
            faults.setdefault(va, {'va': hex(va), 'first_line': line_no, 'lines': []})['lines'].append(line_no)
        if 'DRED: Active objects with VA ranges' in line:
            dred_deleted = False
        elif 'DRED: Recent freed objects with VA ranges' in line:
            dred_deleted = True
        m = re.search(r'SensorNativeDREDAllocation\] deleted=([01])', line)
        if m:
            dred_deleted = bool(int(m[1]))
        if 'Name:' in line or '[SensorNativeDREDAllocation]' in line:
            m = re.search(r'SensorOwned/([^/\s]+)/0x([\da-f]+)/(\d+)', line, re.I)
            if m:
                allocations.append({'line': line_no, 'role': m[1], 'va': hex(int(m[2], 16)),
                                    'bytes': int(m[3]), 'recently_freed': dred_deleted})
    for va, fault in faults.items():
        matches = []
        for h in history:
            if h['created_line'] <= fault['first_line'] and h['va'] <= va < h['va'] + h['bytes']:
                row = {**h, 'va': hex(h['va'])}
                row['released_before_fault_report'] = h['released_line'] is not None and h['released_line'] < fault['first_line']
                matches.append(row)
        fault['native_range_generations'] = matches
        fault['address_reused_or_ambiguous'] = len(matches) > 1
        fault['matching_dred_names'] = [a for a in allocations if int(a['va'], 16) <= va < int(a['va'], 16) + a['bytes']]
        prior = [s for s in submissions if s['line'] < fault['first_line']]
        # D3D12 reports UINT64_MAX on device removal; it does not prove that
        # the faulting GPU work completed safely.
        completed = max((s['completed'] for s in prior if s['completed'] < (1 << 64) - 1), default=0)
        candidates = []
        for s in prior:
            if s['fence'] <= completed:
                continue
            roots = bindings.get(s['fence'], [])
            max_triangles = max((blas_triangles.get(b['blas'], 0) for b in roots), default=0)
            for b in roots:
                allocations_at_bind = [h for h in history if h['va'] == b['surface'] and h['created_line'] < s['line'] and
                                       (h['released_line'] is None or h['released_line'] > s['line'])]
                if len(allocations_at_bind) != 1:
                    continue
                a = allocations_at_bind[0]
                if a['va'] + a['bytes'] <= va < a['va'] + max_triangles * 16:
                    candidates.append({'fence': s['fence'], 'mesh_slot': b['mesh'],
                                       'surface_base': hex(a['va']), 'surface_bytes': a['bytes'],
                                       'scene_max_triangles': max_triangles,
                                       'derived_triangle_index_at_fault_va': (va - a['va']) // 16})
        fault['UnboundedRootSrvOobCandidates'] = candidates
    return {'Valid': bool(history) and not removed and not errors and not faults,
            'Scope': 'Single-process CPU ledger and recorded fences only. Freed-range matches are candidates, not proof of GPU use-after-free or driver correctness. Missing DRED data is unobserved, not zero.',
            'DeviceFailureObserved': removed, 'Creates': len(history),
            'Releases': sum(x['released_line'] is not None for x in history),
            'LiveAtLogEnd': len(active), 'BlasBuilds': builds,
            'TlasInstanceLinks': links, 'NativeSubmits': submits,
            'RecordedReferenceErrors': errors, 'Faults': list(faults.values()),
            'DredNativeAllocations': allocations, 'CrossProcessJoin': False}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('log', type=Path)
    p.add_argument('--output', required=True, type=Path)
    args = p.parse_args()
    raw = args.log.read_bytes()
    text = raw.decode('utf-16' if raw.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig')
    out = analyze(text)
    out.update(InputLog=str(args.log.resolve()), InputLogSHA256=hashlib.sha256(raw).hexdigest())
    args.output.write_text(json.dumps(out, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: out[k] for k in ['Valid', 'Creates', 'Releases', 'NativeSubmits', 'DeviceFailureObserved']}))


if __name__ == '__main__':
    main()
