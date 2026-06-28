#!/bin/bash

# existing.outとnew.outについて、--branch / --loops / --padding-size を3重ループで実行する
# そのときの最大メモリ使用量を測定し、結果を./out/all_memory_usage.csvに出力する
# 実行後に出力される最大メモリ使用量が10GBを超える試行が3回続いたら打ち切る

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

memory_limit_kb=$((10 * 1024 * 1024))
early_break_within_iterations=3

measure_peak_memory_kb() {
	local binary_path="$1"
	local branch="$2"
	local loops="$3"
	local padding_size="$4"
	local tmp_file
	local result_kb=0
	tmp_file=$(mktemp)

	/usr/bin/time -f '%M' -o "$tmp_file" "$binary_path" --branch "$branch" --loops "$loops" --padding-size "$padding_size" >/dev/null 2>/dev/null || true
	result_kb=$(tr -d '[:space:]' < "$tmp_file")
	rm -f "$tmp_file"
	result_kb=${result_kb:-0}
	printf '%s\n' "$result_kb"
	if (( result_kb > memory_limit_kb )); then
		return 1
	fi
	return 0
}

output_file=./out/all_memory_usage.csv

echo "branch,loops,padding_size,existing_memory_kb,existing_status,new_memory_kb,new_status" > "$output_file"

outer_consecutive_early_break=0

for branch in $(seq 10 10 200); do
	middle_consecutive_early_break=0
	middle_broke_early=false
	middle_iteration_count=0
	for loops in $(seq 1000 100 5000); do
		middle_iteration_count=$((middle_iteration_count + 1))
		consecutive_over_limit=0
		inner_broke_by_limit=false
		inner_iteration_count=0
		for padding_size in $(seq 0 100 4000); do
			inner_iteration_count=$((inner_iteration_count + 1))
			existing_status=ok
			existing_memory=$(measure_peak_memory_kb ./existing.out "$branch" "$loops" "$padding_size") || existing_status=over_10gb

			new_status=ok
			new_memory=$(measure_peak_memory_kb ./new.out "$branch" "$loops" "$padding_size") || new_status=over_10gb

			echo "$branch,$loops,$padding_size,$existing_memory,$existing_status,$new_memory,$new_status" >> "$output_file"

			if [[ "$existing_status" == "over_10gb" || "$new_status" == "over_10gb" ]]; then
				consecutive_over_limit=$((consecutive_over_limit + 1))
			else
				consecutive_over_limit=0
			fi

			if (( consecutive_over_limit >= 3 )); then
				inner_broke_by_limit=true
				break
			fi
		done

		if [[ "$inner_broke_by_limit" == true && $inner_iteration_count -le $early_break_within_iterations ]]; then
			middle_consecutive_early_break=$((middle_consecutive_early_break + 1))
		else
			middle_consecutive_early_break=0
		fi

		if (( middle_consecutive_early_break >= 3 )); then
			if (( middle_iteration_count <= early_break_within_iterations )); then
				middle_broke_early=true
			fi
			break
		fi
	done

	if [[ "$middle_broke_early" == true ]]; then
		outer_consecutive_early_break=$((outer_consecutive_early_break + 1))
	else
		outer_consecutive_early_break=0
	fi

	if (( outer_consecutive_early_break >= 3 )); then
		break
	fi
done
