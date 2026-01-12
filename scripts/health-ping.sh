#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
health-ping.sh — периодически дергает /health, чтобы генерировать логи

Использование:
  ./scripts/health-ping.sh [-i SECONDS] [-c COUNT] [-t TIMEOUT] [--experiment URL] [--auth URL]

Параметры:
  -i SECONDS   Интервал между итерациями (по умолчанию: 2)
  -c COUNT     Кол-во итераций (0 = бесконечно) (по умолчанию: 0)
  -t TIMEOUT   Таймаут curl в секундах (по умолчанию: 3)
  --experiment URL  URL health для experiment-service (по умолчанию: http://localhost:8002/health)
  --auth URL        URL health для auth-service (по умолчанию: http://localhost:8001/health)
  -h          Помощь

Также можно задавать через env:
  INTERVAL_SEC, COUNT, TIMEOUT_SEC, EXPERIMENT_HEALTH_URL, AUTH_HEALTH_URL

Примеры:
  ./scripts/health-ping.sh -i 1 -c 30
  INTERVAL_SEC=5 ./scripts/health-ping.sh
USAGE
}

EXPERIMENT_HEALTH_URL="${EXPERIMENT_HEALTH_URL:-http://localhost:8002/health}"
AUTH_HEALTH_URL="${AUTH_HEALTH_URL:-http://localhost:8001/health}"
INTERVAL_SEC="${INTERVAL_SEC:-2}"
COUNT="${COUNT:-0}"          # 0 = бесконечно
TIMEOUT_SEC="${TIMEOUT_SEC:-3}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    -i)
      INTERVAL_SEC="${2:-}"; shift 2 ;;
    -c)
      COUNT="${2:-}"; shift 2 ;;
    -t)
      TIMEOUT_SEC="${2:-}"; shift 2 ;;
    --experiment)
      EXPERIMENT_HEALTH_URL="${2:-}"; shift 2 ;;
    --auth)
      AUTH_HEALTH_URL="${2:-}"; shift 2 ;;
    -h|--help)
      usage; exit 0 ;;
    *)
      echo "Unknown arg: $1" >&2
      usage
      exit 2 ;;
  esac
done

log() { printf '[%s] %s\n' "$(date -Is)" "$*"; }

hit() {
  local name="$1" url="$2"
  local code
  code="$(curl -sS -o /dev/null -w "%{http_code}" --max-time "$TIMEOUT_SEC" "$url" || echo "curl_error")"
  log "$name -> $url -> $code"
}

if ! [[ "$INTERVAL_SEC" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
  echo "INTERVAL_SEC must be a number, got: $INTERVAL_SEC" >&2
  exit 2
fi
if ! [[ "$COUNT" =~ ^[0-9]+$ ]]; then
  echo "COUNT must be an integer, got: $COUNT" >&2
  exit 2
fi
if ! [[ "$TIMEOUT_SEC" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
  echo "TIMEOUT_SEC must be a number, got: $TIMEOUT_SEC" >&2
  exit 2
fi

log "Start: interval=${INTERVAL_SEC}s, count=${COUNT} (0=∞), timeout=${TIMEOUT_SEC}s"
log "Experiment: $EXPERIMENT_HEALTH_URL"
log "Auth:       $AUTH_HEALTH_URL"

i=0
while true; do
  i=$((i + 1))
  hit "experiment-service" "$EXPERIMENT_HEALTH_URL"
  hit "auth-service" "$AUTH_HEALTH_URL"

  if [[ "$COUNT" -ne 0 && "$i" -ge "$COUNT" ]]; then
    log "Done ($i iterations)"
    exit 0
  fi

  sleep "$INTERVAL_SEC"
done

