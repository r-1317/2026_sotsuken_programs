from __future__ import annotations

import csv
from pathlib import Path

import matplotlib.pyplot as plt

try:
	import japanize_matplotlib  # noqa: F401
except Exception:
	pass


BASE_DIR = Path(__file__).resolve().parent
OUT_DIR = BASE_DIR / "out"
GRAPH_PATH = OUT_DIR / "memory_usage_graph.png"


def load_series(csv_path: Path) -> tuple[list[int], list[int], list[int]]:
	x_values: list[int] = []
	existing_values: list[int] = []
	new_values: list[int] = []

	with csv_path.open(newline="", encoding="utf-8") as f:
		reader = csv.DictReader(f)
		for row in reader:
			x_values.append(int(row[reader.fieldnames[0]]))
			existing_values.append(int(row["existing_memory_kb"]))
			new_values.append(int(row["new_memory_kb"]))

	return x_values, existing_values, new_values


def plot_dataset(ax: plt.Axes, csv_path: Path, title: str, x_label: str) -> None:
	x_values, existing_values, new_values = load_series(csv_path)

	ax.plot(x_values, existing_values, marker="o", linewidth=2, label="existing.out")
	ax.plot(x_values, new_values, marker="o", linewidth=2, label="new.out")
	ax.set_title(title)
	ax.set_xlabel(x_label)
	ax.set_ylabel("Peak resident memory [KB]")
	ax.grid(True, alpha=0.3)
	ax.legend()


def main() -> None:
	OUT_DIR.mkdir(exist_ok=True)

	fig, axes = plt.subplots(3, 1, figsize=(12, 16), sharey=False)
	fig.suptitle("Memory usage comparison", fontsize=16)

	datasets = [
		(BASE_DIR / "out" / "loops_memory_usage.csv", "Effect of loops", "loops"),
		(BASE_DIR / "out" / "branch_memory_usage.csv", "Effect of branch", "branch"),
		(BASE_DIR / "out" / "padding_size_memory_usage.csv", "Effect of padding-size", "padding-size"),
	]

	for ax, (csv_path, title, x_label) in zip(axes, datasets, strict=True):
		plot_dataset(ax, csv_path, title, x_label)

	fig.tight_layout(rect=(0, 0, 1, 0.97))
	fig.savefig(GRAPH_PATH, dpi=200, bbox_inches="tight")

	backend = plt.get_backend().lower()
	if backend != "agg":
		plt.show()


if __name__ == "__main__":
	main()