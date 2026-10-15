#!/usr/bin/env python3
# not upstream (SSV Linux port): writes Config/GC/SSV_OV.cfg and SSV_SRB.cfg from the meshes' material names, and adds the HUD combiners to VC_external.msh (design ssv-pbr)
import os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MESH = os.path.join(ROOT, 'Meshes')
VC_OFFSET = (0.0, -1.41, -2.42)  # panels' offset against VC_external's (OV/ParameterValues: VC_OFFSET - VC_EXTERNAL_OFFSET)

MDU = ['REFLECT 0.03 0.03 0.03', 'FRESNEL 5 1024 0.6', 'SMOOTHNESS 0.95 1']
GLASS = ['DIFFUSE 0.60 0.80 0.65 0.15', 'SPECULAR 0.9 0.9 0.9 60', 'REFLECT 0.06 0.08 0.06', 'FRESNEL 4 1024 0.8', 'SMOOTHNESS 0.95 1']
HUD_GLASS = GLASS + ['EMISSIVE 0.03 0.08 0.04']      # VC combiners: a faint green tint, the HUD draws its own light
HUD_GLASS_EXT = GLASS + ['EMISSIVE 0.02 0.04 0.03']  # external combiners: HUD off; SSV's HUD adds its glow (Atlantis::UpdateHUDGlow)
WINDOWS = ['REFLECT 0.05 0.05 0.05', 'FRESNEL 4 1024 0.9', 'SMOOTHNESS 0.98 1']
SRB_PAINT = ['PBR', 'PBR_METALLIC 0', 'PBR_ROUGHNESS 0.55', 'PBR_CLEARCOAT 0.25 0.3']


def read(path):
    with open(path, 'rb') as f:
        return f.read().decode('latin-1').replace('\r\n', '\n').split('\n')


def write(path, lines):
    with open(path, 'wb') as f:
        f.write(('\r\n'.join(lines) + '\r\n').encode('latin-1'))


def materials(lines):
    for i, l in enumerate(lines):
        t = l.split()
        if t and t[0] == 'MATERIALS':
            return [lines[i + 1 + k].strip() for k in range(int(t[1]))]
    return []


def index(mesh, name):  # GC index (from 0) of a material name in a mesh file
    m = materials(read(os.path.join(MESH, *mesh.split('\\')) + '.msh'))
    if name not in m:
        sys.exit('%s: no material %s' % (mesh, name))
    return m.index(name)


def group(lines, label):  # a group's GEOM line, vertices and triangles, by label
    cur = None
    i = 0
    while i < len(lines):
        t = lines[i].split()
        if t and t[0] == 'LABEL':
            cur = ' '.join(t[1:])
        elif t and t[0] == 'GEOM':
            nv, nt = int(t[1]), int(t[2])
            if cur == label:
                return lines[i:i + 1 + nv + nt]
            cur = None
            i += 1 + nv + nt
            continue
        i += 1
    sys.exit('no group ' + label)


def material_block(lines, name):
    for i, l in enumerate(lines):
        if l.split() == ['MATERIAL', name]:
            return lines[i + 1:i + 5]
    sys.exit('no material block ' + name)


def add_combiners():
    path = os.path.join(MESH, 'SSV', 'OV', 'VC_external.msh')
    L = read(path)
    while L and L[-1] == '':
        L.pop()
    if any(l.strip() == 'LABEL HUD_COMBINER_CDR' for l in L):
        return
    ng = next(i for i, l in enumerate(L) if l.startswith('GROUPS '))
    nm = next(i for i, l in enumerate(L) if l.startswith('MATERIALS '))
    nt = next(i for i, l in enumerate(L) if l.startswith('TEXTURES '))
    mats = materials(L)
    new = []
    for side, panel in (('CDR', 'panelF6'), ('PLT', 'panelF8')):
        P = read(os.path.join(MESH, 'SSV', 'OV', panel + '.msh'))
        g = group(P, 'hud')
        nv = int(g[0].split()[1])
        out = ['LABEL HUD_COMBINER_' + side, 'MATERIAL %d' % (len(mats) + (1 if side == 'CDR' else 2)), 'TEXTURE 0', g[0]]
        for v in g[1:1 + nv]:
            t = v.split()
            xyz = [float(t[k]) + VC_OFFSET[k] for k in range(3)]
            out.append(' '.join(['%g' % c for c in xyz] + t[3:]))
        out += g[1 + nv:]
        new += out
    glass = material_block(read(os.path.join(MESH, 'SSV', 'OV', 'panelF6.msh')), 'glass')
    L = (L[:nm] + new + ['MATERIALS %d' % (len(mats) + 2)] + mats + ['HUD_glass_CDR', 'HUD_glass_PLT'] + L[nm + 1 + len(mats):nt]
         + ['MATERIAL HUD_glass_CDR'] + glass + ['MATERIAL HUD_glass_PLT'] + glass + L[nt:])
    L[ng] = 'GROUPS %d' % (int(L[ng].split()[1]) + 2)
    write(path, L)


MARK = '; not upstream (SSV Linux port'


def ours(lines):  # the upstream part of a GC file: everything before our marker
    for i, l in enumerate(lines):
        if l.startswith(MARK):
            return lines[:i]
    return [l for l in lines if l != '']


def gc_ov():
    path = os.path.join(ROOT, 'Config', 'GC', 'SSV_OV.cfg')
    up = ours(read(path))
    last = [l.split()[1] for l in up if l.startswith('MESH ')][-1]
    if last != 'SSV\\OV\\OV':
        sys.exit('SSV_OV.cfg: the OV block is no longer the last; merge the windows by hand')
    out = up + [MARK + ', design ssv-pbr): MDU screen glass, HUD combiner glass and windshield. Written by linux/ssv_materials.py.',
                '; legacy keys, read by every graphics client',
                '; the windshield continues the OV block above',
                'MATERIAL %d' % index('SSV\\OV\\OV', 'WINDOWS_MATERIAL')] + WINDOWS
    for mesh, names in (('SSV\\OV\\panelF6', ['MDU_CDR1', 'MDU_CDR2']), ('SSV\\OV\\panelF7', ['MDU_CRT1', 'MDU_CRT2', 'MDU_CRT3', 'MDU_MFD1', 'MDU_MFD2']),
                        ('SSV\\OV\\panelF8', ['MDU_PLT1', 'MDU_PLT2']), ('SSV\\OV\\panelA134', ['MDU_AFD1'])):
        out.append('MESH ' + mesh)
        for n in names:
            out += ['MATERIAL %d' % index(mesh, n)] + MDU
        if mesh in ('SSV\\OV\\panelF6', 'SSV\\OV\\panelF8'):
            out += ['MATERIAL %d' % index(mesh, 'glass')] + HUD_GLASS
    out += ['MESH SSV\\OV\\VC_external']
    for n in ('HUD_glass_CDR', 'HUD_glass_PLT'):
        out += ['MATERIAL %d' % index('SSV\\OV\\VC_external', n)] + HUD_GLASS_EXT
    write(path, out)


def gc_srb():
    out = ['CONFIG_VERSION 3',
           MARK + ', design ssv-pbr): satin shine on the SRBs\' painted case. Written by linux/ssv_materials.py.',
           '; PBR keys: the Orbiter Linux client only (D3D9Client skips them).']
    d = os.path.join(MESH, 'SSV', 'SRB')
    for f in sorted(os.listdir(d)):
        if not f.endswith('.msh'):
            continue
        mesh = 'SSV\\SRB\\' + f[:-4]
        m = materials(read(os.path.join(d, f)))
        idx = [i for i, n in enumerate(m) if n in ('REFLECTIVE_MATERIAL', 'NON_REFLECTIVE_MATERIAL')]
        if not idx:
            continue
        out.append('MESH ' + mesh)
        for i in idx:
            out += ['MATERIAL %d' % i] + SRB_PAINT
    write(os.path.join(ROOT, 'Config', 'GC', 'SSV_SRB.cfg'), out)


if __name__ == '__main__':
    add_combiners()
    os.makedirs(os.path.join(ROOT, 'Config', 'GC'), exist_ok=True)
    gc_ov()
    gc_srb()
    print('ok')
