import xml.etree.ElementTree as ET, re, sys
reg = ET.parse('proto/khronos/gl.xml').getroot()
cmds = {}
for c in reg.find('commands'):
    n = c.find('proto/name').text
    al = c.find('alias')
    cmds[n] = al.get('name') if al is not None else None
ext = {e.get('name'): e for e in reg.find('extensions')}
def feat(api, maxn):
    s = set()
    for f in reg.findall('feature'):
        if f.get('api') == api and float(f.get('number')) <= maxn:
            for r in f.findall('require'):
                if r.get('profile') == 'core': continue
                s |= {c.get('name') for c in r.findall('command')}
            for r in f.findall('remove'):
                pass
    return s
def extcmds(names, api):
    out = {}
    for x in names:
        e = ext.get(x)
        if e is None: continue
        for r in e.findall('require'):
            if r.get('api') and r.get('api') != api: continue
            for c in r.findall('command'):
                out.setdefault(c.get('name'), x)
    return out
# carried by RISC3D now
sys.argv=['gen.py','/tmp/g']
src = open('proto/gen.py').read()
ns = {'__file__': 'proto/gen.py'}
exec(compile(src.split('# --- how each function travels')[0], 'gen', 'exec'), ns)
carried = set(ns['want']) | {a for a,_ in ns['aliases']}
for api, core, f in (('gl', feat('gl', 2.1), 'riscos-mesa-20.3.5-gl21-extensions.txt'), ('gles1', feat('gles1', 1.1), 'riscos-mesa-20.3.5-es11-extensions.txt'), ('gles2', feat('gles2', 2.0), 'riscos-mesa-20.3.5-es20-extensions.txt')):
    exts = [l.strip() for l in open('docs/surface/'+f) if l.strip()]
    ec = extcmds(exts, api)
    allc = core | set(ec)
    miss_core = sorted(core - carried)
    miss_ext = {}
    for c, x in ec.items():
        if c not in carried and c not in core:
            miss_ext.setdefault(x, []).append(c)
    print('==', api, 'core', len(core), 'missing core', len(miss_core), '| exts', len(exts), 'ext cmds', len(ec), 'missing ext cmds', sum(len(v) for v in miss_ext.values()), 'in', len(miss_ext), 'extensions')
    if api != 'gl': print('  missing core:', ' '.join(miss_core[:200]))
    for x in sorted(miss_ext): print('  ', x, len(miss_ext[x]))
