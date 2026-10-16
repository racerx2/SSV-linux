#!/bin/bash
# Space Shuttle Vessel, Linux port: release candidates in one command, built as Orbiter's (AlmaLinux 9 root). Modified by racerx2, 2026.
# Licensed under the GNU General Public License version 2 or any later version (see SSV-LICENSE.txt).
# usage: linux/rc.sh build | package [rcN] | release [rcN]
set -euo pipefail
H=$(dirname "$(readlink -f "$0")"); SRC=$(readlink -f "$H/.."); cd "$SRC"
ROOT=${ORBITER_RELEASE_ROOT:-$HOME/Projects/orbiter-collision/out/pkg/alma9} # Orbiter's release root (its packaging/linux/rc.sh)
ORB=$(readlink -f "${ORBITER_SRC:-$SRC/../Orbiter-linux-custom}") # the Orbiter repo: its release build and packages
REPO=${SSV_RELEASE_REPO:-racerx2/SSV-linux}
BRANCH=linux
VER=1.16-linux-1.0; TAGP=$VER-
OUT=$SRC/out/portable; W=/work/$(basename "$SRC"); WO=/work/$(basename "$ORB")
die() { echo "rc: STOP: $*" >&2; exit 1; }
say() { echo "rc: $*" >&2; }
[ "$(dirname "$ORB")" = "$(dirname "$SRC")" ] || die "the Orbiter repo must sit next to this one (ORBITER_SRC)"

inroot() { # fake root in the AlmaLinux folder (user namespace, no privileges); the repos' parent is /work
	nice -n 10 bwrap --unshare-user --uid 0 --gid 0 --cap-add ALL --bind "$ROOT" / --dev /dev --proc /proc --tmpfs /tmp \
		--ro-bind /etc/resolv.conf /etc/resolv.conf --share-net --bind "$(dirname "$SRC")" /work \
		--setenv HOME /root --setenv PATH /usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin --setenv LANG C.UTF-8 \
		--chdir / "$@"
}

build() { # the modules against Orbiter's release build, with its compiler and Qt
	[ -f "$ORB/out/portable/build/Orbitersdk/lib/cmake/Orbiter/OrbiterConfig.cmake" ] || die "no Orbiter release build: run packaging/linux/rc.sh build in $ORB"
	local qt; qt=$(ls "$ROOT/opt/Qt" | grep -m1 '^6\.') || die "no Qt in $ROOT/opt/Qt"
	mkdir -p "$OUT"
	inroot bash -c "set -e; source /opt/rh/gcc-toolset-14/enable
		cmake -S '$W' -B '$W/out/portable/build' -G Ninja -DCMAKE_BUILD_TYPE=Release -DORBITER_ROOT='$WO/out/portable/build' -DCMAKE_PREFIX_PATH=/opt/Qt/$qt/gcc_64
		cmake --build '$W/out/portable/build' -j $(( $(nproc) > 3 ? $(nproc) - 2 : 1 ))" > "$OUT/build.log" 2>&1 \
		|| { tail -15 "$OUT/build.log" >&2; die "release build failed: $OUT/build.log"; }
}

clean() { [ -z "$(git status --porcelain --untracked-files=no)" ] || die "uncommitted changes"; }

token() { GH_TOKEN=$(printf 'protocol=https\nhost=github.com\n\n' | git credential fill 2>/dev/null | awk -F= '/^password=/{print $2}'); export GH_TOKEN; [ -n "$GH_TOKEN" ] || die "no GitHub token in git's credential store"; }

nextver() { # one more than the highest 1.16-linux-1.0-rcN
	local n; n=$(gh release list -R "$REPO" -L 100 --json tagName -q '.[].tagName' | sed -n "s/^${TAGP}rc\([0-9][0-9]*\)$/\1/p" | sort -n | tail -1) || die "cannot list the releases of $REPO"
	echo "rc$(( ${n:-0} + 1 ))"
}

orbpkg() { # the newest Orbiter package: name without .tar.xz
	local t; t=$(ls "$ORB"/out/portable/Orbiter2027-Linux-x86_64-rc*.tar.xz 2>/dev/null | sed -n 's/.*-rc\([0-9][0-9]*\)\.tar\.xz$/\1 &/p' | sort -n | tail -1 | cut -d' ' -f2)
	[ -n "$t" ] || die "no Orbiter package in $ORB/out/portable"
	basename "$t" .tar.xz
}

manual() { # the PDF on the host: it does not depend on the distribution
	local M=$SRC/out/manual
	rm -rf "$M"; mkdir -p "$M"
	cp -r "$SRC/Doc/Space Shuttle Vessel/." "$M/"
	(cd "$M" && for i in 1 2 3; do pdflatex -interaction=nonstopmode -halt-on-error "Space Shuttle Vessel Manual.tex" > "$OUT/manual.log" 2>&1 || exit 1; done) || die "manual: $OUT/manual.log"
	echo "$M/Space Shuttle Vessel Manual.pdf"
}

check() { # $1 zip: over the newest Orbiter package, unpacked with a temp HOME
	local T=$OUT/check H0=$HOME O; O=$(orbpkg)
	rm -rf "$T"; mkdir -p "$T/home"
	(cd "$OUT" && sha256sum -c "$1.sha256" > /dev/null) || die "sha256 of $1"
	tar -xf "$ORB/out/portable/$O.tar.xz" -C "$T"
	cd "$T/$O"
	local clash; clash=$(python3 -c "import os,sys,zipfile; print('\n'.join(n for n in zipfile.ZipFile(sys.argv[1]).namelist() if not n.endswith('/') and os.path.exists(n)))" "$OUT/$1")
	[ -z "$clash" ] || die "the zip replaces files of Orbiter: $(echo "$clash" | head -3)"
	python3 -c "import sys,zipfile; zipfile.ZipFile(sys.argv[1]).extractall('.')" "$OUT/$1"
	chmod +x Modules/SSV_*.so
	local g; g=$(objdump -T Modules/SSV_*.so | sed -n 's/.*GLIBC_\([0-9][0-9.]*\).*/\1/p' | sort -V | tail -1)
	[ "$(printf '%s\n2.34\n' "$g" | sort -V | tail -1)" = "2.34" ] || die "the modules need glibc $g (the package: 2.34)"
	g=$(objdump -T Modules/SSV_*.so | sed -n 's/.*GLIBCXX_\([0-9][0-9.]*\).*/\1/p' | sort -V | tail -1)
	[ "$(printf '%s\n3.4.29\n' "$g" | sort -V | tail -1)" = "3.4.29" ] || die "the modules need GLIBCXX $g (the package: 3.4.29)"
	export HOME=$T/home XDG_DATA_HOME=$T/home/.local/share XDG_CONFIG_HOME=$T/home/.config XDG_CACHE_HOME=$T/home/.cache
	local ICD=/usr/share/vulkan/icd.d/lvp_icd.json # SSV needs a graphics client (its surfaces): Vulkan on the CPU in a virtual X server, nothing on the desktop
	[ -f "$ICD" ] || die "no lavapipe ($ICD, mesa-vulkan-drivers) for the client check"
	printf 'StartPaused = FALSE\nFullscreen = FALSE\nWindowWidth = 1280\nWindowHeight = 720\nDeviceIndex = 0\nACTIVE_MODULES\nVulkanClient\nEND_MODULES\n' > Orbiter.cfg
	env -u WAYLAND_DISPLAY QT_QPA_PLATFORM=xcb VK_DRIVER_FILES=$ICD VK_ICD_FILENAMES=$ICD LP_NUM_THREADS=8 timeout 600 xvfb-run -a -s "-screen 0 1280x800x24" \
		./Orbiter "--scenariox=Space Shuttle Vessel/Launch test" --fixedstep=0.05 --maxframes=200 > "$T/client.out" 2>&1 || die "the SSV Launch test failed: $T/client.out, $T/$O/Orbiter.log"
	cp Orbiter.log "$T/client.log"
	local n m; n=$(unzip -Z1 "$OUT/$1" | grep -c '^Modules/SSV_.*\.so$' || true); m=$(grep -c 'Module SSV_[A-Za-z0-9_]*\.so' Orbiter.log || true)
	[ "$m" -ge 5 ] || die "the Launch test loaded $m SSV modules: $T/client.log"
	[ "$(grep -c '(SSV[A-Za-z0-9_]*) \[\(ERROR\|FATAL ERROR\)\]' Orbiter.log || true)" = 0 ] || die "SSV errors in the Launch test: $T/client.log"
	local z; z=$(sed -n 's/.*TileStream: base Cape Canaveral: \([0-9][0-9]*\) clear zones.*/\1/p' Orbiter.log | head -1) # only SSV's Canaveral.cfg has them
	[ "${z:-0}" -ge 2 ] || die "SSV's Cape Canaveral did not load from Config/Earth.d (no clear zones): $T/client.log"
	export HOME=$H0; unset XDG_DATA_HOME XDG_CONFIG_HOME XDG_CACHE_HOME
	cd "$SRC"
	say "checks pass over $O: sha256, no Orbiter file replaced, glibc 2.34, nothing to run, Launch test with the Vulkan client ($m SSV module loads, $n modules in the zip, SSV's bases with $z clear zones)"
}

package() { # $1 rcN
	clean
	local Z=SSV-$VER-$1-x64.zip M
	say "packaging $1 from $(git log -1 --format='%h %s'), against $(orbpkg)"
	mkdir -p "$OUT"
	M=$(manual)
	rm -f "$OUT/$Z.commit"
	build
	python3 "$SRC/linux/mkpkg.py" "$OUT/build" "$M" "$OUT" "$Z" > "$OUT/mkpkg.log" 2>&1 || { cat "$OUT/mkpkg.log" >&2; die "zip"; }
	(cd "$OUT" && sha256sum "$Z" > "$Z.sha256")
	check "$Z"
	echo "$(git rev-parse HEAD) $(git rev-parse 'HEAD^{tree}') $(orbpkg)" > "$OUT/$Z.commit"
	say "package: $OUT/$Z ($(stat -c %s "$OUT/$Z") bytes)"
}

notes() { # $1 rcN, $2 previous tag or "", $3 output, $4 Orbiter package
	local C=$OUT/changes-$1.md G=$OUT/changes-$1.auto.md
	if [ ! -s "$C" ]; then
		if [ -n "$2" ] && git fetch -q origin "refs/tags/$2" 2> /dev/null; then git log --first-parent --format='- %s' FETCH_HEAD..HEAD > "$G"
		else git log --first-parent --format='- %s' -12 > "$G"; fi
		C=$G
		say "no $OUT/changes-$1.md: notes list the commits"
	fi
	python3 - "$H/notes.md" "$C" "$3" "$1" "SSV-$VER-$1-x64.zip" "$4" <<'EOF'
import re, sys
t, c, o, rc, name, orb = sys.argv[1:7]
orbrc = re.sub(r'.*-(rc\d+)$', r'\1', orb).upper()
s = open(t).read().replace('@CHANGES@', open(c).read().strip()).replace('@NAME@', name).replace('@VER@', rc.upper()).replace('@ORB@', orbrc)
open(o, 'w').write(s)
EOF
}

release() { # $1 rcN
	local Z=SSV-$VER-$1-x64.zip TAG=$TAGP$1 n PREV NOTES=$OUT/notes-$1.md R O
	clean
	[ "$(cut -d' ' -f1,2 "$OUT/$Z.commit" 2>/dev/null)" = "$(git rev-parse HEAD) $(git rev-parse 'HEAD^{tree}')" ] || package "$1"
	(cd "$OUT" && sha256sum -c "$Z.sha256" > /dev/null) || die "sha256 of $Z"
	O=$(cut -d' ' -f3 "$OUT/$Z.commit")
	R=$(git ls-remote origin "refs/heads/$BRANCH" | cut -f1)
	if [ "$R" != "$(git rev-parse HEAD)" ]; then
		git merge-base --is-ancestor "$R" HEAD || die "origin $BRANCH has commits that are not here"
		git push -q origin "HEAD:refs/heads/$BRANCH"
	fi
	n=${1#rc}; PREV=""; [ "$n" -gt 1 ] 2> /dev/null && PREV=${TAGP}rc$(( n - 1 ))
	notes "$1" "$PREV" "$NOTES" "$O"
	if gh release view "$TAG" -R "$REPO" > /dev/null 2>&1; then # a re-run: same files on a commit with our tree
		git fetch -q origin "refs/tags/$TAG"
		[ "$(git rev-parse 'FETCH_HEAD^{tree}')" = "$(git rev-parse 'HEAD^{tree}')" ] || die "$TAG exists with other files"
		gh release upload "$TAG" -R "$REPO" --clobber "$OUT/$Z" "$OUT/$Z.sha256"
	else
		gh release create "$TAG" -R "$REPO" --target "$(git rev-parse HEAD)" --prerelease --title "Space Shuttle Vessel 1.16 for Orbiter 2027 Linux, ${1^^}" \
			--notes-file "$NOTES" "$OUT/$Z" "$OUT/$Z.sha256" > /dev/null
	fi
	local D SUM; SUM=$(cut -d' ' -f1 "$OUT/$Z.sha256")
	D=$(gh api "repos/$REPO/releases/tags/$TAG" --jq ".assets[] | select(.name == \"$Z\") | .digest // \"\"")
	[ -z "$D" ] || [ "$D" = "sha256:$SUM" ] || die "GitHub's digest $D is not our sha256"
	say "$REPO: https://github.com/$REPO/releases/tag/$TAG ($(git rev-parse --short HEAD), for $O)"
}

case "${1:-}" in
	build) build; say "release build compiles (out/portable/build)";;
	package) token; package "${2:-$(nextver)}";;
	release) token; release "${2:-$(nextver)}";;
	*) echo "usage: linux/rc.sh build | package [rcN] | release [rcN]" >&2; exit 2;;
esac
