#!/usr/bin/env python3
"""探索結果CSVのケース番号以外の各列について平均を表示する。"""

import argparse
import csv
import math
from pathlib import Path
from statistics import fmean


def calculate_averages(csv_file: Path) -> tuple[int, dict[str, float]]:
    with csv_file.open(encoding="utf-8-sig", newline="") as file:
        reader = csv.DictReader(file)
        if not reader.fieldnames or "case_number" not in reader.fieldnames:
            raise ValueError("CSVにcase_number列がありません")
        columns = [name for name in reader.fieldnames if name != "case_number"]
        if not columns:
            raise ValueError("平均を計算する列がありません")

        values: dict[str, list[float]] = {name: [] for name in columns}
        count = 0
        for row in reader:
            for name in columns:
                try:
                    value = float(row[name])
                except (ValueError, TypeError) as error:
                    raise ValueError(
                        f"{reader.line_num}行目の{name}列が数値ではありません"
                    ) from error
                if not math.isfinite(value):
                    raise ValueError(
                        f"{reader.line_num}行目の{name}列が有限の数値ではありません"
                    )
                values[name].append(value)
            count += 1

    if count == 0:
        raise ValueError("CSVにデータ行がありません")
    return count, {name: fmean(column) for name, column in values.items()}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "csv_file",
        nargs="?",
        type=Path,
        default=Path(__file__).resolve().parent / "out05-bst/a05-bst_results.csv",
        help="対象CSV（省略時: out05-bst/a05-bst_results.csv）",
    )
    args = parser.parse_args()
    try:
        count, averages = calculate_averages(args.csv_file)
    except (OSError, ValueError, csv.Error) as error:
        parser.error(str(error))

    print(f"Cases: {count}")
    for name, average in averages.items():
        print(f"{name}: {average:.6f}")


if __name__ == "__main__":
    main()
