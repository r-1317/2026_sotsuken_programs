#!/bin/bash
set -euo pipefail

# 使用例: ./run-a05.sh --time 1.9 --loops 1000
# MAX_JOBS=1 で逐次実行、OUTPUT_FILE=/tmp/results.csv でCSVの保存先を変更できる。
script_dir=$(cd -- "$(dirname -- "$0")" && pwd)
cd "$script_dir"

MAX_JOBS=${MAX_JOBS:-5}
output_file=${OUTPUT_FILE:-out05/a05_results.csv}
if [[ ! "$MAX_JOBS" =~ ^[1-9][0-9]*$ ]]; then
  printf 'MAX_JOBS must be a positive integer\n' >&2
  exit 1
fi
if [[ ! -x /usr/bin/time ]]; then
  printf '/usr/bin/time is required to measure peak memory usage\n' >&2
  exit 1
fi

shopt -s nullglob
inputs=(in/[0-9][0-9][0-9][0-9].txt)
if (( ${#inputs[@]} == 0 )); then
  printf 'No test cases found in %s/in\n' "$script_dir" >&2
  exit 1
fi

mkdir -p out05 "$(dirname -- "$output_file")"
# ソースの変更を必ず反映するため、実行のたびにコンパイルする。
"${CXX:-g++}" -std=c++17 -O2 a05.cpp -o a05.out

tmp_dir=$(mktemp -d)
trap 'rm -rf -- "$tmp_dir"' EXIT

run_case() {
  local input_file="$1"
  shift
  local case_number=${input_file##*/}
  case_number=${case_number%.txt}
  local stderr_file="out05/out05_${case_number}.stderr.txt"
  local memory_file="$tmp_dir/${case_number}.memory"
  local memory_kb path_length final_layer_visits

  # %M はプロセスの最大RSS（KiB）。計測結果と探索の標準エラーを分けて保存する。
  if ! /usr/bin/time -f '%M' -o "$memory_file" ./a05.out "$@" \
      < "$input_file" > "out05/out05_${case_number}.txt" 2> "$stderr_file"; then
    printf 'Case %s failed. See %s\n' "$case_number" "$stderr_file" >&2
    cat "$stderr_file" >&2
    return 1
  fi

  memory_kb=$(< "$memory_file")
  path_length=$(sed -n 's/^Total path length: \([0-9][0-9]*\)$/\1/p' "$stderr_file")
  final_layer_visits=$(sed -n 's/^Final layer visits: \([0-9][0-9]*\)$/\1/p' "$stderr_file")
  if [[ ! "$memory_kb" =~ ^[0-9]+$ || ! "$path_length" =~ ^[0-9]+$ ||
        ! "$final_layer_visits" =~ ^[0-9]+$ ]]; then
    printf 'Could not read statistics for case %s. See %s\n' \
      "$case_number" "$stderr_file" >&2
    return 1
  fi

  # 並列ジョブごとに別ファイルへ書き、最後にケース番号順で結合する。
  printf '%s,%s,%s,%s\n' "$case_number" "$memory_kb" "$path_length" \
    "$final_layer_visits" > "$tmp_dir/${case_number}.csv"
  printf 'Case %s: memory=%s KiB, path=%s, final_layer_visits=%s\n' \
    "$case_number" "$memory_kb" "$path_length" "$final_layer_visits"
}

pids=()
failed=0
wait_for_cases() {
  local pid
  for pid in "${pids[@]}"; do
    if ! wait "$pid"; then
      failed=1
    fi
  done
  pids=()
}

for input_file in "${inputs[@]}"; do
  run_case "$input_file" "$@" &
  pids+=("$!")
  if (( ${#pids[@]} >= MAX_JOBS )); then
    wait_for_cases
    if (( failed )); then
      exit 1
    fi
  fi
done
wait_for_cases
if (( failed )); then
  exit 1
fi

printf 'case_number,peak_memory_kb,total_path_length,final_layer_visits\n' > "$output_file"
cat "$tmp_dir"/*.csv >> "$output_file"
printf 'Saved %s cases to %s\n' "${#inputs[@]}" "$output_file"
