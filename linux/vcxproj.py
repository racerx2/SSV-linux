#!/usr/bin/env python3
# Space Shuttle Vessel - Linux port, modified by racerx2, 2026
# Licensed under the GNU General Public License version 2 or any later version (see SSV-LICENSE.txt)
# not upstream: reads the Visual Studio projects (the source of truth) and writes the CMake lists of the Linux build
import os, re, sys

def on_disk(path):
	# the path with each part in the letter case found on disk, as the Windows file system matches it
	path = os.path.normpath(path.replace('\\', '/'))
	if os.path.exists(path):
		return path
	parts = path.split('/')
	cur = '/' if path.startswith('/') else ''
	for p in parts:
		if p in ('', '.'):
			continue
		nxt = os.path.join(cur, p)
		if not os.path.exists(nxt) and os.path.isdir(cur or '.'):
			m = [x for x in os.listdir(cur or '.') if x.lower() == p.lower()]
			if m:
				nxt = os.path.join(cur, m[0])
		cur = nxt
	return cur

def built(item):
	# an item not excluded from the Release|Win32 build (ExcludedFromBuild unconditional or for that configuration)
	for cond, val in re.findall(r'<ExcludedFromBuild(?:\s+Condition="([^"]*)")?\s*>\s*([^<]*)</ExcludedFromBuild>', item or ''):
		if val.strip().lower() == 'true' and (not cond or 'Release|Win32' in cond):
			return False
	return True

def cm(s):
	return '"%s"' % s.replace('\\', '/').replace('"', '\\"')

# the Release|Win32 linker inputs and their imported targets of the Orbiter CMake package
LIBS = {
	'orbiter.lib': None,  # the core API: the Orbiter executable exports it, resolved when the module loads
	'orbitersdk.lib': 'Orbiter::Orbitersdk',
	'dlgctrl.lib': 'Orbiter::DlgCtrl',
	'xrsound.lib': 'Orbiter::XRSound',
	'lua.lib': 'Orbiter::lua',
}
# Windows system and C runtime libraries: nothing to link on Linux
WINDOWS_LIBS = {
	'%(additionaldependencies)', 'kernel32.lib', 'user32.lib', 'gdi32.lib', 'winspool.lib', 'comdlg32.lib',
	'advapi32.lib', 'shell32.lib', 'ole32.lib', 'oleaut32.lib', 'uuid.lib', 'odbc32.lib', 'odbccp32.lib',
	'winmm.lib', 'msimg32.lib', 'comctl32.lib', 'shlwapi.lib', 'version.lib', 'imm32.lib', 'ws2_32.lib',
	'legacy_stdio_definitions.lib',
}

def libs(txt, typ):
	# the AdditionalDependencies of the Release|Win32 configuration, mapped to targets; unknown names returned apart
	grp = re.search(r"<ItemDefinitionGroup Condition=\"'\$\(Configuration\)\|\$\(Platform\)'=='Release\|Win32'\">(.*?)</ItemDefinitionGroup>", txt, re.S)
	tool = 'Lib' if typ == 'StaticLibrary' else 'Link'
	sect = re.search(r'<%s>(.*?)</%s>' % (tool, tool), grp.group(1), re.S) if grp else None
	deps = re.findall(r'<AdditionalDependencies>([^<]*)</AdditionalDependencies>', sect.group(1)) if sect else []
	targets, unknown = [], []
	for lib in (l.strip() for d in deps for l in d.split(';')):
		if not lib or lib.lower() in WINDOWS_LIBS:
			continue
		if lib.lower() not in LIBS:
			unknown.append(lib)
		elif LIBS[lib.lower()] and LIBS[lib.lower()] not in targets:
			targets.append(LIBS[lib.lower()])
	return targets, unknown

def main(src, out):
	projects = {}
	guids = {}
	gens = []
	for root, dirs, files in sorted(os.walk(src)):
		for f in sorted(files):
			if not f.endswith('.vcxproj'):
				continue
			p = os.path.join(root, f)
			txt = open(p, encoding='utf-8-sig', errors='replace').read()
			name = re.sub(r'(_2017)?\.vcxproj$', '', f)
			guid = re.search(r'<ProjectGuid>\{([^}]+)\}', txt, re.I)
			if guid:
				guids[guid.group(1).lower()] = name
			typ = re.search(r'<ConfigurationType>([^<]+)', txt)
			srcs = [on_disk(os.path.join(root, s)) for s, item in re.findall(r'<ClCompile Include="([^"]+)"\s*(?:/>|>(.*?)</ClCompile>)', txt, re.S) if built(item)]
			rcs = [on_disk(os.path.join(root, s)) for s in re.findall(r'<ResourceCompile Include="([^"]+)"', txt)]
			refs = re.findall(r'<ProjectReference Include="[^"]+">\s*<Project>\{([^}]+)\}', txt, re.I)
			typ = typ.group(1) if typ else ''
			lt, lu = libs(txt, typ)
			projects[name] = dict(dir=root, type=typ, src=srcs, rc=rcs, refs=[r.lower() for r in refs], libs=lt, unknown_libs=lu)
			# CustomBuild commands of the Release configuration: the generated headers
			for m in re.finditer(r'<CustomBuild Include="([^"]+)">(.*?)</CustomBuild>', txt, re.S):
				cmd = re.search(r"<Command Condition=\"'\$\(Configuration\)\|\$\(Platform\)'=='Release\|Win32'\">(.*?)</Command>", m.group(2), re.S)
				if not cmd:
					continue
				for line in cmd.group(1).replace('&quot;', '"').splitlines():
					line = line.strip()
					t = re.match(r'"\$\(SolutionDir\)utils\\(\w+)\\\$\(Configuration\)\\\w+\.exe"\s*(.*)', line)
					if not t:
						continue
					tool, args = t.group(1), t.group(2)
					toks = re.findall(r'(-\w)?"([^"]*)"|(\S+)', args)
					argv = []
					for opt, q, plain in toks:
						a = (opt or '') + q if q or opt else plain
						a = a.replace('$(SolutionDir)', src + '/')
						if a.startswith('-o'):
							a = '-o' + os.path.normpath(a[2:].replace('\\', '/'))
						elif not a.startswith('-'):
							a = on_disk(a)
						argv.append(a)
					gens.append((tool, argv))
	unknown = ['%s: {%s}' % (n, r) for n, pr in sorted(projects.items()) if pr['src'] for r in pr['refs'] if r not in guids]
	if unknown:
		sys.exit('unknown ProjectReference: ' + ', '.join(unknown))
	unknown = ['%s: %s' % (n, l) for n, pr in sorted(projects.items()) if pr['src'] for l in pr['unknown_libs']]
	if unknown:
		sys.exit('unknown library (add it to LIBS or WINDOWS_LIBS in linux/vcxproj.py): ' + ', '.join(unknown))
	nolink = [n for n, pr in sorted(projects.items()) if pr['src'] and pr['type'] == 'DynamicLibrary' and not pr['libs']]
	if nolink:
		sys.exit('no Orbiter library in the Release|Win32 Link AdditionalDependencies: ' + ', '.join(nolink))
	with open(out, 'w') as fo:
		fo.write('# generated by linux/vcxproj.py from the Visual Studio projects: do not edit\n')
		for name in sorted(projects):
			pr = projects[name]
			if not pr['src']:
				continue
			fo.write('set(SSV_%s_TYPE %s)\n' % (name, pr['type']))
			fo.write('set(SSV_%s_DIR %s)\n' % (name, cm(pr['dir'])))
			fo.write('set(SSV_%s_SRC\n%s)\n' % (name, ''.join('\t%s\n' % cm(s) for s in pr['src'])))
			fo.write('set(SSV_%s_RC %s)\n' % (name, ' '.join(cm(r) for r in pr['rc'])))
			fo.write('set(SSV_%s_REFS %s)\n' % (name, ' '.join(guids[r] for r in pr['refs'])))
			fo.write('set(SSV_%s_LIBS %s)\n' % (name, ' '.join(pr['libs'])))
			fo.write('list(APPEND SSV_PROJECTS %s)\n' % name)
		for tool, argv in gens:
			fo.write('list(APPEND SSV_GEN_%s %s)\n' % (tool, cm('|'.join(argv))))
	print('%d projects, %d generated headers' % (sum(1 for p in projects.values() if p['src']), len(gens)))

if __name__ == '__main__':
	main(os.path.abspath(sys.argv[1]), sys.argv[2])
