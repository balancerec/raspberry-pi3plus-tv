#!/usr/bin/env bash
# The default Linux launcher now starts the native app. Web legacy stays separate.
set -euo pipefail
exec bash "$(dirname -- "${BASH_SOURCE[0]}")/run-native.sh" "$@"
