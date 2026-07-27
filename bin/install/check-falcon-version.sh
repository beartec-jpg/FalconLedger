#!/bin/bash
# Check that this host runs the Falcon testnet pin (mainnet-v2).
# Usage: bash check-falcon-version.sh
set -euo pipefail

EXPECTED_DIGEST="sha256:9362005f1360ad102d0cd76ff53f19ce7548d8149263e50f241489e4b73f3ea5"
EXPECTED_TAG="qxrp/xrpld:mainnet-v2"
EXPECTED_NETWORK=1001

echo "=== Falcon testnet version check ==="
echo "Expected: ${EXPECTED_TAG}"
echo "Digest:   ${EXPECTED_DIGEST}"
echo "Network:  ${EXPECTED_NETWORK}"
echo

# Find a running node container
CNAME=""
for n in qxrp-full qxrp-val2 qxrp-validator falcon-validator; do
  if docker ps --format '{{.Names}}' 2>/dev/null | grep -qx "$n"; then
    CNAME="$n"
    break
  fi
done

if [[ -z "$CNAME" ]]; then
  # fallback: first container using qxrp/xrpld
  CNAME=$(docker ps --format '{{.Names}} {{.Image}}' 2>/dev/null | awk '/xrpld|qxrp\// {print $1; exit}')
fi

if [[ -z "$CNAME" ]]; then
  echo "FAIL: no qxrp/xrpld container running"
  echo "Update / reinstall with the portal one-liner or:"
  echo "  docker pull ${EXPECTED_TAG}"
  echo "  # then set compose image to ${EXPECTED_DIGEST} and: docker compose up -d"
  exit 1
fi

IMAGE=$(docker inspect "$CNAME" --format '{{.Config.Image}}')
echo "Container: $CNAME"
echo "Image:     $IMAGE"

# Resolve digest
DIGEST=$(docker image inspect "$CNAME" --format '{{range .RepoDigests}}{{println .}}{{end}}' 2>/dev/null | head -1 || true)
if [[ -z "$DIGEST" || "$DIGEST" == "<no value>" ]]; then
  DIGEST=$(docker image inspect "$IMAGE" --format '{{range .RepoDigests}}{{println .}}{{end}}' 2>/dev/null | grep sha256 | head -1 || true)
fi
echo "RepoDigest: ${DIGEST:-unknown}"

# Network id via local RPC if possible
RPC_OK=0
if docker exec "$CNAME" curl -sf --max-time 3 -X POST -d '{"method":"server_info"}' http://127.0.0.1:5005 >/tmp/qxrp-si.json 2>/dev/null \
  || docker exec "$CNAME" curl -sf --max-time 3 -X POST -d '{"method":"server_info"}' http://127.0.0.1:6005 >/tmp/qxrp-si.json 2>/dev/null; then
  python3 - <<'PY'
import json
i=json.load(open("/tmp/qxrp-si.json")).get("result",{}).get("info",{})
print("build_version:", i.get("build_version"))
print("network_id:", i.get("network_id"))
print("server_state:", i.get("server_state"))
PY
  RPC_OK=1
fi

MATCH=0
if echo "${DIGEST}" | grep -q "${EXPECTED_DIGEST#sha256:}"; then
  MATCH=1
elif echo "${IMAGE}" | grep -q "mainnet-v2"; then
  MATCH=1
elif echo "${IMAGE}" | grep -q "${EXPECTED_DIGEST}"; then
  MATCH=1
fi

echo
if [[ "$MATCH" -eq 1 ]]; then
  echo "OK — running expected Falcon testnet pin (mainnet-v2)."
  exit 0
fi

echo "NOT OK — image does not match expected pin."
echo
echo "Update with:"
echo "  export IMG='qxrp/xrpld@${EXPECTED_DIGEST}'"
echo "  docker pull \"\$IMG\""
echo "  docker tag \"\$IMG\" ${EXPECTED_TAG}"
echo "  # edit docker-compose.yml image: line to \$IMG or ${EXPECTED_TAG}"
echo "  cd /var/lib/qxrp-validator   # or /var/lib/falcon-validator"
echo "  docker compose up -d --force-recreate"
exit 2
