#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <queue>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

// このファイルは問題固有の State と Action を実装しない。
// State は次の2メンバを持つ必要がある。
//
//   int tree_index;  // 経路復元用の index
//   Action action;   // 直前の状態から取った操作
//
// StateLess(a, b) は a の方が b より優れる（評価値が小さい）
// とき true を返す厳密弱順序の比較関数とする。

namespace chokudai_search_detail {

template <class State>
struct Entry {
  State state;
  std::uint64_t order;
};

// 状態の評価値が同じ場合は、生成が早い状態を優先する。
// この順序をすべてのデータ構造で共通にすることで、
// 探索順序の差が実験結果に混ざらないようにする。
template <class State, auto StateLess>
struct EntryBefore {
  bool operator()(const Entry<State>& lhs, const Entry<State>& rhs) const {
    if (StateLess(lhs.state, rhs.state)) return true;
    if (StateLess(rhs.state, lhs.state)) return false;
    return lhs.order < rhs.order;
  }
};

template <class State, auto StateLess>
struct HeapPriority {
  bool operator()(const Entry<State>& lhs, const Entry<State>& rhs) const {
    // priority_queue では「後ろに並ぶ」要素に true を返す。
    return EntryBefore<State, StateLess>{}(rhs, lhs);
  }
};

inline void check_constructor_arguments(int search_depth, std::size_t r) {
  if (search_depth < 0) {
    throw std::invalid_argument("search_depth must be non-negative");
  }
  if (r == 0) {
    throw std::invalid_argument("r must be positive");
  }
}

inline std::size_t level_count(int search_depth, std::size_t r) {
  check_constructor_arguments(search_depth, r);
  return static_cast<std::size_t>(search_depth) + 1;
}

inline void check_level(int level, int search_depth) {
  if (level < 0 || level > search_depth) {
    throw std::out_of_range("level is outside the search depth");
  }
}

}  // namespace chokudai_search_detail

// 従来法：二分ヒープに生成した全状態を保存する。
// r は他の実装と同じ呼び出し方にするために受け取るが、
// この構造では上限として使わない。
template <class State, auto StateLess>
class HeapStates {
 private:
  using Entry = chokudai_search_detail::Entry<State>;
  using Priority = chokudai_search_detail::HeapPriority<State, StateLess>;
  using Heap = std::priority_queue<Entry, std::vector<Entry>, Priority>;

  int search_depth_;
  std::vector<Heap> levels_;
  std::uint64_t next_order_ = 0;

 public:
  HeapStates(int search_depth, std::size_t r)
      : search_depth_(search_depth),
        levels_(chokudai_search_detail::level_count(search_depth, r)) {}

  void add(int level, const State& state) {
    chokudai_search_detail::check_level(level, search_depth_);
    levels_[static_cast<std::size_t>(level)].push(Entry{state, next_order_++});
  }

  State pop(int level) {
    chokudai_search_detail::check_level(level, search_depth_);
    Heap& heap = levels_[static_cast<std::size_t>(level)];
    if (heap.empty()) throw std::out_of_range("pop from an empty level");
    State result = heap.top().state;
    heap.pop();
    return result;
  }

  bool empty(int level) const {
    chokudai_search_detail::check_level(level, search_depth_);
    return levels_[static_cast<std::size_t>(level)].empty();
  }

  std::size_t size(int level) const {
    chokudai_search_detail::check_level(level, search_depth_);
    return levels_[static_cast<std::size_t>(level)].size();
  }
};

namespace chokudai_search_detail {

template <class State, auto StateLess>
class AVLTree {
 private:
  using EntryType = Entry<State>;

  struct Node {
    explicit Node(const EntryType& entry) : entry(entry) {}

    EntryType entry;
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;
    int height = 1;
  };

  std::unique_ptr<Node> root_;
  std::size_t size_ = 0;

  static int height(const std::unique_ptr<Node>& node) {
    return node ? node->height : 0;
  }

  static void update(Node* node) {
    node->height = 1 + std::max(height(node->left), height(node->right));
  }

  static int balance_factor(const std::unique_ptr<Node>& node) {
    return height(node->left) - height(node->right);
  }

  static std::unique_ptr<Node> rotate_right(std::unique_ptr<Node> node) {
    std::unique_ptr<Node> new_root = std::move(node->left);
    node->left = std::move(new_root->right);
    update(node.get());
    new_root->right = std::move(node);
    update(new_root.get());
    return new_root;
  }

  static std::unique_ptr<Node> rotate_left(std::unique_ptr<Node> node) {
    std::unique_ptr<Node> new_root = std::move(node->right);
    node->right = std::move(new_root->left);
    update(node.get());
    new_root->left = std::move(node);
    update(new_root.get());
    return new_root;
  }

  static std::unique_ptr<Node> rebalance(std::unique_ptr<Node> node) {
    update(node.get());
    const int balance = balance_factor(node);
    if (balance > 1) {
      if (balance_factor(node->left) < 0) {
        node->left = rotate_left(std::move(node->left));
      }
      return rotate_right(std::move(node));
    }
    if (balance < -1) {
      if (balance_factor(node->right) > 0) {
        node->right = rotate_right(std::move(node->right));
      }
      return rotate_left(std::move(node));
    }
    return node;
  }

  static std::unique_ptr<Node> insert(std::unique_ptr<Node> node,
                                      const EntryType& entry) {
    if (!node) return std::make_unique<Node>(entry);
    if (EntryBefore<State, StateLess>{}(entry, node->entry)) {
      node->left = insert(std::move(node->left), entry);
    } else {
      node->right = insert(std::move(node->right), entry);
    }
    return rebalance(std::move(node));
  }

  static std::unique_ptr<Node> erase_min(std::unique_ptr<Node> node) {
    if (!node->left) return std::move(node->right);
    node->left = erase_min(std::move(node->left));
    return rebalance(std::move(node));
  }

  static std::unique_ptr<Node> erase_max(std::unique_ptr<Node> node) {
    if (!node->right) return std::move(node->left);
    node->right = erase_max(std::move(node->right));
    return rebalance(std::move(node));
  }

  static const Node* min_node(const Node* node) {
    while (node->left) node = node->left.get();
    return node;
  }

 public:
  AVLTree() = default;
  AVLTree(const AVLTree&) = delete;
  AVLTree& operator=(const AVLTree&) = delete;
  AVLTree(AVLTree&&) noexcept = default;
  AVLTree& operator=(AVLTree&&) noexcept = default;

  bool empty() const { return root_ == nullptr; }
  std::size_t size() const { return size_; }

  void add(const EntryType& entry) {
    root_ = insert(std::move(root_), entry);
    ++size_;
  }

  State pop_best() {
    if (!root_) throw std::out_of_range("pop from an empty AVL tree");
    State result = min_node(root_.get())->entry.state;
    root_ = erase_min(std::move(root_));
    --size_;
    return result;
  }

  void remove_worst() {
    if (!root_) return;
    root_ = erase_max(std::move(root_));
    --size_;
  }
};

}  // namespace chokudai_search_detail

// 提案法：各層を AVL 木で管理し、最良 r 個だけを保持する。
template <class State, auto StateLess>
class AVLTreeStates {
 private:
  using Entry = chokudai_search_detail::Entry<State>;
  using Tree = chokudai_search_detail::AVLTree<State, StateLess>;

  int search_depth_;
  std::size_t r_;
  std::vector<Tree> levels_;
  std::uint64_t next_order_ = 0;

 public:
  AVLTreeStates(int search_depth, std::size_t r)
      : search_depth_(search_depth),
        r_(r),
        levels_(chokudai_search_detail::level_count(search_depth, r)) {}

  void add(int level, const State& state) {
    chokudai_search_detail::check_level(level, search_depth_);
    Tree& tree = levels_[static_cast<std::size_t>(level)];
    tree.add(Entry{state, next_order_++});
    if (tree.size() > r_) tree.remove_worst();
  }

  State pop(int level) {
    chokudai_search_detail::check_level(level, search_depth_);
    return levels_[static_cast<std::size_t>(level)].pop_best();
  }

  bool empty(int level) const {
    chokudai_search_detail::check_level(level, search_depth_);
    return levels_[static_cast<std::size_t>(level)].empty();
  }

  std::size_t size(int level) const {
    chokudai_search_detail::check_level(level, search_depth_);
    return levels_[static_cast<std::size_t>(level)].size();
  }
};

// std::multiset は標準ライブラリの平衡二分探索木である。
// 本研究の C++ 実験環境（libstdc++）では赤黒木で実装される。
// AVLTreeStates と同じく、各層の最良 r 個だけを保持する。
template <class State, auto StateLess>
class RBTreeStates {
 private:
  using Entry = chokudai_search_detail::Entry<State>;
  using Before = chokudai_search_detail::EntryBefore<State, StateLess>;
  using Tree = std::multiset<Entry, Before>;

  int search_depth_;
  std::size_t r_;
  std::vector<Tree> levels_;
  std::uint64_t next_order_ = 0;

 public:
  RBTreeStates(int search_depth, std::size_t r)
      : search_depth_(search_depth),
        r_(r),
        levels_(chokudai_search_detail::level_count(search_depth, r)) {}

  void add(int level, const State& state) {
    chokudai_search_detail::check_level(level, search_depth_);
    Tree& tree = levels_[static_cast<std::size_t>(level)];
    tree.insert(Entry{state, next_order_++});
    if (tree.size() > r_) tree.erase(std::prev(tree.end()));
  }

  State pop(int level) {
    chokudai_search_detail::check_level(level, search_depth_);
    Tree& tree = levels_[static_cast<std::size_t>(level)];
    if (tree.empty()) throw std::out_of_range("pop from an empty level");
    auto best = tree.begin();
    State result = best->state;
    tree.erase(best);
    return result;
  }

  bool empty(int level) const {
    chokudai_search_detail::check_level(level, search_depth_);
    return levels_[static_cast<std::size_t>(level)].empty();
  }

  std::size_t size(int level) const {
    chokudai_search_detail::check_level(level, search_depth_);
    return levels_[static_cast<std::size_t>(level)].size();
  }
};

template <class Action>
struct ChokudaiSearchPathNode {
  int parent;
  Action action;
};

// tree_index が指すノードから根まで辿り、操作列を復元する。
template <class Action>
std::vector<Action> get_path(
    const std::vector<ChokudaiSearchPathNode<Action>>& tree,
    int tree_index) {
  std::vector<Action> path;
  while (tree_index >= 0) {
    const auto& node = tree[static_cast<std::size_t>(tree_index)];
    path.push_back(node.action);
    tree_index = node.parent;
  }
  std::reverse(path.begin(), path.end());
  return path;
}

// デフォルトは研究の提案法 AVLTreeStates。最後のテンプレート引数を
// HeapStates または RBTreeStates にすると、探索本体を変更せずに比較できる。
//
// 呼び出し例:
//   auto actions = chokudai_search<Action, state_less, get_next_states,
//                                  time_check>(
//       initial_state, search_depth, chokudai_width, max_loop);
//
// 問題ごとに次の関数を実装する必要がある。
//   bool state_less(const State&, const State&);
//   std::vector<State> get_next_states(const State&);
//   bool time_check();
template <class Action,
          auto StateLess,
          auto GetNextStates,
          auto TimeCheck,
          template <class, auto> class States = AVLTreeStates,
          class State>
std::vector<Action> chokudai_search(State first_state,
                                    int search_depth,
                                    int chokudai_width,
                                    int max_loop) {
  if (chokudai_width <= 0) {
    throw std::invalid_argument("chokudai_width must be positive");
  }
  if (max_loop <= 0) {
    throw std::invalid_argument("max_loop must be positive");
  }

  if (search_depth < 0) {
    throw std::invalid_argument("search_depth must be non-negative");
  }

  // 1層から今後取り出し得る最大数。この個数まで保持すれば、
  // それより悪い状態は従来法でも取り出されない。
  const std::size_t state_limit =
      static_cast<std::size_t>(chokudai_width) *
      static_cast<std::size_t>(max_loop);
  States<State, StateLess> states(search_depth, state_limit);
  std::vector<ChokudaiSearchPathNode<Action>> path_tree;

  // 初期状態には直前の操作がないため、木の外側を指す。
  first_state.tree_index = -1;
  states.add(0, first_state);

  for (int loop = 0; loop < max_loop; ++loop) {
    // 問題ごとに TimeCheck を実装する必要がある。
    if (!TimeCheck()) break;

    for (int depth = 0; depth < search_depth; ++depth) {
      for (int width = 0; width < chokudai_width; ++width) {
        if (states.empty(depth)) break;
        State current = states.pop(depth);

        // 問題ごとに GetNextStates を実装する必要がある。
        for (State next_state : GetNextStates(current)) {
          if (path_tree.size() >=
              static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            throw std::length_error("the path tree no longer fits in tree_index");
          }
          const int next_tree_index = static_cast<int>(path_tree.size());
          path_tree.push_back(
              ChokudaiSearchPathNode<Action>{current.tree_index,
                                              next_state.action});
          next_state.tree_index = next_tree_index;
          states.add(depth + 1, next_state);
        }
      }
    }
  }

  // 原則として最終層を選ぶ。行き止まりのある問題で最終層が
  // 空の場合は、参考実装と同様に最も深い非空層から選ぶ。
  for (int depth = search_depth; depth >= 0; --depth) {
    if (!states.empty(depth)) {
      const State best = states.pop(depth);
      return get_path(path_tree, best.tree_index);
    }
  }

  return {};
}
