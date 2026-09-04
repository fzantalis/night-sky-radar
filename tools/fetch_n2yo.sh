#!/usr/bin/env bash
# Captures an N2YO visual-pass prediction as a fixture for test_validation.
#
# N2YO is a DEVELOPMENT-TIME ORACLE ONLY. No firmware code may ever call it.
# Register for a free key at n2yo.com/api and export it - never commit it.
#
#   export N2YO_API_KEY=...
#   ./tools/fetch_n2yo.sh 37.98 23.73 0.1
#
# Output: test/fixtures/n2yo_passes.txt, one pass per line:
#   <startUnix> <maxElDeg> <durationSec>

set -euo pipefail

if [[ -z "${N2YO_API_KEY:-}" ]]; then
  echo "N2YO_API_KEY is not set" >&2
  exit 1
fi

LAT="${1:?usage: fetch_n2yo.sh <lat> <lon> <alt_km>}"
LON="${2:?usage: fetch_n2yo.sh <lat> <lon> <alt_km>}"
ALT="${3:?usage: fetch_n2yo.sh <lat> <lon> <alt_km>}"

NORAD=25544          # ISS
DAYS=5
MIN_VIS_SEC=60

mkdir -p test/fixtures

# UNIT TRAP: our Observer stores altitude in KILOMETRES, N2YO's API takes
# METRES. Convert here, and write the km value to the observer fixture so the
# test feeds our code the units it expects.
ALT_M=$(awk -v a="$ALT" 'BEGIN { printf "%.0f", a * 1000 }')

URL="https://api.n2yo.com/rest/v1/satellite/visualpasses/${NORAD}/${LAT}/${LON}/${ALT_M}/${DAYS}/${MIN_VIS_SEC}/&apiKey=${N2YO_API_KEY}"

# NEVER echo/log $URL - it embeds the API key. Only its JSON response is
# written to disk, and only to the gitignored fixture files below.
curl -sS "$URL" \
  | jq -r '.passes[] | "\(.startUTC) \(.maxEl) \(.endUTC - .startUTC)"' \
  > test/fixtures/n2yo_passes.txt

echo "captured $(wc -l < test/fixtures/n2yo_passes.txt) passes"
echo "Also record the observer used, so the test matches:"
printf '%s %s %s\n' "$LAT" "$LON" "$ALT" > test/fixtures/n2yo_observer.txt
