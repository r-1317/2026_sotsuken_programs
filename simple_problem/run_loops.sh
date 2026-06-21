#!/bin/bash

# existing.outとnew.outについて、--loopsを1000から5000まで100回刻みで実行するループを作成する
# そのときのメモリ使用量を測定し、結果を./out/loops_memory_usage.csvに出力する
# csvには、ループ数、existing.outのメモリ使用量、new.outのメモリ使用量を出力する

set -euo pipefail

script_dir=$(cd "$(dirname "$0")" && pwd)
cd "$script_dir"

mkdir -p ./out

if [[ ! -x ./existing.out ]]; then
	g++ -O2 -o existing.out existing.cpp
fi

if [[ ! -x ./new.out ]]; then
	g++ -O2 -o new.out new.cpp
fi

measure_peak_memory_kb() {
	local binary_path="$1"
	local loops="$2"
	local tmp_file
	tmp_file=$(mktemp)

	/usr/bin/time -f '%M' -o "$tmp_file" "$binary_path" --loops "$loops" >/dev/null 2>/dev/null

	tr -d '[:space:]' < "$tmp_file"
	rm -f "$tmp_file"
}

output_file=./out/loops_memory_usage.csv

echo "loops,existing_memory_kb,new_memory_kb" > "$output_file"

for loops in $(seq 1000 100 5000); do
	existing_memory=$(measure_peak_memory_kb ./existing.out "$loops")
	new_memory=$(measure_peak_memory_kb ./new.out "$loops")
	echo "$loops,$existing_memory,$new_memory" >> "$output_file"
done