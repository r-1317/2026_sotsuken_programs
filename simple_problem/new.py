from __future__ import annotations

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
    state_id: int = 0
    padding: bytearray = field(default_factory=bytearray)

    def eval(self) -> float:
        return self.f_sum + self.g_sum_known

    def true_score(self) -> float:
        return self.f_sum + self.g_sum_true


class AVLNode:
    __slots__ = ("key", "value", "height", "left", "right")

    def __init__(self, key: tuple[float, int], value: State) -> None:
        self.key = key
        self.value = value
        self.height = 1
        self.left: AVLNode | None = None
        self.right: AVLNode | None = None


class AVLSet:
    def __init__(self) -> None:
        self.root: AVLNode | None = None
        self._size = 0

    def __len__(self) -> int:
        return self._size

    def empty(self) -> bool:
        return self._size == 0

    @staticmethod
    def _height(node: AVLNode | None) -> int:
        return 0 if node is None else node.height

    @staticmethod
    def _update(node: AVLNode) -> None:
        node.height = 1 + max(AVLSet._height(node.left), AVLSet._height(node.right))

    @staticmethod
    def _balance_factor(node: AVLNode) -> int:
        return AVLSet._height(node.left) - AVLSet._height(node.right)

    @staticmethod
    def _rotate_right(node: AVLNode) -> AVLNode:
        left = node.left
        assert left is not None
        node.left = left.right
        left.right = node
        AVLSet._update(node)
        AVLSet._update(left)
        return left

    @staticmethod
    def _rotate_left(node: AVLNode) -> AVLNode:
        right = node.right
        assert right is not None
        node.right = right.left
        right.left = node
        AVLSet._update(node)
        AVLSet._update(right)
        return right

    def _rebalance(self, node: AVLNode) -> AVLNode:
        self._update(node)
        balance = self._balance_factor(node)
        if balance > 1:
            assert node.left is not None
            if self._balance_factor(node.left) < 0:
                node.left = self._rotate_left(node.left)
            return self._rotate_right(node)
        if balance < -1:
            assert node.right is not None
            if self._balance_factor(node.right) > 0:
                node.right = self._rotate_right(node.right)
            return self._rotate_left(node)
        return node

    def _insert(self, node: AVLNode | None, key: tuple[float, int], value: State) -> AVLNode:
        if node is None:
            self._size += 1
            return AVLNode(key, value)
        if key < node.key:
            node.left = self._insert(node.left, key, value)
        else:
            node.right = self._insert(node.right, key, value)
        return self._rebalance(node)

    def insert(self, key: tuple[float, int], value: State) -> None:
        self.root = self._insert(self.root, key, value)

    def _delete_min(self, node: AVLNode) -> tuple[AVLNode | None, AVLNode]:
        if node.left is None:
            return node.right, node
        node.left, removed = self._delete_min(node.left)
        return self._rebalance(node), removed

    def _delete_max(self, node: AVLNode) -> tuple[AVLNode | None, AVLNode]:
        if node.right is None:
            return node.left, node
        node.right, removed = self._delete_max(node.right)
        return self._rebalance(node), removed

    def pop_min(self) -> State:
        if self.root is None:
            raise IndexError("pop from empty AVLSet")
        self.root, removed = self._delete_min(self.root)
        self._size -= 1
        return removed.value

    def pop_max(self) -> State:
        if self.root is None:
            raise IndexError("pop from empty AVLSet")
        self.root, removed = self._delete_max(self.root)
        self._size -= 1
        return removed.value


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
    pq = [AVLSet() for _ in range(max_turn + 1)]
    tree: list[TreeNode] = []
    next_state_id = 0

    root = State(padding=bytearray(padding_size))
    root.turn = 0
    root.tree_index = 0
    root.state_id = next_state_id
    next_state_id += 1

    tree.append(TreeNode(parent=-1, children=[], turn=0, action_id=-1))
    pq[0].insert((root.eval(), root.state_id), root)

    for _ in range(loops):
        for t in range(max_turn):
            if pq[t].empty():
                continue

            cur = pq[t].pop_max()

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
                    state_id=next_state_id,
                    padding=bytearray(cur.padding),
                )
                next_state_id += 1

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

                pq[t + 1].insert((nxt.eval(), nxt.state_id), nxt)
                if len(pq[t + 1]) > loops:
                    pq[t + 1].pop_min()

    best: State | None = None
    if not pq[max_turn].empty():
        best = pq[max_turn].pop_max()
    else:
        for t in range(max_turn - 1, -1, -1):
            if not pq[t].empty():
                best = pq[t].pop_max()
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