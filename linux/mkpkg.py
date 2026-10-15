#!/usr/bin/env python3
# Space Shuttle Vessel, Linux port: builds the add-on zip from the repo's tracked files and the built modules. Modified by racerx2, 2026.
# Licensed under the GNU General Public License version 2 or any later version (see SSV-LICENSE.txt).
# usage: linux/mkpkg.py <build dir> <manual pdf or -> <out dir>
import os, re, subprocess, sys, zipfile, zlib

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERSION = '1.16'
PORT = '1.0'
NAME = 'SSV-%s-linux-%s-x64.zip' % (VERSION, PORT)
INSTALL = 'Install/Space Shuttle Vessel/'
DOC = 'Doc/Space Shuttle Vessel/'

def git_files(*opts):
	out = subprocess.run(['git', '-C', REPO, 'ls-files', '-z'] + list(opts), capture_output=True, check=True).stdout
	return [p for p in out.decode('utf-8').split('\0') if p]

def tracked():
	return git_files('--cached', '--others', '--exclude-standard')  # the working tree: new files not yet committed too

def modules(build):
	# the DynamicLibrary projects of the build (projects.cmake, written by linux/vcxproj.py)
	txt = open(os.path.join(build, 'projects.cmake')).read()
	return sorted(m + '.so' for m in re.findall(r'^set\(SSV_(\w+)_TYPE DynamicLibrary\)', txt, re.M))

def place(p, manual):
	# path in the zip, or None: the Orbiter folder's layout, as the Windows release
	if p in ('Config/Earth.cfg', 'Config/Base.cfg'):
		return None  # the setup script adds SSV's lines to ours
	if p.startswith('Textures/Earth/'):
		return INSTALL + 'Earth/' + p[len('Textures/Earth/'):]  # installed by the setup script, never over a file of ours
	if p.startswith(('Config/', 'Meshes/', 'Textures/', 'Sound/', 'Scenarios/', 'Missions/', INSTALL)):
		return p
	if p.startswith(DOC):
		rest = p[len(DOC):]
		if rest.startswith(('CommunityToolkit.Mvvm/', 'Newtonsoft.Json/')):
			return None  # the Mission Editor's libraries; the editor is Windows only
		if manual and (rest.endswith('.tex') or rest.startswith('Images/')):
			return None  # the manual's sources: the PDF is in the package
		return p
	if p in ('SSV-LICENSE.txt', 'SSV Release Notes.txt'):
		return p
	return None  # sources, build files, README.md: in the repository

def main():
	build, manual, outdir = sys.argv[1], sys.argv[2], sys.argv[3]
	manual = None if manual == '-' else manual
	head = subprocess.run(['git', '-C', REPO, 'rev-parse', '--short', 'HEAD'], capture_output=True, check=True, text=True).stdout.strip()
	dirty = subprocess.run(['git', '-C', REPO, 'status', '--porcelain', '--untracked-files=no'], capture_output=True, text=True).stdout.strip()
	files = {}
	untracked, missing = [], []
	others = set(git_files('--others', '--exclude-standard'))
	for p in tracked():
		z = place(p, manual)
		if not z:
			continue
		if not os.path.isfile(os.path.join(REPO, p)):
			missing.append(p)  # deleted in the working tree
			continue
		if p in others:
			untracked.append(p)
		files[z] = os.path.join(REPO, p)
	for p in untracked:
		print('untracked, packed:', p)
	for p in missing:
		print('missing, skipped:', p)
	dirty = dirty or untracked
	files[INSTALL + 'ssv_linux_setup.py'] = os.path.join(REPO, 'linux', 'ssv_linux_setup.py')
	files[DOC + 'SSV-Linux.txt'] = os.path.join(REPO, 'linux', 'SSV-Linux.txt')
	if manual:
		files[DOC + 'Space Shuttle Vessel Manual.pdf'] = manual
	mods = modules(build)
	absent = [m for m in mods if not os.path.isfile(os.path.join(build, m))]
	if not mods or absent:
		sys.exit('modules not built in %s: %s' % (build, ', '.join(absent) or 'no DynamicLibrary project'))
	stripped = os.path.join(outdir, 'modules')
	os.makedirs(stripped, exist_ok=True)
	for m in mods:
		s = os.path.join(stripped, m)
		subprocess.run(['strip', '--strip-unneeded', '-o', s, os.path.join(build, m)], check=True)
		files['Modules/' + m] = s
	zpath = os.path.join(outdir, NAME)
	with zipfile.ZipFile(zpath, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
		for name in sorted(files):
			info = zipfile.ZipInfo.from_file(files[name], name)
			info.compress_type = zipfile.ZIP_DEFLATED
			if name.endswith(('.so', '.py')):
				info.external_attr = (0o100755 << 16)
			with open(files[name], 'rb') as f:
				z.writestr(info, f.read(), compresslevel=9)
	# check: every entry reads back with its CRC, and matches its source
	bad = 0
	with zipfile.ZipFile(zpath) as z:
		if z.testzip() is not None:
			bad += 1
		for info in z.infolist():
			with open(files[info.filename], 'rb') as f:
				if zlib.crc32(f.read()) & 0xffffffff != info.CRC:
					bad += 1
	tops = sorted({n.split('/')[0] for n in files})
	print('%s: %d files, %.1f MB, from %s%s; top folders %s; %s' % (zpath, len(files), os.path.getsize(zpath) / 1e6, head, ' (uncommitted changes)' if dirty else '', ', '.join(tops), 'CRC OK' if not bad else '%d BAD' % bad))
	sys.exit(1 if bad else 0)

if __name__ == '__main__':
	main()
