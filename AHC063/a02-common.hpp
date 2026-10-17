#pragma once

#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <tuple>

#include "../structs/chokudai_search.cpp"

namespace ahc063 {

constexpr int SEARCH_DEPTH = 1000;
constexpr double DEFAULT_TIME_LIMIT = 1.9;
constexpr int DEFAULT_MAX_LOOP = 1000;

struct Problem {
  int n = 0;
  int m = 0;
  int c = 0;
  std::vector<int> desired;
  std::vector<int> board;
  std::vector<std::vector<std::pair<int, char>>> neighbors;

  void build_neighbors() {
    const std::vector<std::tuple<int, int, char>> directions{
        {-1, 0, 'U'}, {1, 0, 'D'}, {0, -1, 'L'}, {0, 1, 'R'}};
    neighbors.assign(n * n, {});
    for (int r = 0; r < n; ++r) {
      for (int col = 0; col < n; ++col) {
        for (const auto& [dr, dc, direction] : directions) {
          const int nr = r + dr;
          const int nc = col + dc;
          if (0 <= nr && nr < n && 0 <= nc && nc < n) {
            neighbors[r * n + col].emplace_back(nr * n + nc, direction);
          }
        }
      }
    }
  }
};

struct State {
  std::vector<int> pos;
  std::vector<int> col;
  std::vector<int> foods;
  int matches = 5;
  int score = 0;
  int turns = 0;
  int tree_index = -1;
  char action = '\0';
};

// 元のビームサーチと同じ評価式。絶対スコアは 20000*M - score に等しい。
inline int score_of(int length, int matches, int turns) {
  return 10000 * (length + matches) - turns;
}

inline bool state_less(const State& lhs, const State& rhs) {
  return lhs.score > rhs.score;
}

inline State initial_state(const Problem& problem) {
  State state;
  state.pos = {4 * problem.n, 3 * problem.n, 2 * problem.n, problem.n, 0};
  state.col.assign(5, 1);
  state.foods = problem.board;
  state.score = score_of(5, 5, 0);
  return state;
}

// 元コードの U, D, L, R 順、Uターン禁止、食事、噛みちぎりを引き継ぐ。
inline std::vector<State> get_next_states(const Problem& problem, const State& state) {
  std::vector<State> result;
  result.reserve(3);
  const int length = static_cast<int>(state.col.size());
  const int head = state.pos[0];
  const int neck = state.pos[1];
  for (const auto& [next_head, direction] : problem.neighbors[head]) {
    if (next_head == neck) continue;

    State next;
    next.matches = state.matches;
    next.turns = state.turns + 1;
    next.action = direction;
    next.foods = state.foods;
    const int food_color = state.foods[next_head];
    if (food_color != 0) {
      // 食事と噛みちぎりは同じターンに発生しない。
      next.foods[next_head] = 0;
      next.pos.reserve(length + 1);
      next.pos.push_back(next_head);
      next.pos.insert(next.pos.end(), state.pos.begin(), state.pos.end());
      next.col = state.col;
      next.col.push_back(food_color);
      if (length < problem.m && food_color == problem.desired[length]) {
        ++next.matches;
      }
    } else {
      next.pos.reserve(length);
      next.pos.push_back(next_head);
      next.pos.insert(next.pos.end(), state.pos.begin(), state.pos.end() - 1);
      next.col = state.col;
      int bite_index = -1;
      for (int h = 1; h <= length - 2; ++h) {
        if (next.pos[h] == next_head) {
          bite_index = h;
          break;
        }
      }
      if (bite_index != -1) {
        for (int p = bite_index + 1; p < length; ++p) {
          next.foods[next.pos[p]] = state.col[p];
        }
        next.pos.resize(bite_index + 1);
        next.col.resize(bite_index + 1);
        next.matches = 0;
        const int upto = std::min(static_cast<int>(next.col.size()), problem.m);
        for (int i = 0; i < upto; ++i) {
          if (next.col[i] == problem.desired[i]) ++next.matches;
        }
      }
    }
    next.score = score_of(static_cast<int>(next.col.size()), next.matches, next.turns);
    result.push_back(std::move(next));
  }
  return result;
}

struct SearchResult {
  std::vector<char> actions;
  std::uint64_t final_layer_visits = 0;
  long long absolute_score = 0;
};

template <template <class, auto> class States>
SearchResult search(const Problem& problem, double time_limit, int max_loop) {
  if (max_loop <= 0 || !std::isfinite(time_limit) || time_limit <= 0.0) {
    throw std::invalid_argument("time limit and maximum loops must be positive");
  }
  const auto start = std::chrono::steady_clock::now();
  // 各層から今後取り出す数は最大 max_loop 個。赤黒木版ではその上位だけを残す。
  States<State, state_less> states(SEARCH_DEPTH, static_cast<std::size_t>(max_loop));
  std::vector<ChokudaiSearchPathNode<char>> path_tree;
  State best = initial_state(problem);
  states.add(0, best);
  SearchResult result;
  bool timed_out = false;

  for (int loop = 0; loop < max_loop && !timed_out; ++loop) {
    bool expanded = false;
    for (int depth = 0; depth < SEARCH_DEPTH; ++depth) {
      if (states.empty(depth)) continue;
      const State current = states.pop(depth);
      expanded = true;
      const auto next_states = get_next_states(problem, current);
      for (State next : next_states) {
        if (path_tree.size() >=
            static_cast<std::size_t>(std::numeric_limits<int>::max())) {
          throw std::length_error("the path tree no longer fits in tree_index");
        }
        next.tree_index = static_cast<int>(path_tree.size());
        path_tree.push_back({current.tree_index, next.action});
        // 元コードと同じく全手数の候補を比較し、途中で得た最良解も記録する。
        // 同評価値では更新せず、生成が早い状態を優先する。
        if (state_less(next, best)) best = next;
        // 1000手目は展開しないので、最良解の比較だけを行う。
        if (depth + 1 < SEARCH_DEPTH) states.add(depth + 1, next);
      }

      // 999手目の状態から1000手目の子を生成した展開を1回と数える。
      if (depth + 1 == SEARCH_DEPTH && !next_states.empty()) {
        ++result.final_layer_visits;
      }
      const double elapsed =
          std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
      timed_out = elapsed > time_limit;
      if (timed_out) break;
    }
    if (!expanded) break;
  }

  result.actions = get_path(path_tree, best.tree_index);
  const int length = static_cast<int>(best.col.size());
  result.absolute_score = best.turns +
      10000LL * (length - best.matches + 2LL * (problem.m - length));
  return result;
}

struct Options {
  double time_limit = DEFAULT_TIME_LIMIT;
  int max_loop = DEFAULT_MAX_LOOP;
};

inline double parse_time(const std::string& value) {
  std::size_t end = 0;
  const double parsed = std::stod(value, &end);
  if (end != value.size() || !std::isfinite(parsed) || parsed <= 0.0) {
    throw std::invalid_argument("TIME_LIMIT_SECONDS must be a finite value > 0");
  }
  return parsed;
}

inline int parse_loops(const std::string& value) {
  std::size_t end = 0;
  const int parsed = std::stoi(value, &end);
  if (end != value.size() || parsed <= 0) {
    throw std::invalid_argument("loop count must be a positive integer");
  }
  return parsed;
}

inline void print_usage(const char* program) {
  std::cerr << "Usage: " << program
            << " [TIME_LIMIT_SECONDS] [-t SECONDS|--time SECONDS|--time=SECONDS]"
            << " [--loops COUNT|--loops=COUNT]\n"
            << "Search depth: " << SEARCH_DEPTH << '\n'
            << "Default time limit: " << DEFAULT_TIME_LIMIT << " seconds\n"
            << "Default maximum loops: " << DEFAULT_MAX_LOOP << '\n';
}

inline Options parse_options(int argc, char** argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg.rfind("--time=", 0) == 0) {
      options.time_limit = parse_time(arg.substr(7));
    } else if (arg == "--time" || arg == "-t") {
      if (++i >= argc) throw std::invalid_argument("missing time limit");
      options.time_limit = parse_time(argv[i]);
    } else if (arg.rfind("--loops=", 0) == 0) {
      options.max_loop = parse_loops(arg.substr(8));
    } else if (arg == "--loops") {
      if (++i >= argc) throw std::invalid_argument("missing loop count");
      options.max_loop = parse_loops(argv[i]);
    } else if (i == 1 && !arg.empty() && arg[0] != '-') {
      options.time_limit = parse_time(arg);
    } else {
      throw std::invalid_argument("unknown argument: " + arg);
    }
  }
  return options;
}

inline Problem read_problem() {
  Problem problem;
  if (!(std::cin >> problem.n >> problem.m >> problem.c) ||
      problem.n < 8 || problem.n > 16 || problem.c < 3 || problem.c > 7 ||
      4 * problem.m < problem.n * problem.n ||
      4 * problem.m > 3 * problem.n * problem.n) {
    throw std::invalid_argument("invalid N, M, or C");
  }
  problem.desired.resize(problem.m);
  for (int i = 0; i < problem.m; ++i) {
    int& color = problem.desired[i];
    if (!(std::cin >> color) || color < 1 || color > problem.c || (i < 5 && color != 1)) {
      throw std::invalid_argument("invalid desired color sequence");
    }
  }
  problem.board.resize(problem.n * problem.n);
  int food_count = 0;
  for (int& color : problem.board) {
    if (!(std::cin >> color) || color < 0 || color > problem.c) {
      throw std::invalid_argument("invalid board cell");
    }
    if (color != 0) ++food_count;
  }
  if (food_count != problem.m - 5) throw std::invalid_argument("invalid food count");
  for (int i = 0; i < 5; ++i) {
    if (problem.board[i * problem.n] != 0) {
      throw std::invalid_argument("initial snake cells must not contain food");
    }
  }
  problem.build_neighbors();
  return problem;
}

template <template <class, auto> class States>
int run(int argc, char** argv) {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);
  try {
    const Options options = parse_options(argc, argv);
    const Problem problem = read_problem();
    const SearchResult result = search<States>(problem, options.time_limit, options.max_loop);
    for (char action : result.actions) std::cout << action << '\n';
    // 元コードのスコア出力を先頭行に残し、測定用の2項目を追加する。
    std::cerr << result.absolute_score / problem.m << '\n'
              << "Total path length: " << result.actions.size() << '\n'
              << "Final layer visits: " << result.final_layer_visits << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    print_usage(argv[0]);
    return 1;
  }
}

}  // namespace ahc063
