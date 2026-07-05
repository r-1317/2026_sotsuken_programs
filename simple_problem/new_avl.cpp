#include <bits/stdc++.h>
#include <time.h>
using namespace std;

struct TreeNode {
  int parent = -1;
  vector<int> children;
  int turn = 0;
  int action_id = -1;
};

struct State {
  int turn = 0;
  double f_sum = 0.0;
  double g_sum_known = 0.0;
  double g_sum_true = 0.0;
  deque<double> pending_g;
  int tree_index = -1;
  uint64_t state_id = 0;
  // padding_sizeのぶんだけ1バイトのダミーデータを入れる
  vector<char> padding;
  State(int padding_size = 0) : padding(padding_size, 0) {}  // コンストラクタでpaddingを初期化

  // chokudai search 用の評価値:
  // 「それまでの f の和 + L ターン前までに判明した g の和」
  double eval() const {
    return f_sum + g_sum_known;
  }

  // 最終的な真のスコア確認用
  double true_score() const {
    return f_sum + g_sum_true;
  }
};

struct StateCmp {
  bool operator()(const State& a, const State& b) const {
    if (a.eval() != b.eval()) return a.eval() < b.eval();
    return a.state_id < b.state_id;
  }
};

class AVLSet {
 private:
  struct Node {
    State value;
    Node* left = nullptr;
    Node* right = nullptr;
    int height = 1;

    explicit Node(const State& v) : value(v) {}
  };

  Node* root_ = nullptr;
  size_t size_ = 0;

  static int height(Node* node) {
    return node ? node->height : 0;
  }

  static int compare(const State& a, const State& b) {
    StateCmp cmp;
    if (cmp(a, b)) return -1;
    if (cmp(b, a)) return 1;
    return 0;
  }

  static void update(Node* node) {
    node->height = 1 + std::max(height(node->left), height(node->right));
  }

  static int balance_factor(Node* node) {
    return height(node->left) - height(node->right);
  }

  static Node* rotate_right(Node* node) {
    Node* new_root = node->left;
    Node* moved = new_root->right;
    new_root->right = node;
    node->left = moved;
    update(node);
    update(new_root);
    return new_root;
  }

  static Node* rotate_left(Node* node) {
    Node* new_root = node->right;
    Node* moved = new_root->left;
    new_root->left = node;
    node->right = moved;
    update(node);
    update(new_root);
    return new_root;
  }

  static Node* rebalance(Node* node) {
    update(node);
    int bf = balance_factor(node);
    if (bf > 1) {
      if (balance_factor(node->left) < 0) {
        node->left = rotate_left(node->left);
      }
      return rotate_right(node);
    }
    if (bf < -1) {
      if (balance_factor(node->right) > 0) {
        node->right = rotate_right(node->right);
      }
      return rotate_left(node);
    }
    return node;
  }

  static Node* insert(Node* node, const State& value) {
    if (!node) return new Node(value);

    if (compare(value, node->value) < 0) {
      node->left = insert(node->left, value);
    } else {
      node->right = insert(node->right, value);
    }
    return rebalance(node);
  }

  static Node* erase_min(Node* node) {
    if (!node->left) {
      Node* right = node->right;
      delete node;
      return right;
    }
    node->left = erase_min(node->left);
    return rebalance(node);
  }

  static Node* erase_max(Node* node) {
    if (!node->right) {
      Node* left = node->left;
      delete node;
      return left;
    }
    node->right = erase_max(node->right);
    return rebalance(node);
  }

  static void clear(Node* node) {
    if (!node) return;
    clear(node->left);
    clear(node->right);
    delete node;
  }

  static Node* max_node(Node* node) {
    while (node->right) node = node->right;
    return node;
  }

 public:
  AVLSet() = default;
  ~AVLSet() { clear(root_); }

  AVLSet(const AVLSet&) = delete;
  AVLSet& operator=(const AVLSet&) = delete;
  AVLSet(AVLSet&&) = delete;
  AVLSet& operator=(AVLSet&&) = delete;

  bool empty() const { return root_ == nullptr; }
  size_t size() const { return size_; }

  void insert(const State& value) {
    root_ = this->insert(root_, value);
    ++size_;
  }

  void erase_smallest() {
    if (!root_) return;
    root_ = erase_min(root_);
    --size_;
  }

  void erase_largest() {
    if (!root_) return;
    root_ = erase_max(root_);
    --size_;
  }

  const State& max() const {
    return max_node(root_)->value;
  }
};

static void usage(const char* prog) {
  cerr << "Usage: " << prog
       << " [--loops N] [--branch N] [--turns N] [--delay L] [--seed N] [--padding-size N]\n";
  cerr << "Defaults: loops=1000 branch=30 turns=100 delay=10 seed=1 padding-size=0\n";
}

int main(int argc, char** argv) {
  clock_t start_time = clock();  // 開始時刻を記録

  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  int loops = 1000;       // chokudai search 外側ループ回数
  int branch = 30;        // 各状態から生成する遷移先数
  int max_turn = 100;     // 問題文では100
  int delay = 10;         // g が判明するまでのターン数 L
  uint64_t seed = 1;      // 乱数シード
  int padding_size = 0;   // State内に入れる余分なデータのサイズ（盤面の大きさを擬似的に再現）

  for (int i = 1; i < argc; i++) {
    string a = argv[i];

    auto need_value = [&](const string& name) {
      if (i + 1 >= argc) {
        cerr << "Missing value for " << name << "\n";
        usage(argv[0]);
        exit(1);
      }
    };

    if (a == "--loops") {  // ループ回数
      need_value(a);
      loops = stoi(argv[++i]);
    } else if (a == "--branch") {  // 分岐数
      need_value(a);
      branch = stoi(argv[++i]);
    } else if (a == "--turns") {  // 最大ターン数
      need_value(a);
      max_turn = stoi(argv[++i]);
    } else if (a == "--delay") {  // g の遅延ターン数
      need_value(a);
      delay = stoi(argv[++i]);
    } else if (a == "--seed") {  // 乱数シード
      need_value(a);
      seed = stoull(argv[++i]);
    } else if (a == "--padding-size") {  // State内のダミー領域サイズ
      need_value(a);
      padding_size = stoi(argv[++i]);
    } else if (a == "-h" || a == "--help") {  // ヘルプ
      usage(argv[0]);
      return 0;
    } else {
      cerr << "Unknown argument: " << a << "\n";  // 不明な引数
      usage(argv[0]);
      return 1;
    }
  }

  if (loops <= 0 || branch <= 0 || max_turn <= 0 || delay < 0 || padding_size < 0) {
    cerr << "Invalid argument.\n";
    usage(argv[0]);
    return 1;
  }

  mt19937_64 rng(seed);
  uniform_real_distribution<double> dist(0.0, 1.0);

  vector<AVLSet> pq(max_turn + 1);
  vector<TreeNode> tree;
  uint64_t next_state_id = 0;

  State root(padding_size);
  root.turn = 0;
  root.tree_index = 0;
  root.state_id = next_state_id++;

  tree.push_back(TreeNode{
    .parent = -1,
    .children = {},
    .turn = 0,
    .action_id = -1
  });

  pq[0].insert(root);

  for (int loop = 0; loop < loops; loop++) {
    for (int t = 0; t < max_turn; t++) {
      if (pq[t].empty()) continue;

      State cur = pq[t].max();
      pq[t].erase_largest();

      for (int k = 0; k < branch; k++) {
        double f = dist(rng);
        double g = dist(rng);

        State nxt = cur;

        nxt.turn = cur.turn + 1;
        nxt.f_sum += f;
        nxt.g_sum_true += g;
        nxt.pending_g.push_back(g);
        nxt.state_id = next_state_id++;

        if ((int)nxt.pending_g.size() > delay) {
          nxt.g_sum_known += nxt.pending_g.front();
          nxt.pending_g.pop_front();
        }

        int node_index = (int)tree.size();
        tree.push_back(TreeNode{
          .parent = cur.tree_index,
          .children = {},
          .turn = nxt.turn,
          .action_id = k
        });

        tree[cur.tree_index].children.push_back(node_index);
        nxt.tree_index = node_index;

        pq[t + 1].insert(nxt);
        if ((int)pq[t + 1].size() > loops) {
          pq[t + 1].erase_smallest();
        }
      }
    }
  }

  State best;
  bool has_best = false;

  // できれば最終ターンから選ぶ
  if (!pq[max_turn].empty()) {
    best = pq[max_turn].max();
    has_best = true;
  } else {
    // 念のため、最も深い非空レベルから選ぶ
    for (int t = max_turn - 1; t >= 0; t--) {
      if (!pq[t].empty()) {
        best = pq[t].max();
        has_best = true;
        break;
      }
    }
  }

  if (!has_best) {
    cerr << "No state found.\n";
    return 1;
  }

  vector<int> path_nodes;
  for (int v = best.tree_index; v != -1; v = tree[v].parent) {
    path_nodes.push_back(v);
  }
  reverse(path_nodes.begin(), path_nodes.end());

  // cout << fixed << setprecision(10);
  // cout << "evaluated_score = " << best->eval() << "\n";
  // cout << "true_score      = " << best->true_score() << "\n";
  // cout << "turns           = " << best->turn << "\n";
  // cout << "nodes           = " << tree.size() << "\n";

  // cout << "\npath:\n";
  // for (int i = 1; i < (int)path_nodes.size(); i++) {
  //   const auto& n = tree[path_nodes[i]];
  //   cout << "turn " << n.turn
  //         << ": action=" << n.action_id
  //         << "\n";
  // }

  clock_t end_time = clock();  // 終了時刻を記録
  double elapsed_time = double(end_time - start_time) / CLOCKS_PER_SEC;
  cerr << "elapsed_time = " << elapsed_time << " seconds\n";

  return 0;
}