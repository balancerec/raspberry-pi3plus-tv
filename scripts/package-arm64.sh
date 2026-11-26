#!/usr/bin/env bash
# Clean build + CTest + native ARM64 package. Never installs or starts the app.
set -euo pipefail
umask 077
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
for script in "$root"/scripts/*.sh; do bash -n "$script"; done
[ "$(uname -m)" = aarch64 ] || { printf 'Este pacote exige build nativo aarch64 no destino.\n' >&2; exit 1; }
bash "$root/scripts/diagnose-native.sh"
revision=$(git -C "$root" rev-parse --short=12 HEAD)
[ -z "$(git -C "$root" status --porcelain --untracked-files=no)" ] || { printf 'Commit as alteracoes rastreadas antes de empacotar.\n' >&2; exit 1; }
release="1.0.0-rc1-$revision"
work=$(mktemp -d "${TMPDIR:-/tmp}/raspberry-tv-build.XXXXXXXX")
printf 'Build isolado e logs preservados em: %s\n' "$work"
cmake -S "$root/native" -B "$work/build" -DCMAKE_BUILD_TYPE=Release -DBUILD_NATIVE_APP=ON -DBUILD_TESTING=ON 2>&1 | tee "$work/configure.log"
cmake --build "$work/build" --parallel "${JOBS:-1}" 2>&1 | tee "$work/compile.log"
ctest --test-dir "$work/build" --output-on-failure 2>&1 | tee "$work/ctest.log"
stage="$work/raspberry-tv-$release-arm64"
cmake --install "$work/build" --prefix "$stage"
mkdir -p "$stage/scripts" "$stage/docs" "$stage/config" "$stage/validation"
cp -- "$root"/scripts/{run-native,diagnose-native,install-native,rollback-native,uninstall-native,release-common,validate-device,spotify}.sh "$stage/scripts/"
cp -- "$root"/docs/{ARCHITECTURE,INSTALLATION,CONTROLS,PLAYLISTS,TROUBLESHOOTING,TEST_REPORT,RELEASE_CHECKLIST}.md "$stage/docs/"
cp -- "$root/docs/SPOTIFY.md" "$stage/docs/"
cp -- "$root/README.md" "$root/CHANGELOG.md" "$stage/"
cp -- "$root/config/defaults.conf" "$stage/config/"
cp -- "$work"/{configure,compile,ctest}.log "$stage/validation/"
printf '%s\n' "$release" > "$stage/RELEASE_ID"
printf 'raspberry-tv-managed-release-v1\n' > "$stage/.raspberry-tv-release"
(cd "$stage" && find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS)
mkdir -p "$root/dist"
package="$root/dist/raspberry-tv-$release-arm64.tar.gz"
[ ! -e "$package" ] || { printf 'Pacote ja existe; nao sera sobrescrito: %s\n' "$package" >&2; exit 1; }
tar -C "$work" -czf "$package" "$(basename "$stage")"
(cd "$root/dist" && sha256sum "$(basename "$package")") > "$package.sha256"
printf 'Pacote gerado (validacao fisica ainda necessaria): %s\n' "$package"
