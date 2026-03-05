#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DURATION_HOURS="12"
MAX_RUNS="0"
P99_THRESHOLD="0.10"
SLEEP_SECONDS="0"
CONFIG="Release"
PLATFORM="linux"
LINUX_BACKEND="headless"
TEST_EXECUTABLE=""
SKIP_BUILD="0"
CLEAN_CONFIG_ARTIFACTS="1"

log_step() {
  echo "[validate_release.sh] $*"
}

usage() {
  cat <<'EOF'
Usage: scripts/validate_release.sh [options]

Options:
  --project-root <path>         Project root (default: script_dir/..)
  --duration-hours <float>      Endurance duration in hours (default: 12)
  --max-runs <int>              Stop after N runs (0 = unlimited by count)
  --p99-threshold <float>       Allowed p99 drift ratio (default: 0.10)
  --sleep-seconds <int>         Sleep between runs (default: 0)
  --config <name>               Build configuration (default: Release)
  --linux-backend <name>        linux backend for jenga (default: headless)
  --test-exe <path>             Override test executable path
  --no-clean                    Do not clean Build artifacts for this config
  --skip-build                  Skip jenga build
  --help                        Show this help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --project-root) PROJECT_ROOT="$2"; shift 2 ;;
    --duration-hours) DURATION_HOURS="$2"; shift 2 ;;
    --max-runs) MAX_RUNS="$2"; shift 2 ;;
    --p99-threshold) P99_THRESHOLD="$2"; shift 2 ;;
    --sleep-seconds) SLEEP_SECONDS="$2"; shift 2 ;;
    --config) CONFIG="$2"; shift 2 ;;
    --linux-backend) LINUX_BACKEND="$2"; shift 2 ;;
    --test-exe) TEST_EXECUTABLE="$2"; shift 2 ;;
    --no-clean) CLEAN_CONFIG_ARTIFACTS="0"; shift 1 ;;
    --skip-build) SKIP_BUILD="1"; shift 1 ;;
    --help|-h) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage; exit 2 ;;
  esac
done

if [[ "$PLATFORM" != "linux" ]]; then
  echo "This script is dedicated to Linux/WSL validation." >&2
  exit 2
fi

if ! command -v jenga >/dev/null 2>&1; then
  echo "Command 'jenga' not found in PATH." >&2
  exit 2
fi

resolve_test_exe() {
  local root="$1"
  local cfg="$2"
  local override="$3"

  if [[ -n "$override" ]]; then
    if [[ ! -f "$override" ]]; then
      echo "Test executable not found: $override" >&2
      return 1
    fi
    echo "$override"
    return 0
  fi

  local candidates=(
    "$root/Build/Tests/${cfg}-Linux/NKMemory_Tests"
    "$root/Build/Tests/${cfg}-Linux/NKMemory_Tests.exe"
    "$root/Build/Tests/${cfg}/NKMemory_Tests"
  )

  for c in "${candidates[@]}"; do
    if [[ -f "$c" ]]; then
      echo "$c"
      return 0
    fi
  done

  local found
  found="$(find "$root/Build/Tests" -maxdepth 3 -type f -name "NKMemory_Tests*" 2>/dev/null | head -n1 || true)"
  if [[ -n "$found" ]]; then
    echo "$found"
    return 0
  fi

  echo "Unable to locate NKMemory_Tests executable." >&2
  return 1
}

timestamp="$(date -u +%Y%m%d-%H%M%S)"
report_root="$PROJECT_ROOT/Build/Reports/NKMemory/Validation-Linux-${CONFIG}-${timestamp}"
log_dir="$report_root/logs"
mkdir -p "$log_dir"

runs_csv="$report_root/runs.csv"
bench_csv="$report_root/benchmark.csv"
reg_csv="$report_root/regressions.csv"
summary_json="$report_root/summary.json"
summary_md="$report_root/summary.md"

echo "run,start_iso,end_iso,duration_ms,exit_code,tests_passed,tests_total,asserts_passed,asserts_total" >"$runs_csv"
echo "run,name,avg_ns,p99_ns,failures,samples" >"$bench_csv"

pushd "$PROJECT_ROOT" >/dev/null
if [[ "$SKIP_BUILD" != "1" ]]; then
  if [[ "$CLEAN_CONFIG_ARTIFACTS" == "1" ]]; then
    log_step "Cleaning config artifacts for ${CONFIG}-Linux"
    rm -rf \
      "$PROJECT_ROOT/Build/Obj/${CONFIG}-Linux" \
      "$PROJECT_ROOT/Build/Lib/${CONFIG}-Linux" \
      "$PROJECT_ROOT/Build/Tests/${CONFIG}-Linux"
  fi

  log_step "Building project: jenga build --platform linux --config $CONFIG --linux-backend $LINUX_BACKEND"
  jenga build --platform linux --config "$CONFIG" --linux-backend "$LINUX_BACKEND"
fi

test_exe="$(resolve_test_exe "$PROJECT_ROOT" "$CONFIG" "$TEST_EXECUTABLE")"
log_step "Using test executable: $test_exe"

start_iso="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
duration_seconds="$(awk -v h="$DURATION_HOURS" 'BEGIN { printf("%d", h*3600) }')"
deadline_epoch="$(( $(date +%s) + duration_seconds ))"

run=0
while true; do
  now_epoch="$(date +%s)"
  if (( now_epoch >= deadline_epoch )); then
    break
  fi
  if (( MAX_RUNS > 0 && run >= MAX_RUNS )); then
    break
  fi

  run="$((run + 1))"
  log_step "Run #$run"
  run_start_iso="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  run_start_ns="$(date +%s%N)"
  log_file="$log_dir/run_$(printf "%05d" "$run").log"

  set +e
  "$test_exe" >"$log_file" 2>&1
  exit_code=$?
  set -e

  run_end_ns="$(date +%s%N)"
  run_end_iso="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  duration_ms="$(awk -v s="$run_start_ns" -v e="$run_end_ns" 'BEGIN { printf("%.3f", (e-s)/1000000.0) }')"

  cat "$log_file"

  tests_line="$(grep -E 'Tests[[:space:]]*:' "$log_file" | tail -n1 || true)"
  asserts_line="$(grep -E 'Assertions[[:space:]]*:' "$log_file" | tail -n1 || true)"

  tests_pair="$(echo "$tests_line" | sed -nE 's/.*Tests[[:space:]]*:[[:space:]]*([0-9]+)[^0-9]+([0-9]+).*/\1,\2/p')"
  if [[ -z "$tests_pair" ]]; then tests_pair="0,0"; fi
  IFS=',' read -r tests_passed tests_total <<<"$tests_pair"

  asserts_pair="$(echo "$asserts_line" | sed -nE 's/.*Assertions[[:space:]]*:[[:space:]]*([0-9]+)[^0-9]+([0-9]+).*/\1,\2/p')"
  if [[ -z "$asserts_pair" ]]; then asserts_pair="0,0"; fi
  IFS=',' read -r asserts_passed asserts_total <<<"$asserts_pair"

  echo "$run,$run_start_iso,$run_end_iso,$duration_ms,$exit_code,$tests_passed,$tests_total,$asserts_passed,$asserts_total" >>"$runs_csv"

  while IFS= read -r line; do
    name="$(echo "$line" | sed -nE 's/^[[:space:]]*-[[:space:]]*([A-Za-z0-9_]+).*/\1/p')"
    avg="$(echo "$line" | sed -nE 's/.*avg=[[:space:]]*([0-9]+(\.[0-9]+)?).*/\1/p')"
    p99="$(echo "$line" | sed -nE 's/.*p99=[[:space:]]*([0-9]+(\.[0-9]+)?).*/\1/p')"
    pair="$(echo "$line" | sed -nE 's/.*failures=([0-9]+)\/([0-9]+).*/\1,\2/p')"
    if [[ -z "$name" || -z "$avg" || -z "$p99" || -z "$pair" ]]; then
      continue
    fi
    IFS=',' read -r fail_num fail_den <<<"$pair"
    echo "$run,$name,$avg,$p99,$fail_num,$fail_den" >>"$bench_csv"
  done < <(grep -E '^[[:space:]]*-[[:space:]]+[A-Za-z0-9_]+[[:space:]]+avg=' "$log_file" || true)

  if (( exit_code != 0 )); then
    log_step "Stopping campaign on first failure (exit code $exit_code)."
    break
  fi

  if (( SLEEP_SECONDS > 0 )); then
    sleep "$SLEEP_SECONDS"
  fi
done

end_iso="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

total_runs="$(awk -F, 'NR>1{c++} END{print c+0}' "$runs_csv")"
run_failures="$(awk -F, 'NR>1 && $5 != 0 {c++} END{print c+0}' "$runs_csv")"
bench_failures="$(awk -F, 'NR>1 {s+=$5} END{print s+0}' "$bench_csv")"

awk -F, -v thr="$P99_THRESHOLD" '
BEGIN{
  print "name,baseline_p99_ns,max_p99_ns,drift_ratio"
}
NR>1{
  name=$2
  p99=$4+0
  if(!(name in base)){base[name]=p99}
  if(!(name in max) || p99>max[name]){max[name]=p99}
}
END{
  for(name in base){
    if(base[name] > 0){
      drift=(max[name]-base[name])/base[name]
      if(drift > thr){
        printf("%s,%.6f,%.6f,%.6f\n", name, base[name], max[name], drift)
      }
    }
  }
}
' "$bench_csv" >"$reg_csv"

reg_count="$(awk -F, 'NR>1{c++} END{print c+0}' "$reg_csv")"

status="GO"
if (( total_runs == 0 || run_failures > 0 || bench_failures > 0 || reg_count > 0 )); then
  status="NO-GO"
fi

cat >"$summary_json" <<EOF
{
  "status": "$status",
  "platform": "linux",
  "config": "$CONFIG",
  "start_utc": "$start_iso",
  "end_utc": "$end_iso",
  "duration_hours_requested": $DURATION_HOURS,
  "runs_executed": $total_runs,
  "run_failures": $run_failures,
  "benchmark_failures": $bench_failures,
  "p99_regressions": $reg_count,
  "p99_regression_threshold": $P99_THRESHOLD,
  "artifacts": {
    "logs_dir": "$log_dir",
    "runs_csv": "$runs_csv",
    "benchmark_csv": "$bench_csv",
    "regressions_csv": "$reg_csv",
    "summary_md": "$summary_md"
  }
}
EOF

latest_run="$(awk -F, 'NR>1{if($1>m)m=$1} END{print m+0}' "$bench_csv")"

{
  echo "# NKMemory Release Validation (Linux/WSL)"
  echo
  echo "- Status: **$status**"
  echo "- Platform: Linux/WSL"
  echo "- Config: $CONFIG"
  echo "- Runs: $total_runs"
  echo "- Run failures: $run_failures"
  echo "- Benchmark failures: $bench_failures"
  echo "- P99 regressions: $reg_count (threshold=$P99_THRESHOLD)"
  echo
  echo "## Artifacts"
  echo
  echo "- JSON summary: \`$summary_json\`"
  echo "- Runs CSV: \`$runs_csv\`"
  echo "- Benchmark CSV: \`$bench_csv\`"
  echo "- Regressions CSV: \`$reg_csv\`"
  echo "- Logs dir: \`$log_dir\`"
  echo
  echo "## Latest Run Benchmarks"
  echo
  echo "| Allocator | Avg(ns) | P99(ns) | Failures | Samples |"
  echo "|---|---:|---:|---:|---:|"
  awk -F, -v latest="$latest_run" 'NR>1 && $1==latest {printf("| %s | %s | %s | %s | %s |\n", $2, $3, $4, $5, $6)}' "$bench_csv"
  if [[ "$latest_run" == "0" ]]; then
    echo "| (none) | - | - | - | - |"
  fi
  echo
  echo "## GO / NO-GO Rule"
  echo
  echo "- GO if: no test run failure, no benchmark failure, no p99 regression over threshold."
  echo "- NO-GO otherwise."
} >"$summary_md"

log_step "Validation completed with status: $status"
log_step "Summary JSON: $summary_json"
log_step "Summary Markdown: $summary_md"

popd >/dev/null
