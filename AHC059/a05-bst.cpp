#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// structs/chokudai_search.cpp を基にした赤黒木版。
// AHC059 固有の State と Action は探索本体の後ろに実装する。
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

// デフォルトは赤黒木版 RBTreeStates。States を HeapStates にすると、
// 探索本体を変更せずに比較できる。
// ResultLess は解の選択に使う比較関数（AHC059 では total_cost）。
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
          template <class, auto> class States = RBTreeStates,
          auto ResultLess = StateLess,
          class State>
std::vector<Action> chokudai_search(State first_state,
                                    int search_depth,
                                    int chokudai_width,
                                    int max_loop,
                                    std::uint64_t* final_layer_visits = nullptr) {
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

  // 最終層は展開しないので、全候補から ResultLess で最良解を記録する。
  // 探索中の評価値とは異なるため、上位 r 個への削減の対象にしない。
  // 途中終了した場合も、最も深い生成済み層の最良候補を返せる。
  State best_result = first_state;
  int best_depth = 0;
  if (final_layer_visits) *final_layer_visits = 0;
  bool keep_searching = true;

  for (int loop = 0; loop < max_loop && keep_searching; ++loop) {

    for (int depth = 0; depth < search_depth; ++depth) {
      for (int width = 0; width < chokudai_width; ++width) {
        if (states.empty(depth)) break;
        State current = states.pop(depth);

        const auto next_states = GetNextStates(current);
        for (State next_state : next_states) {
          if (path_tree.size() >=
              static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            throw std::length_error("the path tree no longer fits in tree_index");
          }
          const int next_tree_index = static_cast<int>(path_tree.size());
          path_tree.push_back(
              ChokudaiSearchPathNode<Action>{current.tree_index,
                                              next_state.action});
          next_state.tree_index = next_tree_index;
          if (depth + 1 > best_depth ||
              (depth + 1 == best_depth && ResultLess(next_state, best_result))) {
            best_result = next_state;
            best_depth = depth + 1;
          }
          if (depth + 1 < search_depth) {
            states.add(depth + 1, next_state);
          }
        }

        if (depth + 1 == search_depth && !next_states.empty() &&
            final_layer_visits) {
          ++*final_layer_visits;
        }

        // a05.cpp と同じく、1状態の展開が終わるたびに時間を確認する。
        if (!TimeCheck()) {
          keep_searching = false;
          break;
        }
      }
      if (!keep_searching) break;
    }
  }

  return get_path(path_tree, best_result.tree_index);
}

using namespace std;

static constexpr int N = 20;
static constexpr int SEARCH_DEPTH = N * N / 2;
static constexpr double DEFAULT_TIME_LIMIT = 1.9;
static constexpr int DEFAULT_MAX_LOOP = 1000;

struct Pos {
  int x, y;
};

static inline int manhattan(const Pos& a, const Pos& b) {
  return abs(a.x - b.x) + abs(a.y - b.y);
}

// 1回の遷移で、前半に回収するマスと後半に回収する対のマスを選ぶ。
struct Action {
  Pos current_pos{0, 0};
  Pos stack_top{-1, -1};
};

struct State {
  bitset<N * N> used;
  int prev_path_length = 0;
  int tree_index = -1;
  Action action;

  bool has_stack_top() const { return action.stack_top.x >= 0; }

  int total_cost() const {
    return prev_path_length +
           (has_stack_top() ? manhattan(action.current_pos, action.stack_top) : 0);
  }
};

static array<array<int, N>, N> grid;
static array<array<Pos, 2>, N * N> nums_idx;
static chrono::steady_clock::time_point search_start;
static double time_limit = DEFAULT_TIME_LIMIT;

// a05.cpp と同じく、探索中は前半・後半それぞれの移動距離の和で評価する。
static bool state_less(const State& a, const State& b) {
  return a.prev_path_length < b.prev_path_length;
}

// 同評価値の解は更新せず、生成が早い解を優先する。
static bool result_less(const State& a, const State& b) {
  return a.total_cost() < b.total_cost();
}

static vector<State> get_next_states(const State& state) {
  vector<State> result;
  result.reserve(N * N);
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      if (state.used.test(i * N + j)) continue;
      const Pos candidate{i, j};
      const auto& positions = nums_idx[grid[i][j]];
      const Pos pair_pos =
          positions[0].x == i && positions[0].y == j ? positions[1] : positions[0];

      State next = state;
      next.used.set(i * N + j);
      next.used.set(pair_pos.x * N + pair_pos.y);
      next.action = Action{candidate, pair_pos};
      next.prev_path_length += manhattan(state.action.current_pos, candidate);
      if (state.has_stack_top()) {
        next.prev_path_length += manhattan(state.action.stack_top, pair_pos);
      }
      next.tree_index = -1;
      result.push_back(next);
    }
  }
  return result;
}

static bool time_check() {
  const double elapsed =
      chrono::duration<double>(chrono::steady_clock::now() - search_start).count();
  return elapsed <= time_limit;
}

static vector<char> make_commands(const vector<Pos>& collect_order, Pos current_pos) {
  vector<char> commands;
  int x = current_pos.x, y = current_pos.y;
  for (const Pos& target : collect_order) {
    while (x < target.x) { commands.push_back('D'); ++x; }
    while (x > target.x) { commands.push_back('U'); --x; }
    while (y < target.y) { commands.push_back('R'); ++y; }
    while (y > target.y) { commands.push_back('L'); --y; }
    commands.push_back('Z');
  }
  return commands;
}

static int get_path_length(const vector<Pos>& path) {
  int length = 0;
  Pos current{0, 0};
  for (const Pos& next : path) {
    length += manhattan(current, next);
    current = next;
  }
  return length;
}

static void print_usage(const char* prog) {
  cerr << "Usage: " << prog << " [TIME_LIMIT_SECONDS] [-t SECONDS|--time SECONDS|--time=SECONDS] [--loops COUNT|--loops=COUNT]\n";
  cerr << "Default time limit: " << DEFAULT_TIME_LIMIT << " seconds\n";
  cerr << "Default maximum loops: " << DEFAULT_MAX_LOOP << "\n";
}

static int parse_max_loop(const string& value) {
  size_t end = 0;
  const int max_loop = stoi(value, &end);
  if (end != value.size() || max_loop <= 0) {
    throw invalid_argument("loop count must be a positive integer");
  }
  return max_loop;
}

int main(int argc, char** argv) {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  int max_loop = DEFAULT_MAX_LOOP;
  for (int argi = 1; argi < argc; ++argi) {
    const string a = argv[argi];
    try {
      if (a.rfind("--time=", 0) == 0) {
        time_limit = stod(a.substr(7));
      } else if (a == "--time" || a == "-t") {
        if (argi + 1 >= argc) {
          print_usage(argv[0]);
          return 1;
        }
        time_limit = stod(argv[++argi]);
      } else if (a.rfind("--loops=", 0) == 0) {
        max_loop = parse_max_loop(a.substr(8));
      } else if (a == "--loops") {
        if (argi + 1 >= argc) {
          print_usage(argv[0]);
          return 1;
        }
        max_loop = parse_max_loop(argv[++argi]);
      } else if (!a.empty() && a[0] != '-' && argi == 1) {
        time_limit = stod(a);
      }
    } catch (const exception&) {
      cerr << "Invalid argument: " << a << "\n";
      print_usage(argv[0]);
      return 1;
    }
  }
  if (!(time_limit > 0.0)) {
    cerr << "TIME_LIMIT_SECONDS must be > 0\n";
    print_usage(argv[0]);
    return 1;
  }

  int Nin;
  cin >> Nin;
  array<int, N * N> count{};
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      cin >> grid[i][j];
      const int num = grid[i][j];
      const int k = count[num]++;
      if (k < 2) nums_idx[num][k] = Pos{i, j};
    }
  }

  uint64_t final_layer_visits = 0;
  search_start = chrono::steady_clock::now();
  const auto actions =
      chokudai_search<Action, state_less, get_next_states, time_check,
                       RBTreeStates, result_less>(
          State{}, SEARCH_DEPTH, 1, max_loop, &final_layer_visits);

  // 前半は選択順、対のマスを回収する後半は逆順にする。
  vector<Pos> collect_order;
  collect_order.reserve(actions.size() * 2);
  for (const Action& action : actions) collect_order.push_back(action.current_pos);
  for (auto it = actions.rbegin(); it != actions.rend(); ++it) {
    collect_order.push_back(it->stack_top);
  }

  for (char command : make_commands(collect_order, Pos{0, 0})) {
    cout << command << '\n';
  }
  cerr << "Total path length: " << get_path_length(collect_order) << '\n';
  cerr << "Final layer visits: " << final_layer_visits << '\n';
  return 0;
}
