"""NAMING-01: 명시된 파일/타입 이동. 문자열 계약과 기본 subobject 이름은 유지한다."""
import argparse
import collections
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Saved/Reports/NAMING-01'
PLUGIN = ROOT / 'Plugins/DTDevice'
HOST_SOURCE = 'Source/ma0t10_dt/MA0T10/'


def tracked(root, *paths):
    return subprocess.check_output(['git', '-C', str(root), 'ls-files', *paths], text=True).splitlines()


def put(path, text):
    path.write_bytes(text.replace('\r\n', '\n').replace('\n', '\r\n').encode('utf8'))


def build_plan():
    names, owners = {}, {}
    for line in (ROOT/'docs/NAMING_01_INVENTORY.md').read_text(encoding='utf8').splitlines():
        if line.startswith('## 3.'): break
        cells=[c.strip().strip('`') for c in line.split('|')[1:-1]]
        if len(cells)<4 or cells[0] not in ['DTDevice','Project']: continue
        old=cells[1]; new=cells[2] if cells[2].startswith('U') and cells[2]!='UDxWidget' else cells[3]
        if re.fullmatch(r'U\w+',old) and re.fullmatch(r'U\w+',new) and new!='UDxWidget':
            names[old]=new; owners[old]=cells[0]
    spec={'names':names,'owners':owners}
    if len(names)!=48: raise RuntimeError('Confirmed inventory must contain 48 renamed classes')
    rows = json.loads((ROOT / 'Tools/NamingPlan/inventory-7aefff54.json').read_text())['Rows']
    stems = {a[1:]: b[1:] for a, b in names.items()}
    moves = {'host': {}, 'plugin': {}}
    in_moves = False
    for line in (ROOT / 'docs/NAMING_01_INVENTORY.md').read_text(encoding='utf8').splitlines():
        if line.startswith('## 3.'): in_moves = True; continue
        if in_moves and line.startswith('## '): break
        match = re.match(r'\| `([^`]+)` \| `([^`]+)` \|', line)
        if not in_moves or not match: continue
        old, dest = match.groups()
        if dest.startswith('('):
            f = Path(old).name
            category = ('Manager/Automation' if f.startswith('Automation') else
                        'Coil' if f.startswith('Coil') else 'Ship' if f.startswith('Ship') else
                        'Slab/Scenario' if f.startswith('Slab') else
                        'Training' if f.startswith('Train') else 'Dataset')
            dest = category + '/Tests/'
        moves['host'][HOST_SOURCE + old] = HOST_SOURCE + dest + Path(old).name
    for owner, base in [('host', ROOT), ('plugin', PLUGIN)]:
        for old in tracked(base, 'Source', 'TestHost/Source'):
            if Path(old).suffix not in ['.cpp', '.h']: continue
            path = Path(moves[owner].get(old, old))
            for before, after in sorted(stems.items(), key=lambda pair: -len(pair[0])):
                if path.name.startswith(before):
                    path = path.with_name(after + path.name[len(before):]); break
            if path.as_posix() != old: moves[owner][old] = path.as_posix()
    # 멤버만 변경하며 기존 Blueprint 함수/인자 이름은 유지한다.
    member_names = {}
    properties = []
    declarations = re.compile(r'class\s+(?:\w+_API\s+)?([UA]\w+)\s*:\s*')
    targets = '|'.join(names)
    field = re.compile(r'(?:TObjectPtr<\s*(?:' + targets + r')\s*>|(?:' + targets + r')\s*\*)\s+(\w+Component)\s*(?=[;=])')
    for owner, base in [('host', ROOT), ('plugin', PLUGIN)]:
        for file in tracked(base, 'Source'):
            if not file.endswith('.h'): continue
            text = (base / file).read_text(encoding='utf-8-sig')
            classes = list(declarations.finditer(text))
            for match in field.finditer(text):
                name = match[1]; member_names[name] = name[:-9] + 'Comp'
                preceding = text[:match.start()]
                # A field whose nearest declaration has UPROPERTY is reflected; pointer locals are not.
                tail = preceding[max(preceding.rfind(';'), preceding.rfind('}')) + 1:]
                if 'UPROPERTY' not in tail: continue
                cls = next((m[1] for m in reversed(classes) if m.start() < match.start()), None)
                if not cls: raise RuntimeError('No property owner: ' + file + ':' + name)
                module = 'DeviceUI' if '/DeviceUI/' in file else 'DeviceRuntime' if owner == 'plugin' else 'ma0t10_dt'
                properties.append({'module': module, 'owner': cls[1:], 'old': name, 'new': member_names[name], 'file': file})
    # The capture actor's member is part of the explicit public component naming rule, not a type rename.
    member_names['CaptureComponent'] = 'CaptureComp'
    properties.append({'module':'DeviceRuntime','owner':'VirtualCameraSensorActor','old':'CaptureComponent','new':'CaptureComp','file':'Source/DeviceRuntime/Public/Device/Camera/VirtualCameraSensorActor.h'})
    return dict(names=names, owners=spec['owners'], moves=moves, members=member_names, properties=properties, rows=rows)


TOKEN = re.compile(r'//[^\n]*|/\*.*?\*/|R"([^ ()\\\t\r\n]{0,16})\(.*?\)\1"|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|\b[A-Za-z_]\w*\b', re.S)


def transform_source(text, replacements, includes):
    def token(m):
        word = m.group()
        return replacements.get(word, word) if re.fullmatch(r'[A-Za-z_]\w*', word) else word
    result = TOKEN.sub(token, text)
    def inc(m):
        path = m[2]
        return m[1] + includes.get(path, path) + m[3]
    return re.sub(r'(^\s*#\s*include\s*["<])([^">]+)([">])', inc, result, flags=re.M)


def apply(plan):
    names, moves = plan['names'], plan['moves']
    identifiers = {**names, **plan['members']}
    includes = {}
    path_refs = {}
    for owner in moves:
        for old, new in moves[owner].items():
            if old == new: continue
            path_refs[old] = new
            if old.startswith('Source/ma0t10_dt/'):
                includes['ma0t10_dt/' + old[len('Source/ma0t10_dt/'):]] = 'ma0t10_dt/' + new[len('Source/ma0t10_dt/'):]
            for suffix in ['Public/', 'Private/']:
                if suffix in old:
                    includes[old.split(suffix,1)[1]] = new.split(suffix,1)[1]
            includes[Path(old).name] = Path(new).name
            if old.endswith('.h'):
                includes[Path(old).stem+'.generated.h'] = Path(new).stem+'.generated.h'
    # Every moved header can be included by basename in its own module.
    frozen = []
    for owner, base in [('host', ROOT), ('plugin', PLUGIN)]:
        files = tracked(base, 'Source', 'TestHost/Source')
        for rel in files:
            p = base / rel
            if p.suffix not in ['.h', '.cpp', '.cs']: continue
            text = p.read_text(encoding='utf-8-sig')
            new = transform_source(text, identifiers, includes)
            frozen.append({'owner':owner,'file':rel,'literals':re.findall(r'"(?:\\.|[^"\\])*"',text)})
            if new != text: put(p,new)
        for old, new in sorted(moves[owner].items()):
            if old == new: continue
            (base / new).parent.mkdir(parents=True, exist_ok=True)
            subprocess.run(['git','-C',str(base),'mv','--',old,new],check=True)
    # Explicit source paths in tools/docs; protected/history/planning files stay original.
    refs = dict(path_refs)
    refs.update(includes)
    for owner, base in [('host',ROOT),('plugin',PLUGIN)]:
        for rel in tracked(base):
            p=base/rel
            if p.suffix not in ['.md','.py','.ps1','.cs','.bat','.json']: continue
            if rel.startswith(('Source/','TestHost/Source/','Tools/NamingPlan/')): continue
            if rel in ['CLAUDE.md','docs/IMPROVEMENTS_CLAUDE.md','docs/VALIDATION_HISTORY.md','docs/NAMING_01_INVENTORY.md']: continue
            text=p.read_text(encoding='utf-8-sig');new=text
            for old,dest in sorted(refs.items(),key=lambda pair:-len(pair[0])): new=new.replace(old,dest)
            if new!=text:put(p,new)
    (OUT/'frozen-literals-before.json').write_text(json.dumps(frozen,indent=2),encoding='utf8')
    plan.pop('rows')
    (OUT/'applied-plan.json').write_text(json.dumps(plan,indent=2),encoding='utf8')


if __name__ == '__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--apply',action='store_true');args=parser.parse_args()
    plan=build_plan()
    print('Classes',len(plan['names']),'members',len(plan['members']),'reflected properties',len(plan['properties']))
    print('Path changes',{owner:sum(a!=b for a,b in paths.items()) for owner,paths in plan['moves'].items()})
    if args.apply:
        if (OUT/'applied-plan.json').exists(): raise RuntimeError('Already applied; preserve this worktree and report')
        apply(plan)
