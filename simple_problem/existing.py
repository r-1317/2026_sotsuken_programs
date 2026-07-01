from __future__ import annotations

import heapq
import random
import sys
import time
from collections import deque
from dataclasses import dataclass, field


@dataclass
class TreeNode:
    parent: int = -1
    children: list[int] = field(default_factory=list)
    turn: int = 0
    action_id: int = -1


@dataclass
class State:
    turn: int = 0
    f_sum: float = 0.0
    g_sum_known: float = 0.0
    g_sum_true: float = 0.0
    pending_g: deque[float] = field(default_factory=deque)
    tree_index: int = -1
    padding: bytearray = field(default_factory=bytearray)
    order: int = 0

    def eval(self) -> float:
        return self.f_sum + self.g_sum_known

    def true_score(self) -> float:
        return self.f_sum + self.g_sum_true


def usage(prog: str) -> None:
    print(
        f"Usage: {prog} [--loops N] [--branch N] [--turns N] [--delay L] [--seed N] [--padding-size N]",
        file=sys.stderr,
    )
    print("Defaults: loops=1000 branch=30 turns=100 delay=10 seed=1 padding-size=0", file=sys.stderr)


def parse_args(argv: list[str]) -> tuple[int, int, int, int, int, int]:
    loops = 1000
    branch = 30
    max_turn = 100
    delay = 10
    seed = 1
    padding_size = 0

    i = 1
    while i < len(argv):
        a = argv[i]
        if a == "--loops":
            if i + 1 >= len(argv):
                print(f"Missing value for {a}", file=sys.stderr)
                usage(argv[0])
                raise SystemExit(1)
            loops = int(argv[i + 1])
            i += 2
        elif a == "--branch":
            if i + 1 >= len(argv):
                print(f"Missing value for {a}", file=sys.stderr)
                usage(argv[0])
                raise SystemExit(1)
            branch = int(argv[i + 1])
            i += 2
        elif a == "--turns":
            if i + 1 >= len(argv):
                print(f"Missing value for {a}", file=sys.stderr)
                usage(argv[0])
                raise SystemExit(1)
            max_turn = int(argv[i + 1])
            i += 2
        elif a == "--delay":
            if i + 1 >= len(argv):
                print(f"Missing value for {a}", file=sys.stderr)
                usage(argv[0])
                raise SystemExit(1)
            delay = int(argv[i + 1])
            i += 2
        elif a == "--seed":
            if i + 1 >= len(argv):
                print(f"Missing value for {a}", file=sys.stderr)
                usage(argv[0])
                raise SystemExit(1)
            seed = int(argv[i + 1])
            i += 2
        elif a == "--padding-size":
            if i + 1 >= len(argv):
                print(f"Missing value for {a}", file=sys.stderr)
                usage(argv[0])
                raise SystemExit(1)
            padding_size = int(argv[i + 1])
            i += 2
        elif a in ("-h", "--help"):
            usage(argv[0])
            raise SystemExit(0)
        else:
            print(f"Unknown argument: {a}", file=sys.stderr)
            usage(argv[0])
            raise SystemExit(1)

    if loops <= 0 or branch <= 0 or max_turn <= 0 or delay < 0 or padding_size < 0:
        print("Invalid argument.", file=sys.stderr)
        usage(argv[0])
        raise SystemExit(1)

    return loops, branch, max_turn, delay, seed, padding_size


def main(argv: list[str]) -> int:
    start_time = time.process_time()
    loops, branch, max_turn, delay, seed, padding_size = parse_args(argv)

    rng = random.Random(seed)
    pq: list[list[tuple[float, int, State]]] = [[] for _ in range(max_turn + 1)]
    tree: list[TreeNode] = []
    next_order = 0

    root = State(padding=bytearray(padding_size))
    root.turn = 0
    root.tree_index = 0
    root.order = next_order
    next_order += 1

    tree.append(TreeNode(parent=-1, children=[], turn=0, action_id=-1))
    heapq.heappush(pq[0], (-root.eval(), root.order, root))

    for _ in range(loops):
        for t in range(max_turn):
            if not pq[t]:
                continue

            _, _, cur = heapq.heappop(pq[t])

            for k in range(branch):
                f = rng.random()
                g = rng.random()

                nxt = State(
                    turn=cur.turn,
                    f_sum=cur.f_sum,
                    g_sum_known=cur.g_sum_known,
                    g_sum_true=cur.g_sum_true,
                    pending_g=deque(cur.pending_g),
                    tree_index=cur.tree_index,
                    padding=bytearray(cur.padding),
                    order=0,
                )

                nxt.turn = cur.turn + 1
                nxt.f_sum += f
                nxt.g_sum_true += g
                nxt.pending_g.append(g)

                if len(nxt.pending_g) > delay:
                    nxt.g_sum_known += nxt.pending_g.popleft()

                node_index = len(tree)
                tree.append(TreeNode(parent=cur.tree_index, children=[], turn=nxt.turn, action_id=k))
                tree[cur.tree_index].children.append(node_index)
                nxt.tree_index = node_index

                nxt.order = next_order
                next_order += 1
                heapq.heappush(pq[t + 1], (-nxt.eval(), nxt.order, nxt))

    best: State | None = None
    if pq[max_turn]:
        best = pq[max_turn][0][2]
    else:
        for t in range(max_turn - 1, -1, -1):
            if pq[t]:
                best = pq[t][0][2]
                break

    if best is None:
        print("No state found.", file=sys.stderr)
        return 1

    path_nodes: list[int] = []
    v = best.tree_index
    while v != -1:
        path_nodes.append(v)
        v = tree[v].parent
    path_nodes.reverse()

    elapsed_time = time.process_time() - start_time
    print(f"elapsed_time = {elapsed_time} seconds", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))