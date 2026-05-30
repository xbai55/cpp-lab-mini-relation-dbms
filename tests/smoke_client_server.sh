#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PORT="${MINIDB_PORT:-19090}"
SERVER_BIN="${1:-$ROOT_DIR/build/mini_db_server}"
CLIENT_BIN="${2:-$ROOT_DIR/build/mini_db_client}"
SERVER_LOG="$(mktemp)"
CLIENT_OUT="$(mktemp)"

cleanup() {
    if [[ -n "${SERVER_PID:-}" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -rf "$ROOT_DIR/data"
    rm -f "$SERVER_LOG" "$CLIENT_OUT"
}
trap cleanup EXIT

cd "$ROOT_DIR"
rm -rf data

"$SERVER_BIN" "$PORT" >"$SERVER_LOG" 2>&1 &
SERVER_PID=$!
sleep 1

"$CLIENT_BIN" 127.0.0.1 "$PORT" >"$CLIENT_OUT" <<'SQL'
create database person
use person
create table person (id int primary, name string)
insert person values (1001, "peter")
select name from person where id = 1001
update person set name = "tom" where id = 1001
select * from person
delete person where id = 1001
select * from person
exit
SQL

grep -q "peter" "$CLIENT_OUT"
grep -q "tom" "$CLIENT_OUT"
grep -q "0 rows in set" "$CLIENT_OUT"

echo "Smoke test passed."
echo "--- Client output ---"
cat "$CLIENT_OUT"
