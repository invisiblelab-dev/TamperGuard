#!/usr/bin/env bash
#
# Run a Filebench workload multiple times with a fixed interval between runs.
#
# Example:
#   ./scripts/filebench/filebench_repeats.sh --workload /path/to/workload.f --repeats 5
#   ./scripts/filebench/filebench_repeats.sh --workload workloads/webserver.f --repeats 3 --interval 30
#   ./scripts/filebench/filebench_repeats.sh --workload workloads/webserver.f --workload workloads/fileserver.f --repeats 3
#   ./scripts/filebench/filebench_repeats.sh --workload my.f --repeats 10 --filebench-bin /usr/local/bin/filebench
#

set -euo pipefail

WORKLOADS=()
REPEATS=""
INTERVAL="10"
FILEBENCH_BIN="filebench"
LOG_DIR=""
AGGREGATE="1"
CLEAN_DIR=""

usage() {
  cat <<'EOF'
Usage:
  filebench_repeats.sh --workload PATH [--workload PATH ...] --repeats N [--interval SECONDS] [--filebench-bin PATH] [--log-dir DIR] [--clean-dir DIR]

Required:
  --workload PATH          Workload file path passed to: filebench -f PATH (can be repeated)
  --repeats N              Number of runs to execute (integer > 0)

Optional:
  --interval SECONDS       Sleep interval between runs (default: 10; integer >= 0)
  --filebench-bin PATH     Filebench binary (default: filebench)
  --log-dir DIR            Where to write per-run logs (default: /tmp/filebench_runs_YYYYmmdd_HHMMSS)
  --clean-dir DIR          After EACH run, delete contents of this directory (directory itself is kept)
  --no-aggregate            Do not generate all_runs.log / averages.tsv at the end
  -h, --help               Show this help

Notes:
  - Each workload gets its own subdir under log_dir.
  - Each run is logged to: <workload>/<workload>_run_XXX.log
  - Exit code is non-zero if any run fails.
EOF
}

is_int() {
  [[ "${1:-}" =~ ^-?[0-9]+$ ]]
}

safe_realpath() {
  if command -v realpath >/dev/null 2>&1; then
    realpath "$1"
  else
    readlink -f "$1"
  fi
}

clean_dir_contents() {
  local dir="$1"
  [[ -n "$dir" ]] || return 0

  if [[ ! -d "$dir" ]]; then
    echo "Error: --clean-dir is not a directory: $dir" >&2
    return 2
  fi

  # Safety: resolve symlinks/.. and refuse obviously dangerous targets.
  local rp
  rp="$(safe_realpath "$dir" 2>/dev/null || true)"
  [[ -n "$rp" ]] || { echo "Error: unable to resolve --clean-dir: $dir" >&2; return 2; }

  case "$rp" in
    "/"|"/home"|"/root"|"/tmp")
      echo "Error: refusing to clean dangerous directory: $rp" >&2
      return 2
      ;;
  esac
  if [[ "$rp" == "$HOME" || "$rp" == "$HOME/"* ]]; then
    echo "Error: refusing to clean within HOME: $rp" >&2
    return 2
  fi

  # Delete contents, keep the directory.
  # Includes dotfiles via the ./* and ..?* globs.
  rm -rf -- "$rp"/* "$rp"/.[!.]* "$rp"/..?* 2>/dev/null || true
}

sanitize_workload_name() {
  # Convert a workload path into a safe prefix for file/dir names.
  # Example: workloads/webserver.f -> webserver
  local w="$1"
  local base
  base="$(basename "$w")"
  base="${base%.f}"
  # Replace any non-alnum with underscores.
  base="$(echo "$base" | tr -c 'A-Za-z0-9' '_')"
  # Trim leading/trailing underscores.
  base="${base##_}"
  base="${base%%_}"
  [[ -n "$base" ]] || base="workload"
  printf '%s' "$base"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --workload)
      [[ -n "${2:-}" ]] || { echo "Error: --workload requires a value" >&2; usage; exit 2; }
      WORKLOADS+=("$2")
      shift 2
      ;;
    --repeats)
      [[ -n "${2:-}" ]] || { echo "Error: --repeats requires a value" >&2; usage; exit 2; }
      REPEATS="$2"
      shift 2
      ;;
    --interval)
      [[ -n "${2:-}" ]] || { echo "Error: --interval requires a value" >&2; usage; exit 2; }
      INTERVAL="$2"
      shift 2
      ;;
    --filebench-bin)
      [[ -n "${2:-}" ]] || { echo "Error: --filebench-bin requires a value" >&2; usage; exit 2; }
      FILEBENCH_BIN="$2"
      shift 2
      ;;
    --log-dir)
      [[ -n "${2:-}" ]] || { echo "Error: --log-dir requires a value" >&2; usage; exit 2; }
      LOG_DIR="$2"
      shift 2
      ;;
    --clean-dir)
      [[ -n "${2:-}" ]] || { echo "Error: --clean-dir requires a value" >&2; usage; exit 2; }
      CLEAN_DIR="$2"
      shift 2
      ;;
    --no-aggregate)
      AGGREGATE="0"
      shift 1
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Error: Unknown option: $1" >&2
      usage
      exit 2
      ;;
  esac
done

[[ ${#WORKLOADS[@]} -gt 0 ]] || { echo "Error: --workload is required (pass at least one)" >&2; usage; exit 2; }
[[ -n "$REPEATS" ]] || { echo "Error: --repeats is required" >&2; usage; exit 2; }

if ! is_int "$REPEATS" || [[ "$REPEATS" -le 0 ]]; then
  echo "Error: --repeats must be an integer > 0 (got: $REPEATS)" >&2
  exit 2
fi
if ! is_int "$INTERVAL" || [[ "$INTERVAL" -lt 0 ]]; then
  echo "Error: --interval must be an integer >= 0 (got: $INTERVAL)" >&2
  exit 2
fi

for w in "${WORKLOADS[@]}"; do
  if [[ ! -f "$w" ]]; then
    echo "Error: workload file not found: $w" >&2
    exit 2
  fi
done

if ! command -v "$FILEBENCH_BIN" >/dev/null 2>&1; then
  # If user passed an absolute path, command -v can fail; check directly.
  if [[ ! -x "$FILEBENCH_BIN" ]]; then
    echo "Error: filebench binary not found/executable: $FILEBENCH_BIN" >&2
    exit 2
  fi
fi

if [[ -z "$LOG_DIR" ]]; then
  LOG_DIR="/tmp/filebench_runs_$(date +%Y%m%d_%H%M%S)"
fi
mkdir -p "$LOG_DIR"

if [[ -n "$CLEAN_DIR" ]]; then
  # Validate early so we fail fast before starting any workload.
  clean_dir_contents "$CLEAN_DIR" >/dev/null
fi

aggregate_outputs() {
  local dir="$1"
  local prefix="$2"
  local all_log="$dir/all_runs.log"
  local avg_tsv="$dir/averages.tsv"
  local avg_log="$dir/averages.log"

  # 1) Concatenate all runs with simple headers.
  : >"$all_log"
  for f in "$dir"/"${prefix}"_run_*.log; do
    [[ -f "$f" ]] || continue
    {
      echo "===== $(basename "$f") ====="
      cat "$f"
      echo
    } >>"$all_log"
  done

  # 2) Compute averages for per-op breakdown lines and IO Summary.
  # Produces a TSV that's easy to post-process.
  awk '
    function add_op(name, ops, ops_s, mb_s, ms_op, min_ms, max_ms) {
      op_cnt[name]++
      op_sum_ops[name] += ops
      op_sum_ops_s[name] += ops_s
      op_sum_mb_s[name] += mb_s
      op_sum_ms_op[name] += ms_op
      if (min_ms != "") op_sum_min_ms[name] += min_ms
      if (max_ms != "") op_sum_max_ms[name] += max_ms
      if (min_ms != "") op_cnt_minmax[name]++
    }
    function add_io(ops, ops_s, rd, wr, mb_s, ms_op) {
      io_cnt++
      io_sum_ops += ops
      io_sum_ops_s += ops_s
      io_sum_rd += rd
      io_sum_wr += wr
      io_sum_mb_s += mb_s
      io_sum_ms_op += ms_op
    }
    BEGIN {
      OFS = "\t"
    }
    {
      # Per-operation breakdown line example:
      # readfile1 87860ops 2927ops/s 365.9mb/s 0.090ms/op [0.025ms - 15.439ms]
      if (match($0, /^([A-Za-z0-9_.-]+)[[:space:]]+([0-9]+)ops[[:space:]]+([0-9.]+)ops\/s[[:space:]]+([0-9.]+)mb\/s[[:space:]]+([0-9.]+)ms\/op/, m)) {
        name = m[1]
        ops = m[2] + 0
        ops_s = m[3] + 0
        mb_s = m[4] + 0
        ms_op = m[5] + 0
        min_ms = ""
        max_ms = ""
        if (match($0, /\[([0-9.]+)ms[[:space:]]*-[[:space:]]*([0-9.]+)ms\]/, r)) {
          min_ms = r[1] + 0
          max_ms = r[2] + 0
        }
        add_op(name, ops, ops_s, mb_s, ms_op, min_ms, max_ms)
        next
      }

      # IO Summary line example:
      # 35.039: IO Summary: 2723603 ops 90747.243 ops/s 29273/2927 rd/wr 3682.0mb/s 0.037ms/op
      if (match($0, /IO Summary:[[:space:]]*([0-9]+)[[:space:]]*ops[[:space:]]*([0-9.]+)[[:space:]]*ops\/s[[:space:]]*([0-9]+)\/([0-9]+)[[:space:]]*rd\/wr[[:space:]]*([0-9.]+)mb\/s[[:space:]]*([0-9.]+)ms\/op/, s)) {
        add_io(s[1] + 0, s[2] + 0, s[3] + 0, s[4] + 0, s[5] + 0, s[6] + 0)
        next
      }
    }
    END {
      print "section","name","samples","avg_ops","avg_ops_per_s","avg_mb_per_s","avg_ms_per_op","avg_min_ms","avg_max_ms"
      for (name in op_cnt) {
        c = op_cnt[name]
        avg_ops = op_sum_ops[name] / c
        avg_ops_s = op_sum_ops_s[name] / c
        avg_mb_s = op_sum_mb_s[name] / c
        avg_ms_op = op_sum_ms_op[name] / c
        avg_min = ""
        avg_max = ""
        if (op_cnt_minmax[name] > 0) {
          avg_min = op_sum_min_ms[name] / op_cnt_minmax[name]
          avg_max = op_sum_max_ms[name] / op_cnt_minmax[name]
        }
        print "op", name, c, avg_ops, avg_ops_s, avg_mb_s, avg_ms_op, avg_min, avg_max
      }
      if (io_cnt > 0) {
        print "io_summary","IO Summary",io_cnt,(io_sum_ops/io_cnt),(io_sum_ops_s/io_cnt),(io_sum_mb_s/io_cnt),(io_sum_ms_op/io_cnt),(io_sum_rd/io_cnt),(io_sum_wr/io_cnt)
      }
    }
  ' "$dir"/"${prefix}"_run_*.log | sort >"$avg_tsv"

  # Human-friendly view (still simple).
  {
    echo "=== Averages across runs (from per-run logs) ==="
    echo "Source dir: $dir"
    echo
    echo "Averages TSV: $avg_tsv"
    echo
    cat "$avg_tsv"
  } >"$avg_log"
}

echo "=== Filebench repeats ==="
echo "  workloads     = ${#WORKLOADS[@]}"
for w in "${WORKLOADS[@]}"; do
  echo "    - $w"
done
echo "  repeats       = $REPEATS"
echo "  interval      = ${INTERVAL}s"
echo "  filebench_bin = $FILEBENCH_BIN"
echo "  log_dir       = $LOG_DIR"
if [[ -n "$CLEAN_DIR" ]]; then
  echo "  clean_dir     = $CLEAN_DIR"
fi
echo

fail_count=0
fail_runs=()

declare -A workload_seen=()

for w in "${WORKLOADS[@]}"; do
  base_prefix="$(sanitize_workload_name "$w")"
  seen="${workload_seen[$base_prefix]:-0}"
  workload_seen[$base_prefix]=$((seen + 1))

  # If the same workload name appears multiple times, suffix it to keep outputs distinct.
  prefix="$base_prefix"
  if [[ "$seen" -ge 1 ]]; then
    prefix="${base_prefix}_$((seen + 1))"
  fi

  w_dir="$LOG_DIR/$prefix"
  mkdir -p "$w_dir"

  echo "=== Workload: $w ==="
  echo "  prefix = $prefix"
  echo "  outdir = $w_dir"
  echo

  for ((i=1; i<=REPEATS; i++)); do
    log_file="$w_dir/${prefix}_run_$(printf '%03d' "$i").log"
    start_ts="$(date -Is)"

    echo "Run $i/$REPEATS @ $start_ts"
    echo "  cmd: $FILEBENCH_BIN -f $w"
    echo "  log: $log_file"

    # Run and capture exit code without `set -e` behavior.
    set +e
    "$FILEBENCH_BIN" -f "$w" >"$log_file" 2>&1
    rc=$?
    set -e

    end_ts="$(date -Is)"
    if [[ $rc -ne 0 ]]; then
      echo "  result: FAIL (exit $rc) @ $end_ts"
      fail_count=$((fail_count + 1))
      fail_runs+=("${prefix}:run${i}:$rc")
    else
      echo "  result: OK @ $end_ts"
    fi

    if [[ -n "$CLEAN_DIR" ]]; then
      echo "  cleaning: $CLEAN_DIR"
      clean_dir_contents "$CLEAN_DIR" || { echo "  cleaning: FAIL" >&2; }
    fi

    if [[ $i -lt $REPEATS && $INTERVAL -gt 0 ]]; then
      echo "Sleeping ${INTERVAL}s..."
      sleep "$INTERVAL"
    fi
    echo
  done

  if [[ "$AGGREGATE" == "1" ]]; then
    aggregate_outputs "$w_dir" "$prefix" || true
  fi
done

echo "=== Summary ==="
echo "  workloads  = ${#WORKLOADS[@]}"
echo "  repeats    = $REPEATS"
echo "  failures   = $fail_count"
if [[ $fail_count -ne 0 ]]; then
  echo "  failed_runs(exit_code) = ${fail_runs[*]}"
  echo "Logs: $LOG_DIR"
  exit 1
fi

echo "Logs: $LOG_DIR"
exit 0

