#!/usr/bin/env bash
# Optional interactive login and CLI. No services or global audio configuration.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
exec python3 "$root/integrations/spotify/spotify_client.py" "$@"
