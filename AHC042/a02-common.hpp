#pragma once

#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>

#include "../structs/chokudai_search.cpp"

namespace ahc042 {

constexpr int N = 20;
constexpr int MAX_OPERATIONS = 4 * N * N;
constexpr double DEFAULT_TIME_LIMIT = 1.9;
constexpr int DEFAULT_MAX_LOOP = 1000;
constexpr std::array<char, 4> DIRECTIONS{'L', 'R', 'U', 'D'};
using Board = std::array<std::array<char, N>, N>;

struct Action {
  char direction = 'L';
  int position = 0;
};

struct State {
  Board board{};
  double evaluation = 0.0;
  int oni_count = 0;
  int fuku_count = 0;
  int tree_index = -1;
  Action action;
};

// a02.py と同じ評価値。大きい値を優先し、同評価値はコンテナの生成順で比較する。
inline bool state_less(const State& lhs, const State& rhs) {
  return lhs.evaluation > rhs.evaluation;
}

inline double calc_x_eval(const Board& board, int i, int j) {
  constexpr std::array<std::pair<int, int>, 4> directions{
      std::pair<int, int>{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
  double evaluation = 0.0;
  for (const auto& [di, dj] : directions) {
    int x = i + di;
    int y = j + dj;
    int distance = 1;
    bool blocked = false;
    while (0 <= x && x < N && 0 <= y && y < N) {
      if (board[x][y] == 'o') {
        blocked = true;
        break;
      }
      // 元の評価関数では、鬼のあるマスは距離に含めない。
      if (board[x][y] == '.') ++distance;
      x += di;
      y += dj;
    }
    if (!blocked) {
      evaluation = std::max(evaluation, 1.0 / (distance * distance * distance));
    }
  }
  return evaluation;
}

inline double calc_eval(const State& state) {
  double evaluation = -1e9 * (2 * N - state.fuku_count) +
                      1e5 * (2 * N - state.oni_count);
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      if (state.board[i][j] == 'x') {
        evaluation += calc_x_eval(state.board, i, j);
      }
    }
  }
  return evaluation;
}

inline State move(const State& state, char direction, int position) {
  State next = state;
  char removed = '.';
  if (direction == 'L') {
    removed = state.board[position][0];
    for (int j = 0; j < N - 1; ++j) {
      next.board[position][j] = state.board[position][j + 1];
    }
    next.board[position][N - 1] = '.';
  } else if (direction == 'R') {
    removed = state.board[position][N - 1];
    next.board[position][0] = '.';
    for (int j = 1; j < N; ++j) {
      next.board[position][j] = state.board[position][j - 1];
    }
  } else if (direction == 'U') {
    removed = state.board[0][position];
    for (int i = 0; i < N - 1; ++i) {
      next.board[i][position] = state.board[i + 1][position];
    }
    next.board[N - 1][position] = '.';
  } else if (direction == 'D') {
    removed = state.board[N - 1][position];
    next.board[0][position] = '.';
    for (int i = 1; i < N; ++i) {
      next.board[i][position] = state.board[i - 1][position];
    }
  } else {
    throw std::invalid_argument("unknown direction");
  }
  if (removed == 'x') --next.oni_count;
  if (removed == 'o') --next.fuku_count;
  next.action = Action{direction, position};
  next.tree_index = -1;
  next.evaluation = calc_eval(next);
  return next;
}

struct SearchResult {
  std::vector<Action> actions;
  std::uint64_t final_layer_visits = 0;
};

// 両版で探索本体・評価値・生成順・時間計測を共通にする。
// 福を残して鬼をすべて除去した解が見つかるたび、より短い解だけを探索する。
template <template <class, auto> class States>
SearchResult search(const State& initial, double time_limit, int max_loop) {
  if (max_loop <= 0 || !std::isfinite(time_limit) || time_limit <= 0.0) {
    throw std::invalid_argument("time limit and maximum loops must be positive");
  }
  const auto start = std::chrono::steady_clock::now();
  States<State, state_less> states(MAX_OPERATIONS, static_cast<std::size_t>(max_loop));
  std::vector<ChokudaiSearchPathNode<Action>> path_tree;
  State root = initial;
  root.tree_index = -1;
  states.add(0, root);

  State best_partial = root;
  int best_tree_index = -1;
  int best_length = MAX_OPERATIONS + 1;
  SearchResult result;
  if (root.oni_count == 0) return result;

  bool timed_out = false;
  for (int loop = 0; loop < max_loop && !timed_out; ++loop) {
    // a02.py の range(min_len - 1) と同じ上限。
    const int depth_limit = best_length - 1;
    bool expanded = false;
    bool reached_final = false;
    bool found_solution = false;
    for (int depth = 0; depth < depth_limit; ++depth) {
      if (states.empty(depth)) continue;
      const State current = states.pop(depth);
      expanded = true;

      for (char direction : DIRECTIONS) {
        for (int position = 0; position < N; ++position) {
          State next = move(current, direction, position);
          if (path_tree.size() >=
              static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            throw std::length_error("the path tree no longer fits in tree_index");
          }
          next.tree_index = static_cast<int>(path_tree.size());
          path_tree.push_back({current.tree_index, next.action});

          // 解はキューの削減とは別に記録する。福を失った状態を解にしない。
          if (next.oni_count == 0 && next.fuku_count == root.fuku_count) {
            best_tree_index = next.tree_index;
            best_length = depth + 1;
            found_solution = true;
            reached_final = true;
            break;
          }
          if (state_less(next, best_partial)) best_partial = next;
          if (depth + 1 < MAX_OPERATIONS) states.add(depth + 1, next);
        }
        if (found_solution) break;
      }

      if (depth + 1 == depth_limit) reached_final = true;
      const double elapsed =
          std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
      timed_out = elapsed > time_limit;
      if (found_solution || timed_out) break;
    }
    // 解を発見した周回、またはその周回の上限層まで生成した周回を1回と数える。
    if (reached_final) ++result.final_layer_visits;
    if (!expanded || best_length <= 1) break;
  }

  // 完全な解がまだなければ、生成済み状態のうち評価が最良の経路を返す。
  const int result_index = best_tree_index >= 0 ? best_tree_index : best_partial.tree_index;
  result.actions = get_path(path_tree, result_index);
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

inline State read_initial_state() {
  int size;
  if (!(std::cin >> size) || size != N) {
    throw std::invalid_argument("board size must be 20");
  }
  State state;
  for (int i = 0; i < N; ++i) {
    std::string row;
    if (!(std::cin >> row) || row.size() != N) {
      throw std::invalid_argument("each board row must contain 20 cells");
    }
    for (int j = 0; j < N; ++j) {
      const char cell = row[j];
      if (cell != '.' && cell != 'o' && cell != 'x') {
        throw std::invalid_argument("unknown board cell");
      }
      state.board[i][j] = cell;
      if (cell == 'x') ++state.oni_count;
      if (cell == 'o') ++state.fuku_count;
    }
  }
  state.evaluation = calc_eval(state);
  return state;
}

template <template <class, auto> class States>
int run(int argc, char** argv) {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);
  try {
    const Options options = parse_options(argc, argv);
    const State initial = read_initial_state();
    const SearchResult result = search<States>(initial, options.time_limit, options.max_loop);
    for (const Action& action : result.actions) {
      std::cout << action.direction << ' ' << action.position << '\n';
    }
    std::cerr << "Total path length: " << result.actions.size() << '\n'
              << "Final layer visits: " << result.final_layer_visits << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    print_usage(argv[0]);
    return 1;
  }
}

}  // namespace ahc042
