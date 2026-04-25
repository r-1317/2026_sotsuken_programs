#include <bits/stdc++.h>
using namespace std;

static constexpr int N = 20;               // 固定
static constexpr double DEFAULT_TIME_LIMIT = 1.85;  // 秒
static constexpr int DEFAULT_MAX_WIDTH = 1000;     // chokudai_level の最大幅

struct Pos {
  int x, y;
};

// マンハッタン距離計算
static inline int manhattan(const Pos& a, const Pos& b) {
  return abs(a.x - b.x) + abs(a.y - b.y);
}

// 20x20=400 マスの使用済み管理
struct IsUsedBB {
  std::bitset<N * N> board;

  inline bool is_used(int x, int y) const {
    return board.test(x * N + y);
  }
  inline void set_used(int x, int y) {
    board.set(x * N + y);
  }
};

// num のもう片方の座標を返す
static inline Pos get_pair_pos(int num, const vector<array<Pos, 2>>& nums_idx, const Pos& other) {
  const Pos& p1 = nums_idx[num][0];
  const Pos& p2 = nums_idx[num][1];
  if (p1.x == other.x && p1.y == other.y) return p2;
  return p1;
}

struct Node {
  IsUsedBB used;
  Pos current_pos{-1, -1};

  int uid = -1;  // 重複順序を安定させるための一意ID

  bool has_stack_top = false;
  Pos stack_top{-1, -1};

  int prev_path_length = 0;
  // const Node* prev_node = nullptr;  // 元コードとは異なり、ノード間のつながりは別の木で管理
  int tree_index = -1; // ノード木でのインデックス

  // ノードの総コスト計算 Python版ではもう少し下にあったやつ
  inline int total_cost() const {
    // Python: prev_path_length + dist(current_pos, stack_top) if stack_top else prev_path_length
    return has_stack_top ? (prev_path_length + manhattan(current_pos, stack_top)) : prev_path_length;
  }

  // 次ノード生成（Python の next_nodes 相当）
  vector<Node*> next_nodes(const vector<vector<int>>& grid, const vector<array<Pos, 2>>& nums_idx, deque<Node>& pool) const {
    vector<Node*> res;
    res.reserve(N * N);

    for (int i = 0; i < N; i++) {
      for (int j = 0; j < N; j++) {
        if (used.is_used(i, j)) continue;

        Pos candidate{i, j};
        int num = grid[i][j];
        Pos pair_pos = get_pair_pos(num, nums_idx, candidate);

        pool.emplace_back();
        Node* nn = &pool.back();

        nn->used = used;  // copy
        nn->used.set_used(i, j);
        nn->used.set_used(pair_pos.x, pair_pos.y);

        nn->current_pos = candidate;
        nn->has_stack_top = true;
        nn->stack_top = pair_pos;

        int dist_1 = manhattan(current_pos, candidate);
        int dist_2 = has_stack_top ? manhattan(stack_top, pair_pos) : 0;

        nn->prev_path_length = prev_path_length + dist_1 + dist_2;
        // nn->prev_node = this;  // 元コードとは異なり、ノード間のつながりは別の木で管理
        nn->tree_index = -1; // ノード木でのインデックスは後で設定

        res.push_back(nn);
      }
    }
    return res;
  }
};

static int GLOBAL_NODE_UID = 0;

// chokudai_level 用の順序（Python版の heap の key 相当）
// prev_path_length が小さいものが「最小」
struct NodePtrCmp {
  bool operator()(const Node* a, const Node* b) const {
    if (a->prev_path_length != b->prev_path_length) return a->prev_path_length < b->prev_path_length;
    return a->uid < b->uid;
  }
};

using LevelSet = std::set<Node*, NodePtrCmp>;

static inline Node* pop_min(LevelSet& s) {
  auto it = s.begin();
  Node* v = *it;
  s.erase(it);
  return v;
}

static inline void pop_max(LevelSet& s) {
  auto it = prev(s.end());
  s.erase(it);
}

// コマンド列生成
static vector<char> make_commands(const vector<Pos>& collect_order, Pos current_pos) {
  vector<char> commands;
  int x = current_pos.x, y = current_pos.y;

  for (const auto& target : collect_order) {
    int tx = target.x, ty = target.y;

    while (x < tx) { commands.push_back('D'); x++; }
    while (x > tx) { commands.push_back('U'); x--; }
    while (y < ty) { commands.push_back('R'); y++; }
    while (y > ty) { commands.push_back('L'); y--; }

    commands.push_back('Z'); // 収集
  }
  return commands;
}

// パス長計算
static int get_path_length(const vector<Pos>& path) {
  int length = 0;
  for (size_t i = 1; i < path.size(); i++) {
    length += manhattan(path[i - 1], path[i]);
  }
  return length;
}

static void print_usage(const char* prog) {
  cerr << "Usage: " << prog
       << " [TIME_LIMIT_SECONDS] [-t SECONDS|--time SECONDS|--time=SECONDS] "
       << "[-w WIDTH|--width WIDTH|--width=WIDTH]\n";
  cerr << "Default time limit: " << DEFAULT_TIME_LIMIT << " seconds\n";
  cerr << "Default max width: " << DEFAULT_MAX_WIDTH << "\n";
}

int main(int argc, char** argv) {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  double time_limit = DEFAULT_TIME_LIMIT;
  int max_width = DEFAULT_MAX_WIDTH;
  for (int argi = 1; argi < argc; ++argi) {
    string a = argv[argi];
    try {
      if (a.rfind("--time=", 0) == 0) {
        time_limit = stod(a.substr(7));
      } else if (a.rfind("--width=", 0) == 0) {
        max_width = stoi(a.substr(8));
      } else if (a == "--time" || a == "-t") {
        if (argi + 1 >= argc) {
          print_usage(argv[0]);
          return 1;
        }
        time_limit = stod(string(argv[++argi]));
      } else if (a == "--width" || a == "-w") {
        if (argi + 1 >= argc) {
          print_usage(argv[0]);
          return 1;
        }
        max_width = stoi(string(argv[++argi]));
      } else if (!a.empty() && a[0] != '-' && argi == 1) {
        // 位置引数で TIME_LIMIT_SECONDS を受け取る（最小仕様）
        time_limit = stod(a);
      }
    } catch (const std::exception&) {
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
  if (max_width <= 0) {
    cerr << "WIDTH must be > 0\n";
    print_usage(argv[0]);
    return 1;
  }

  int Nin;
  cin >> Nin; // N=20 固定のため無視（Python と同じ）

  vector<vector<int>> grid(N, vector<int>(N));
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) cin >> grid[i][j];
  }

  // Python: nums_idx_list = [[] for _ in range(N**2)]
  // ここも 400 確保（実際に使う num は 0..199）
  vector<array<Pos, 2>> nums_idx_list(N * N);
  vector<int> cnt(N * N, 0);

  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      int num = grid[i][j];
      int k = cnt[num]++;
      if (k < 2) nums_idx_list[num][k] = Pos{i, j};
      // 3つ目以降は来ない前提
    }
  }

  // ノード実体を保持（prev_node ポインタを安定させるため deque）
  deque<Node> pool;

  pool.emplace_back();
  Node* root = &pool.back();
  root->used = IsUsedBB{};
  root->current_pos = Pos{0, 0};
  root->has_stack_top = false;
  root->prev_path_length = 0;
  root->uid = GLOBAL_NODE_UID++;
  // root->prev_node = nullptr;  // 元コードとは異なり、ノード間のつながりは別の木で管理

  const int LEVELS = (N * N) / 2 + 1; // 201
  vector<LevelSet> chokudai_levels(LEVELS);
  chokudai_levels[0].insert(root);

  // ノードをつなぐ木を構築するための可変長配列
  // 各要素は{親index, current_pos, stack_top}のタプル
  vector<tuple<int, Pos, Pos>> node_tree;
  node_tree.emplace_back(-1, root->current_pos, Pos{0, 0}); // rootノード
  root->tree_index = 0;  // rootノードのインデックスを設定

  auto start = chrono::steady_clock::now();
  bool flag = true;

  while (flag) {
    for (int i = 0; i < (N * N) / 2; i++) {
      auto& cur_level = chokudai_levels[i];
      auto& next_level = chokudai_levels[i + 1];

      if (cur_level.empty()) continue;

      // Python: node = heapq.heappop(chokudai_level)
      Node* node = pop_min(cur_level);

      // Python: next_nodes = node.next_nodes(...)
      auto next_nodes = node->next_nodes(grid, nums_idx_list, pool);

      for (Node* nn : next_nodes) {
        nn->uid = GLOBAL_NODE_UID++;
      }

      // Python: for next_node in next_nodes: heapq.heappush(next_level, next_node)
      for (Node* nn : next_nodes) {
        next_level.insert(nn);
      }

      // ノード木に新ノードを追加し、インデックスを設定
      for (Node* nn : next_nodes) {
        int parent_index = node->tree_index;
        node_tree.emplace_back(parent_index, nn->current_pos, nn->stack_top);
        nn->tree_index = static_cast<int>(node_tree.size()) - 1;
      }

      
      /* ここに最大要素の削除を追加したい */
      while (next_level.size() > static_cast<size_t>(max_width)) {
        pop_max(next_level);
      }

      // 時間制限チェック
      double elapsed = chrono::duration<double>(chrono::steady_clock::now() - start).count();
      if (elapsed > time_limit) {
        flag = false;
        break;
      }
    }
  }

  // Python は最終レベルで total_cost ソートして [0] を取るが、
  // タイムアウトで最終レベルが空の場合があるので、C++ では「最も深い非空レベル」から選ぶ
  Node* best_node = root;
  for (int lv = LEVELS - 1; lv >= 0; lv--) {
    if (chokudai_levels[lv].empty()) continue;
    int best_cost = numeric_limits<int>::max();
    for (Node* cand : chokudai_levels[lv]) {
      int c = cand->total_cost();
      if (c < best_cost) {
        best_cost = c;
        best_node = cand;
      }
    }
    break;
  }

  // vector<Pos> collect_order = best_node->reconstruct_path();

  // ノード木をたどって経路復元
  vector<Pos> collect_order;
  vector<Pos> collect_order_2;
  int index = best_node->tree_index;  // best_node のインデックスを取得
  while (index != -1) {
    const auto& [parent_index, current_pos, stack_top] = node_tree[index];
    collect_order.push_back(current_pos);  // current_pos を追加
    if (stack_top.x != -1 || stack_top.y != -1) {  // stack_top が存在する場合
      collect_order_2.push_back(stack_top);  // stack_top を追加
    }
    index = parent_index;
  }
  // collect_order と collect_order_2 を結合 ただし、collect_order_2 は逆順に追加
  if (!collect_order.empty()) {
    collect_order.pop_back();  // (0,0) を想定して除去
  }
  reverse(collect_order.begin(), collect_order.end());
  collect_order.insert(collect_order.end(), collect_order_2.begin(), collect_order_2.end());
  collect_order.pop_back(); // 最後の余分な要素を削除

  vector<char> commands = make_commands(collect_order, Pos{0, 0});

  for (char c : commands) {
    cout << c << "\n";
  }

  // デバッグ（Python は stderr に出している）
  cerr << "Total path length: " << get_path_length(collect_order) << "\n";

  return 0;
}