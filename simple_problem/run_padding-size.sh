#!/bin/bash

# existing.outとnew.outについて、--padding-sizeを0から4000まで100回刻みで実行するループを作成する
# そのときのメモリ使用量を測定し、結果を./out/padding_size_memory_usage.csvに出力する
# csvには、padding-size、existing.outのメモリ使用量、new.outのメモリ使用量を出力する

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
	local padding_size="$2"
	local tmp_file
	tmp_file=$(mktemp)

	/usr/bin/time -f '%M' -o "$tmp_file" "$binary_path" --padding-size "$padding_size" >/dev/null 2>/dev/null

	tr -d '[:space:]' < "$tmp_file"
	rm -f "$tmp_file"
}

output_file=./out/padding_size_memory_usage.csv

echo "padding_size,existing_memory_kb,new_memory_kb" > "$output_file"

for padding_size in $(seq 0 100 4000); do
	existing_memory=$(measure_peak_memory_kb ./existing.out "$padding_size")
	new_memory=$(measure_peak_memory_kb ./new.out "$padding_size")
	echo "$padding_size,$existing_memory,$new_memory" >> "$output_file"
done