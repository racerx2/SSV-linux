#!/usr/bin/env python3
# Space Shuttle Vessel, Linux port: one-time setup in the Orbiter folder. Modified by racerx2, 2026.
# Licensed under the GNU General Public License version 2 or any later version (see SSV-LICENSE.txt).
# Run from the Orbiter folder (or give its path). Safe to run again: only missing parts are added.
import filecmp, os, shutil, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SURFBASE = 'DIR Earth\\SSV CONTEXT SSV'
OBSERVERS = [
	'KSC:LC-39 Press Site: -80.64542 28.58199 2.00',
	'KSC:VAB NW CNR OTV-081: -80.65175 28.58646 169.46',
	'KSC:VAB SE CNR OTV-082: -80.65022 28.58564 169.46',
	'KSC:VAB HPG BLDG K7-853 OTV-089: -80.64726 28.58593 1.25',
	'KSC:LC 39A TV-4A: -80.59946 28.60839 10.76',
	'KSC:LC-39A OTV-070: -80.60204 28.60519 1.72',
	'KSC:LC-39A OTV-071: -80.60527 28.60574 1.64',
	'KSC:LC-39A OTV-060: -80.60312 28.60940 30.00',
	'KSC:LC 39B TV-7B: -80.61635 28.62722 10.76',
	'KSC:LC-39B OTV-170: -80.61969 28.62518 1.72',
	'KSC:LC-39B OTV-171: -80.62177 28.62453 1.64',
	'KSC:LC-39B OTV-160: -80.61985 28.62828 30.00',
]
BASETEX = 'SSV\\VAFB\\DeltaBlue'
INDEX = {'Elev_mod': 'mod', 'Surf': 'written', 'Mask': 'written'}  # the tile stream's index for files it wrote in that layer
changes, notes = [], []

def read_lines(p):
	with open(p, 'rb') as f:
		raw = f.read().decode('latin-1')
	nl = '\r\n' if '\r\n' in raw else '\n'
	return raw.replace('\r\n', '\n').split('\n'), nl

def write_lines(p, lines, nl):
	bak = p + '.ssv-backup'
	if not os.path.exists(bak):
		shutil.copy2(p, bak)
	tmp = p + '.ssv-tmp'
	with open(tmp, 'wb') as f:
		f.write(nl.join(lines).encode('latin-1'))
	os.replace(tmp, p)

def norm(l):
	# a config line without its comment, whitespace runs as one space, any letter case
	return ' '.join(l.split(';')[0].split()).lower()

def block(lines, begin, end):
	b = next((i for i, l in enumerate(lines) if l.strip().upper() == begin), None)
	e = next((i for i, l in enumerate(lines) if b is not None and i > b and l.strip().upper() == end), None)
	return b, e

def earth_cfg(root):
	p = os.path.join(root, 'Config', 'Earth.cfg')
	lines, nl = read_lines(p)
	n0 = len(lines)
	b, e = block(lines, 'BEGIN_SURFBASE', 'END_SURFBASE')
	moved = False
	if b is None or e is None:
		notes.append('Config/Earth.cfg has no BEGIN_SURFBASE/END_SURFBASE block: add "%s" first in it by hand' % SURFBASE)
	elif norm(lines[b + 1]) != norm(SURFBASE):
		# first in the block: a base name loads once, so SSV's Cape Canaveral, Edwards, Vandenberg... come before the stock ones
		old = [i for i in range(b + 1, e) if norm(lines[i]) == norm(SURFBASE)]
		for i in reversed(old):
			del lines[i]
		lines.insert(b + 1, SURFBASE)
		moved = bool(old)
	b, e = block(lines, 'BEGIN_OBSERVER', 'END_OBSERVER')
	if b is None or e is None:
		notes.append('Config/Earth.cfg has no BEGIN_OBSERVER/END_OBSERVER block: the SSV ground cameras are not added')
	else:
		have = {norm(l.split(':')[0]) + ':' + norm(l.split(':')[1]) for l in lines[b + 1:e] if l.count(':') >= 2}
		add = [o for o in OBSERVERS if norm(o.split(':')[0]) + ':' + norm(o.split(':')[1]) not in have]
		lines[e:e] = add
	if len(lines) != n0 or moved:
		write_lines(p, lines, nl)
		changes.append('Config/Earth.cfg: %d lines added%s' % (len(lines) - n0, ', the SSV bases moved first' if moved else ''))

def base_cfg(root):
	p = os.path.join(root, 'Config', 'Base.cfg')
	if not os.path.isfile(p):
		notes.append('Config/Base.cfg not found: add "%s" to its BEGIN_TEXTURES/END_TEXTURES block by hand' % BASETEX)
		return
	lines, nl = read_lines(p)
	b, e = block(lines, 'BEGIN_TEXTURES', 'END_TEXTURES')
	if b is None or e is None:
		notes.append('Config/Base.cfg has no BEGIN_TEXTURES/END_TEXTURES block: add "%s" by hand' % BASETEX)
	elif not any(norm(l) == norm(BASETEX) for l in lines[b:e]):
		lines.insert(e, BASETEX)
		write_lines(p, lines, nl)
		changes.append('Config/Base.cfg: %s added' % BASETEX)

def deindex(earth, layer, key):
	# the stream manages, replaces and evicts only files in its index; a tile replaced here becomes someone else's
	if layer not in INDEX:
		return False
	p = os.path.join(earth, 'TileStream', INDEX[layer] + '.idx')
	if not os.path.exists(p):
		return False
	lines, nl = read_lines(p)
	keep = [l for l in lines if l.split()[:3] != [str(k) for k in key]]
	if len(keep) == len(lines):
		return False
	tmp = p + '.ssv-tmp'
	with open(tmp, 'wb') as f:
		f.write(nl.join(keep).encode('latin-1'))
	os.replace(tmp, p)
	return True

def terrain(root):
	src = os.path.join(HERE, 'Earth')
	earth = os.path.join(root, 'Textures', 'Earth')
	if not os.path.isdir(src):
		notes.append('no terrain tiles beside the setup script (%s)' % src)
		return
	added = replaced = same = 0
	for layer in sorted(os.listdir(src)):
		for d, _, files in os.walk(os.path.join(src, layer)):
			for f in sorted(files):
				s = os.path.join(d, f)
				rel = os.path.relpath(s, src)
				t = os.path.join(earth, rel)
				if os.path.exists(t) and not os.path.islink(t) and filecmp.cmp(s, t, shallow=False):
					same += 1
					continue
				os.makedirs(os.path.dirname(t), exist_ok=True)
				if os.path.lexists(t):
					bak = os.path.join(HERE, 'Earth-backup', rel)
					if not os.path.exists(bak):
						os.makedirs(os.path.dirname(bak), exist_ok=True)
						shutil.copy2(t, bak)
					parts = rel.split(os.sep)
					tile = len(parts) == 4 and parts[1].isdigit() and parts[2].isdigit() and os.path.splitext(parts[3])[0].isdigit()  # layer/level/lat/lng.ext
					if tile and deindex(earth, layer, (int(parts[1]), int(parts[2]), int(os.path.splitext(parts[3])[0]))):
						notes.append('%s: was a tile stream tile; the stream now leaves it alone (old one in Earth-backup)' % rel)
					os.unlink(t)
					replaced += 1
				else:
					added += 1
				shutil.copy2(s, t)
	if added or replaced:
		changes.append('Textures/Earth: %d terrain tiles added, %d replaced' % (added, replaced))

def settings(root):
	def items(p):
		d = {}
		if os.path.exists(p):
			for l in read_lines(p)[0]:
				if '=' in l and not l.lstrip().startswith(';'):
					k, v = l.split('=', 1)
					d[k.strip().lower()] = v.split(';')[0].strip()
		return d
	o = items(os.path.join(root, 'Orbiter.cfg'))
	if o.get('enablelocallights', 'FALSE').upper() != 'TRUE':
		notes.append('local light sources are off: Launchpad > Visual effects > Local light sources (the SSV lights need them)')
	if o.get('vcmfdsize', '512') != '512':
		notes.append('VC MFD texture size is %s: SSV needs 512 (Launchpad > Extra > Instruments and panels > MFD parameter configuration)' % o['vcmfdsize'])
	v = items(os.path.join(root, 'Modules', 'VulkanClient', 'VulkanClient.cfg'))
	if v.get('lightconfiguration', '1') == '0':
		notes.append('local lights are off in the graphics client: Launchpad > Video > Advanced > Local lights')
	if v.get('nearclipplanemode', '0') != '1':
		notes.append('SSV recommends Launchpad > Video > Advanced > "Disable near clip plane compatibility mode"')

def main():
	root = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.getcwd())
	if not os.path.isfile(os.path.join(root, 'Config', 'Earth.cfg')) or not os.path.isdir(os.path.join(root, 'Modules')):
		sys.exit('not an Orbiter folder: %s (run from the Orbiter folder, or give its path)' % root)
	try:
		running = subprocess.run(['pgrep', '-x', 'Orbiter'], capture_output=True).returncode == 0
	except OSError:
		running = False  # no pgrep: the user was told to close Orbiter
	if running:
		sys.exit('Orbiter is running: close it first')
	earth_cfg(root)
	base_cfg(root)
	terrain(root)
	settings(root)
	print('SSV setup in %s' % root)
	for c in changes or ['nothing to change']:
		print('  ' + c)
	for n in notes:
		print('  note: ' + n)

if __name__ == '__main__':
	main()
