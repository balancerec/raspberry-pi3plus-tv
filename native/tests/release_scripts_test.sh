#!/usr/bin/env bash
# Filesystem integration simulation; does NOT validate the ARM executable/hardware.
set -euo pipefail
root=$1
for script in "$root"/scripts/*.sh; do bash -n "$script"; done
if [ "$(id -u)" -eq 0 ]; then printf 'Non-root installer test requires ordinary user\n'; exit 77; fi
test_home=$(mktemp -d /tmp/rtv-install-test.XXXXXXXX)
cleanup() { case "$test_home" in /tmp/rtv-install-test.*) rm -rf -- "$test_home" ;; esac; }
trap cleanup EXIT
export HOME="$test_home"
export RASPBERRY_TV_HOME="$HOME/app"
mkdir "$HOME/mock-bin"
printf '#!/bin/sh\nprintf "aarch64\\n"\n' > "$HOME/mock-bin/uname"
chmod +x "$HOME/mock-bin/uname"
export PATH="$HOME/mock-bin:$PATH"
make_fixture() {
  local revision=$1 package="$HOME/pkg-$1"
  mkdir -p "$package/bin" "$package/scripts"
  cp "$root"/scripts/{install-native,rollback-native,uninstall-native,release-common}.sh "$package/scripts/"
  printf '#!/bin/sh\nexit 0\n' > "$package/bin/raspberry-tv-native"
  chmod +x "$package/bin/raspberry-tv-native"
  printf 'raspberry-tv-managed-release-v1\n' > "$package/.raspberry-tv-release"
  printf '1.0.0-rc1-%s\n' "$revision" > "$package/RELEASE_ID"
  (cd "$package" && find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS)
}
make_fixture aaaaaaaaaaaa
bash "$HOME/pkg-aaaaaaaaaaaa/scripts/install-native.sh"
[ "$(basename "$(readlink -f "$HOME/app/current")")" = 1.0.0-rc1-aaaaaaaaaaaa ]
make_fixture bbbbbbbbbbbb
bash "$HOME/pkg-bbbbbbbbbbbb/scripts/install-native.sh"
[ "$(basename "$(readlink -f "$HOME/app/previous")")" = 1.0.0-rc1-aaaaaaaaaaaa ]
bash "$HOME/app/current/scripts/rollback-native.sh"
[ "$(basename "$(readlink -f "$HOME/app/current")")" = 1.0.0-rc1-aaaaaaaaaaaa ]
if bash "$HOME/pkg-aaaaaaaaaaaa/scripts/install-native.sh"; then printf 'Duplicate install must fail\n'; exit 1; fi
mkdir "$HOME/shared"
if RASPBERRY_TV_HOME="$HOME/shared" bash "$HOME/pkg-bbbbbbbbbbbb/scripts/install-native.sh"; then printf 'Unmanaged prefix must be rejected\n'; exit 1; fi
mkdir -p "$HOME/.config/raspberry-tv";printf 'user data\n' > "$HOME/.config/raspberry-tv/keep"
bash "$HOME/app/current/scripts/uninstall-native.sh" --all
[ ! -e "$HOME/app" ] && [ -f "$HOME/.config/raspberry-tv/keep" ] && [ -d "$HOME/shared" ]
printf 'Install/update/rollback/uninstall filesystem simulation passed\n'
