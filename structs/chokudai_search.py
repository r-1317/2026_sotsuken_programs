# このファイルは CPython 3.12 と Codon の両方で使える構文に限定する。
# 問題固有の State と Action は実装しない。State は次の2メンバを
# 持つ必要がある。
#
#   tree_index: int  # 経路復元用の index
#   action           # 直前の状態から取った Action
#
# state_less(a, b) は a の方が b より優れる（評価値が小さい）
# とき True を返す厳密弱順序の比較関数とする。


class _Entry[State]:
    state: State
    order: int

    def __init__(self, state: State, order: int):
        self.state = state
        self.order = order


def _entry_before(lhs, rhs, state_less):
    if state_less(lhs.state, rhs.state):
        return True
    if state_less(rhs.state, lhs.state):
        return False
    # 同評価値は生成が早い状態を優先する。
    return lhs.order < rhs.order


def _check_constructor_arguments(search_depth, r):
    if search_depth < 0:
        raise ValueError("search_depth must be non-negative")
    if r <= 0:
        raise ValueError("r must be positive")


def _check_level(level, search_depth):
    if level < 0 or level > search_depth:
        raise IndexError("level is outside the search depth")


class _HeapLevel[State, StateLess]:
    items: list[_Entry[State]]
    state_less: StateLess

    def __init__(self, state_less: StateLess):
        self.items = []
        self.state_less = state_less

    def __len__(self):
        return len(self.items)

    def empty(self):
        return len(self.items) == 0

    def add(self, entry):
        self.items.append(entry)
        index = len(self.items) - 1
        while index > 0:
            parent = (index - 1) // 2
            if not _entry_before(
                self.items[index], self.items[parent], self.state_less
            ):
                break
            self.items[index], self.items[parent] = (
                self.items[parent],
                self.items[index],
            )
            index = parent

    def pop_best(self):
        if len(self.items) == 0:
            raise IndexError("pop from an empty heap")
        best = self.items[0]
        last = self.items.pop()
        if len(self.items) > 0:
            self.items[0] = last
            index = 0
            while True:
                left = index * 2 + 1
                if left >= len(self.items):
                    break
                right = left + 1
                child = left
                if right < len(self.items) and _entry_before(
                    self.items[right], self.items[left], self.state_less
                ):
                    child = right
                if not _entry_before(
                    self.items[child], self.items[index], self.state_less
                ):
                    break
                self.items[index], self.items[child] = (
                    self.items[child],
                    self.items[index],
                )
                index = child
        return best.state


# 従来法：二分ヒープに生成した全状態を保存する。
# r は他の実装と同じ呼び出し方にするために受け取るが、
# この構造では上限として使わない。
class HeapStates[State, StateLess]:
    levels: list[_HeapLevel[State, StateLess]]
    search_depth: int
    next_order: int

    def __init__(self, search_depth: int, r: int, state_less: StateLess):
        _check_constructor_arguments(search_depth, r)
        self.search_depth = search_depth
        self.levels = [_HeapLevel(state_less) for _ in range(search_depth + 1)]
        self.next_order = 0

    def add(self, level: int, state: State):
        _check_level(level, self.search_depth)
        self.levels[level].add(_Entry(state, self.next_order))
        self.next_order += 1

    def pop(self, level):
        _check_level(level, self.search_depth)
        return self.levels[level].pop_best()

    def empty(self, level):
        _check_level(level, self.search_depth)
        return self.levels[level].empty()

    def size(self, level):
        _check_level(level, self.search_depth)
        return len(self.levels[level])


class _AVLNode[State]:
    entry: _Entry[State]
    left: int
    right: int
    height: int

    def __init__(self, entry: _Entry[State]):
        self.entry = entry
        self.left = -1
        self.right = -1
        self.height = 1


class _AVLLevel[State, StateLess]:
    nodes: list[_AVLNode[State]]
    free_nodes: list[int]
    state_less: StateLess
    root: int
    node_count: int

    def __init__(self, state_less: StateLess):
        self.nodes = []
        self.free_nodes = []
        self.root = -1
        self.node_count = 0
        self.state_less = state_less

    def __len__(self):
        return self.node_count

    def empty(self):
        return self.node_count == 0

    def _new_node(self, entry: _Entry[State]):
        if len(self.free_nodes) > 0:
            index = self.free_nodes.pop()
            self.nodes[index] = _AVLNode(entry)
            return index
        self.nodes.append(_AVLNode(entry))
        return len(self.nodes) - 1

    def _release_node(self, index):
        self.free_nodes.append(index)

    def _height(self, index):
        if index < 0:
            return 0
        return self.nodes[index].height

    def _update(self, index):
        node = self.nodes[index]
        node.height = 1 + max(self._height(node.left), self._height(node.right))

    def _balance_factor(self, index):
        node = self.nodes[index]
        return self._height(node.left) - self._height(node.right)

    def _rotate_right(self, index):
        node = self.nodes[index]
        new_root_index = node.left
        new_root = self.nodes[new_root_index]
        node.left = new_root.right
        new_root.right = index
        self._update(index)
        self._update(new_root_index)
        return new_root_index

    def _rotate_left(self, index):
        node = self.nodes[index]
        new_root_index = node.right
        new_root = self.nodes[new_root_index]
        node.right = new_root.left
        new_root.left = index
        self._update(index)
        self._update(new_root_index)
        return new_root_index

    def _rebalance(self, index):
        self._update(index)
        balance = self._balance_factor(index)
        if balance > 1:
            node = self.nodes[index]
            if self._balance_factor(node.left) < 0:
                node.left = self._rotate_left(node.left)
            return self._rotate_right(index)
        if balance < -1:
            node = self.nodes[index]
            if self._balance_factor(node.right) > 0:
                node.right = self._rotate_right(node.right)
            return self._rotate_left(index)
        return index

    def _insert(self, index, entry):
        if index < 0:
            return self._new_node(entry)
        node = self.nodes[index]
        if _entry_before(entry, node.entry, self.state_less):
            node.left = self._insert(node.left, entry)
        else:
            node.right = self._insert(node.right, entry)
        return self._rebalance(index)

    def add(self, entry: _Entry[State]):
        self.root = self._insert(self.root, entry)
        self.node_count += 1

    def _delete_min(self, index):
        node = self.nodes[index]
        if node.left < 0:
            new_root = node.right
            self._release_node(index)
            return new_root
        node.left = self._delete_min(node.left)
        return self._rebalance(index)

    def _delete_max(self, index):
        node = self.nodes[index]
        if node.right < 0:
            new_root = node.left
            self._release_node(index)
            return new_root
        node.right = self._delete_max(node.right)
        return self._rebalance(index)

    def pop_best(self):
        if self.root < 0:
            raise IndexError("pop from an empty AVL tree")
        best_index = self.root
        while self.nodes[best_index].left >= 0:
            best_index = self.nodes[best_index].left
        result = self.nodes[best_index].entry.state
        self.root = self._delete_min(self.root)
        self.node_count -= 1
        return result

    def remove_worst(self):
        if self.root < 0:
            return
        self.root = self._delete_max(self.root)
        self.node_count -= 1


# 提案法：各層を AVL 木で管理し、最良 r 個だけを保持する。
class AVLTreeStates[State, StateLess]:
    levels: list[_AVLLevel[State, StateLess]]
    search_depth: int
    r: int
    next_order: int

    def __init__(self, search_depth: int, r: int, state_less: StateLess):
        _check_constructor_arguments(search_depth, r)
        self.search_depth = search_depth
        self.r = r
        self.levels = [_AVLLevel(state_less) for _ in range(search_depth + 1)]
        self.next_order = 0

    def add(self, level: int, state: State):
        _check_level(level, self.search_depth)
        tree = self.levels[level]
        tree.add(_Entry(state, self.next_order))
        self.next_order += 1
        if len(tree) > self.r:
            tree.remove_worst()

    def pop(self, level):
        _check_level(level, self.search_depth)
        return self.levels[level].pop_best()

    def empty(self, level):
        _check_level(level, self.search_depth)
        return self.levels[level].empty()

    def size(self, level):
        _check_level(level, self.search_depth)
        return len(self.levels[level])


RED = True
BLACK = False


class _RBNode[State]:
    entry: _Entry[State]
    left: int
    right: int
    red: bool

    def __init__(self, entry: _Entry[State], red: bool):
        self.entry = entry
        self.left = -1
        self.right = -1
        self.red = red


# left-leaning red-black tree。この探索で必要な挿入、最良の取得、
# 最悪の削除だけを実装する。
class _RBLevel[State, StateLess]:
    nodes: list[_RBNode[State]]
    free_nodes: list[int]
    state_less: StateLess
    root: int
    node_count: int

    def __init__(self, state_less: StateLess):
        self.nodes = []
        self.free_nodes = []
        self.root = -1
        self.node_count = 0
        self.state_less = state_less

    def __len__(self):
        return self.node_count

    def empty(self):
        return self.node_count == 0

    def _new_node(self, entry: _Entry[State]):
        if len(self.free_nodes) > 0:
            index = self.free_nodes.pop()
            self.nodes[index] = _RBNode(entry, RED)
            return index
        self.nodes.append(_RBNode(entry, RED))
        return len(self.nodes) - 1

    def _release_node(self, index):
        self.free_nodes.append(index)

    def _is_red(self, index):
        return index >= 0 and self.nodes[index].red == RED

    def _rotate_left(self, index):
        node = self.nodes[index]
        new_root_index = node.right
        new_root = self.nodes[new_root_index]
        node.right = new_root.left
        new_root.left = index
        new_root.red = node.red
        node.red = RED
        return new_root_index

    def _rotate_right(self, index):
        node = self.nodes[index]
        new_root_index = node.left
        new_root = self.nodes[new_root_index]
        node.left = new_root.right
        new_root.right = index
        new_root.red = node.red
        node.red = RED
        return new_root_index

    def _flip_colors(self, index):
        node = self.nodes[index]
        node.red = not node.red
        if node.left >= 0:
            self.nodes[node.left].red = not self.nodes[node.left].red
        if node.right >= 0:
            self.nodes[node.right].red = not self.nodes[node.right].red

    def _insert(self, index, entry):
        if index < 0:
            return self._new_node(entry)

        node = self.nodes[index]
        if _entry_before(entry, node.entry, self.state_less):
            node.left = self._insert(node.left, entry)
        else:
            node.right = self._insert(node.right, entry)

        if self._is_red(node.right) and not self._is_red(node.left):
            index = self._rotate_left(index)
            node = self.nodes[index]
        if self._is_red(node.left) and self._is_red(self.nodes[node.left].left):
            index = self._rotate_right(index)
            node = self.nodes[index]
        if self._is_red(node.left) and self._is_red(node.right):
            self._flip_colors(index)
        return index

    def add(self, entry: _Entry[State]):
        self.root = self._insert(self.root, entry)
        self.nodes[self.root].red = BLACK
        self.node_count += 1

    def _move_red_left(self, index):
        self._flip_colors(index)
        node = self.nodes[index]
        if node.right >= 0 and self._is_red(self.nodes[node.right].left):
            node.right = self._rotate_right(node.right)
            index = self._rotate_left(index)
            self._flip_colors(index)
        return index

    def _move_red_right(self, index):
        self._flip_colors(index)
        node = self.nodes[index]
        if node.left >= 0 and self._is_red(self.nodes[node.left].left):
            index = self._rotate_right(index)
            self._flip_colors(index)
        return index

    def _fix_up(self, index):
        node = self.nodes[index]
        if self._is_red(node.right):
            index = self._rotate_left(index)
            node = self.nodes[index]
        if self._is_red(node.left) and self._is_red(self.nodes[node.left].left):
            index = self._rotate_right(index)
            node = self.nodes[index]
        if self._is_red(node.left) and self._is_red(node.right):
            self._flip_colors(index)
        return index

    def _delete_min(self, index):
        node = self.nodes[index]
        if node.left < 0:
            self._release_node(index)
            return -1
        if not self._is_red(node.left) and not self._is_red(
            self.nodes[node.left].left
        ):
            index = self._move_red_left(index)
            node = self.nodes[index]
        node.left = self._delete_min(node.left)
        return self._fix_up(index)

    def _delete_max(self, index):
        node = self.nodes[index]
        if self._is_red(node.left):
            index = self._rotate_right(index)
            node = self.nodes[index]
        if node.right < 0:
            self._release_node(index)
            return -1
        if not self._is_red(node.right) and not self._is_red(
            self.nodes[node.right].left
        ):
            index = self._move_red_right(index)
            node = self.nodes[index]
        node.right = self._delete_max(node.right)
        return self._fix_up(index)

    def pop_best(self):
        if self.root < 0:
            raise IndexError("pop from an empty red-black tree")
        best_index = self.root
        while self.nodes[best_index].left >= 0:
            best_index = self.nodes[best_index].left
        result = self.nodes[best_index].entry.state

        root_node = self.nodes[self.root]
        if not self._is_red(root_node.left) and not self._is_red(root_node.right):
            root_node.red = RED
        self.root = self._delete_min(self.root)
        if self.root >= 0:
            self.nodes[self.root].red = BLACK
        self.node_count -= 1
        return result

    def remove_worst(self):
        if self.root < 0:
            return
        root_node = self.nodes[self.root]
        if not self._is_red(root_node.left) and not self._is_red(root_node.right):
            root_node.red = RED
        self.root = self._delete_max(self.root)
        if self.root >= 0:
            self.nodes[self.root].red = BLACK
        self.node_count -= 1


# 各層を赤黒木で管理し、最良 r 個だけを保持する。
class RBTreeStates[State, StateLess]:
    levels: list[_RBLevel[State, StateLess]]
    search_depth: int
    r: int
    next_order: int

    def __init__(self, search_depth: int, r: int, state_less: StateLess):
        _check_constructor_arguments(search_depth, r)
        self.search_depth = search_depth
        self.r = r
        self.levels = [_RBLevel(state_less) for _ in range(search_depth + 1)]
        self.next_order = 0

    def add(self, level: int, state: State):
        _check_level(level, self.search_depth)
        tree = self.levels[level]
        tree.add(_Entry(state, self.next_order))
        self.next_order += 1
        if len(tree) > self.r:
            tree.remove_worst()

    def pop(self, level):
        _check_level(level, self.search_depth)
        return self.levels[level].pop_best()

    def empty(self, level):
        _check_level(level, self.search_depth)
        return self.levels[level].empty()

    def size(self, level):
        _check_level(level, self.search_depth)
        return len(self.levels[level])


class ChokudaiSearchPathNode[Action]:
    parent: int
    action: Action

    def __init__(self, parent: int, action: Action):
        self.parent = parent
        self.action = action


def get_path(tree, tree_index):
    path = []
    while tree_index >= 0:
        node = tree[tree_index]
        path.append(node.action)
        tree_index = node.parent
    path.reverse()
    return path


# 問題ごとに次の3関数を実装する必要がある。
#
#   def state_less(lhs, rhs): ...
#   def get_next_states(state): ...
#   def time_check(): ...
#
# デフォルトは研究の提案法 AVLTreeStates。_chokudai_search 内の
# AVLTreeStates を HeapStates または RBTreeStates に置き換えるだけで比較できる。
def _chokudai_search(
    first_state,
    search_depth,
    chokudai_width,
    max_loop,
    state_less_function,
    get_next_states_function,
    time_check_function,
):
    if chokudai_width <= 0:
        raise ValueError("chokudai_width must be positive")
    if max_loop <= 0:
        raise ValueError("max_loop must be positive")

    if search_depth < 0:
        raise ValueError("search_depth must be non-negative")

    # 1層から今後取り出し得る最大数。この個数まで保持すれば、
    # それより悪い状態は従来法でも取り出されない。
    state_limit = chokudai_width * max_loop
    states = AVLTreeStates(search_depth, state_limit, state_less_function)
    path_tree = []

    # 初期状態には直前の操作がないため、木の外側を指す。
    first_state.tree_index = -1
    states.add(0, first_state)

    for _ in range(max_loop):
        if not time_check_function():
            break

        for depth in range(search_depth):
            for _ in range(chokudai_width):
                if states.empty(depth):
                    break
                current = states.pop(depth)

                for next_state in get_next_states_function(current):
                    next_tree_index = len(path_tree)
                    path_tree.append(
                        ChokudaiSearchPathNode(
                            current.tree_index, next_state.action
                        )
                    )
                    next_state.tree_index = next_tree_index
                    states.add(depth + 1, next_state)

    # 原則として最終層を選ぶ。行き止まりのある問題で最終層が
    # 空の場合は、参考実装と同様に最も深い非空層から選ぶ。
    for depth in range(search_depth, -1, -1):
        if not states.empty(depth):
            best = states.pop(depth)
            return get_path(path_tree, best.tree_index)

    return []


def chokudai_search(first_state, search_depth, chokudai_width, max_loop):
    return _chokudai_search(
        first_state,
        search_depth,
        chokudai_width,
        max_loop,
        state_less,  # 問題ごとに実装する必要がある。
        get_next_states,  # 問題ごとに実装する必要がある。
        time_check,  # 問題ごとに実装する必要がある。
    )
