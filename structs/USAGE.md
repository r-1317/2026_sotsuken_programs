# 汎用chokudai search 使用説明書

この文書は、次の2ファイルに実装されている汎用chokudai searchの使い方を説明する。

- C++版：`chokudai_search.cpp`
- Python/Codon版：`chokudai_search.py`

実装には、探索本体と次の3種類の状態コンテナが含まれている。

| クラス | 内部のデータ構造 | 各層に保持する状態数 |
|---|---|---|
| `HeapStates` | 二分ヒープ | 生成された状態をすべて保持する |
| `AVLTreeStates` | AVL木 | 評価の良い方から最大`r`個 |
| `RBTreeStates` | 赤黒木 | 評価の良い方から最大`r`個 |

デフォルトの探索では、研究の提案手法である`AVLTreeStates`を使用する。

## 1. 探索関数の概要

探索関数の通常引数は、C++版とPython版で共通である。

```text
chokudai_search(
    first_state,
    search_depth,
    chokudai_width,
    max_loop,
)
```

### 引数

| 引数 | 意味 |
|---|---|
| `first_state` | 探索を開始する初期状態 |
| `search_depth` | 探索する操作回数。状態を保存する層数は`search_depth + 1` |
| `chokudai_width` | 1回の外側ループで、各層から最大何状態を展開するか |
| `max_loop` | 外側ループを繰り返す最大回数 |

例えば`search_depth = 100`の場合、状態コンテナには0層目から100層目までの101層が作られる。

```text
0層目   : 初期状態
1層目   : 1回操作した状態
2層目   : 2回操作した状態
...
100層目 : 100回操作した状態
```

`search_depth`は現在の深さではなく、問題全体で実行する最大操作回数である。

### 戻り値

最良状態に到達するまでの`Action`の配列を返す。

- C++版：`std::vector<Action>`
- Python版：`list[Action]`

通常は最終層から最良状態を選ぶ。行き止まりなどにより最終層が空の場合は、最も深い非空層から最良状態を選ぶ。この場合、戻り値の長さは`search_depth`より短くなる。

なお、ここでいう「最良」は探索で生成・保持された状態の中で最も良いという意味である。chokudai searchはヒューリスティック探索であるため、問題全体の厳密な最適解が常に保証されるわけではない。

## 2. 問題ごとに用意するもの

探索を使用する問題側では、次のものを実装する。

1. 操作を表す`Action`
2. 盤面を表す`State`
3. 状態を比較する`state_less`
4. 次状態を生成する`get_next_states`
5. 制限時間を確認する`time_check`

## 3. `Action`の要件

`Action`は、1回の操作を表す型である。整数、列挙型、座標、独自クラスなどを使用できる。

C++の例：

```cpp
enum class Action {
  MoveLeft,
  MoveRight,
  Stay,
};
```

Pythonの例：

```python
# 操作を整数で表す例
MOVE_LEFT = 0
MOVE_RIGHT = 1
STAY = 2
```

C++版では、経路木と戻り値の`std::vector`に保存できるように、`Action`がコピー可能である必要がある。

## 4. `State`の要件

`State`には、問題固有の盤面データと評価値に加えて、次の2メンバを必ず用意する。

| メンバ | 型 | 用途 |
|---|---|---|
| `tree_index` | 整数 | 経路復元用の木における現在状態のindex |
| `action` | `Action` | 親状態から現在状態へ遷移するときに使った操作 |

C++の最小例：

```cpp
using Action = int;

struct State {
  int score = 0;

  // chokudai searchが使用するメンバ
  int tree_index = -1;
  Action action = -1;
};
```

Pythonの最小例：

```python
class State:
    def __init__(self, score=0, action=-1):
        self.score = score

        # chokudai searchが使用するメンバ
        self.tree_index = -1
        self.action = action
```

初期状態には直前の操作が存在しないため、初期状態の`action`の値は探索結果には使用されない。子状態については、`get_next_states`内で必ず`action`を設定する。

C++版の状態コンテナは`State`をコピーして保存する。そのため、C++の`State`はコピー可能である必要がある。

Python版の`get_next_states`では、各遷移先に対して別々の`State`オブジェクトを生成することを推奨する。同じオブジェクトを複数の遷移先として使い回すと、後から行った変更がすでに追加した状態にも反映される可能性がある。

## 5. `state_less`

`state_less(lhs, rhs)`は、`lhs`の方が`rhs`より優れているときに`true`または`True`を返す。

この実装では「評価値が小さいほど良い」という規約を採用している。

C++：

```cpp
bool state_less(const State& lhs, const State& rhs) {
  return lhs.score < rhs.score;
}
```

Python：

```python
def state_less(lhs, rhs):
    return lhs.score < rhs.score
```

### 比較関数の条件

`state_less`は厳密弱順序を満たす必要がある。

- 同じ状態同士の比較は`false`にする
- `state_less(a, b)`と`state_less(b, a)`を同時に`true`にしない
- 比較結果が途中で変化しないようにする

評価値が同じ2状態については、両方向の`state_less`が`false`になるようにする。

```python
state_less(a, b) == False
state_less(b, a) == False
```

同評価値の状態は、ライブラリ内部の生成順によって順位を決める。3種類の状態コンテナで同じ生成順規則を使用するため、データ構造だけを変更した実験でも探索順を揃えられる。

`state_less`は探索中に何度も呼ばれる。評価値の計算が重い場合は、状態を比較するたびに再計算せず、`State`のメンバとして計算済み評価値を保存するとよい。

```python
def state_less(lhs, rhs):
    return lhs.cached_evaluation < rhs.cached_evaluation
```

Codonでは関数の型に応じてコードが特殊化されるため、`-release`ビルドで単純な比較関数を使用する場合、関数として渡すこと自体の影響は通常小さい。ただし、比較関数内の計算量はそのまま実行時間に影響する。

## 6. `get_next_states`

`get_next_states(state)`は、引数の状態から1回の操作で到達できるすべての子状態を返す。

C++：

```cpp
std::vector<State> get_next_states(const State& state) {
  std::vector<State> result;

  for (int action = 0; action < 3; ++action) {
    State next = state;
    next.action = action;
    next.score += action + 1;
    result.push_back(next);
  }

  return result;
}
```

Python：

```python
def get_next_states(state):
    result = []

    for action in range(3):
        next_state = State(
            score=state.score + action + 1,
            action=action,
        )
        result.append(next_state)

    return result
```

`tree_index`は探索関数が追加時に設定するため、`get_next_states`側で正しい値を設定する必要はない。一方、`action`は経路復元に使うため、子状態ごとに必ず設定する。

遷移できない状態では空配列を返す。

```python
def get_next_states(state):
    if state.is_dead_end:
        return []
    ...
```

## 7. `time_check`

`time_check()`は、探索を継続してよい場合に`true`または`True`を返す。

探索の外側ループの先頭で呼び出される。`false`を返すと、その時点で新しい外側ループを開始せず、残っている状態から結果を選ぶ。

C++の例：

```cpp
#include <chrono>

const auto start_time = std::chrono::steady_clock::now();

bool time_check() {
  const auto now = std::chrono::steady_clock::now();
  const auto elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
  return elapsed.count() < 1900;
}
```

Python/Codonの例：

```python
import time

start_time = time.time()

def time_check():
    return time.time() - start_time < 1.9
```

時間制限を使わず、`max_loop`だけで終了させる場合は常に`True`を返してよい。

```python
def time_check():
    return True
```

時間確認は状態展開のたびではなく、外側ループごとに行われる。そのため、`chokudai_width`や1状態からの分岐数が非常に大きい場合は、設定した時間を多少超過する可能性がある。

## 8. C++版の使用方法

### 8.1 最小構成

以下は、評価値の合計が小さい操作列を探索する完全な例である。

```cpp
#include <chrono>
#include <iostream>
#include <vector>

#include "chokudai_search.cpp"

using Action = int;

struct State {
  int score = 0;
  int tree_index = -1;
  Action action = -1;
};

bool state_less(const State& lhs, const State& rhs) {
  return lhs.score < rhs.score;
}

std::vector<State> get_next_states(const State& state) {
  std::vector<State> result;
  for (int action = 0; action < 3; ++action) {
    State next = state;
    next.score += action + 1;
    next.action = action;
    result.push_back(next);
  }
  return result;
}

const auto start_time = std::chrono::steady_clock::now();

bool time_check() {
  const auto now = std::chrono::steady_clock::now();
  return now - start_time < std::chrono::milliseconds(1900);
}

int main() {
  State initial_state;

  const int search_depth = 10;
  const int chokudai_width = 1;
  const int max_loop = 100;

  const std::vector<Action> actions =
      chokudai_search<Action, state_less, get_next_states, time_check>(
          initial_state,
          search_depth,
          chokudai_width,
          max_loop);

  for (const Action action : actions) {
    std::cout << action << '\n';
  }
}
```

`problem.cpp`と`chokudai_search.cpp`を同じディレクトリに置いた場合、次のようにコンパイルできる。

```bash
g++ -std=c++17 -O2 problem.cpp -o problem
./problem
```

`chokudai_search.cpp`はテンプレート実装を含むため、この例ではヘッダーファイルと同じように`#include`している。`problem.cpp`と`chokudai_search.cpp`を別々の翻訳単位としてコンパイルするだけでは、問題固有の`State`に対するテンプレートが実体化されない。

### 8.2 状態コンテナの切り替え

テンプレート引数を省略した場合は`AVLTreeStates`を使用する。

```cpp
chokudai_search<Action, state_less, get_next_states, time_check>(
    initial_state, search_depth, chokudai_width, max_loop);
```

従来のヒープ方式を使用する場合：

```cpp
chokudai_search<Action,
                state_less,
                get_next_states,
                time_check,
                HeapStates>(
    initial_state, search_depth, chokudai_width, max_loop);
```

赤黒木方式を使用する場合：

```cpp
chokudai_search<Action,
                state_less,
                get_next_states,
                time_check,
                RBTreeStates>(
    initial_state, search_depth, chokudai_width, max_loop);
```

C++版の`RBTreeStates`は`std::multiset`を使用する。現在の実験環境であるlibstdc++では、`std::multiset`は赤黒木を基礎として実装されている。

## 9. Python/Codon版の使用方法

Python版の公開関数は、問題固有関数を同じモジュール内から参照する。

```python
def chokudai_search(first_state, search_depth, chokudai_width, max_loop):
    ...
```

### 9.1 同じファイルで使用する場合

`chokudai_search.py`の探索実装より後ろに、問題固有の`State`、`state_less`、`get_next_states`、`time_check`とメイン処理を追加すれば、公開関数`chokudai_search`をそのまま呼び出せる。

```python
class State:
    def __init__(self, score=0, action=-1):
        self.score = score
        self.tree_index = -1
        self.action = action


def state_less(lhs, rhs):
    return lhs.score < rhs.score


def get_next_states(state):
    result = []
    for action in range(3):
        result.append(State(state.score + action + 1, action))
    return result


def time_check():
    return True


initial_state = State()
actions = chokudai_search(initial_state, 10, 1, 100)
print(actions)
```

### 9.2 別ファイルから使用する場合

問題コードを別ファイルにする場合は、内部関数`_chokudai_search`へ問題固有関数を明示的に渡す。次の`problem.py`を`chokudai_search.py`と同じディレクトリに置く。

```python
from chokudai_search import _chokudai_search


class State:
    def __init__(self, score=0, action=-1):
        self.score = score
        self.tree_index = -1
        self.action = action


def state_less(lhs, rhs):
    return lhs.score < rhs.score


def get_next_states(state):
    result = []
    for action in range(3):
        result.append(State(state.score + action + 1, action))
    return result


def time_check():
    return True


initial_state = State()
actions = _chokudai_search(
    initial_state,
    10,  # search_depth
    1,   # chokudai_width
    100, # max_loop
    state_less,
    get_next_states,
    time_check,
)
print(actions)
```

CPython 3.12での実行例：

```bash
python3 problem.py
```

Codonでの実行例：

```bash
codon run -release problem.py
```

実行ファイルを生成する場合：

```bash
codon build -release problem.py -o problem
./problem
```

このPython実装はPEP 695形式のジェネリッククラス構文を使用するため、CPythonで実行する場合はPython 3.12以降が必要である。

### 9.3 Python版の状態コンテナの切り替え

Python版の`_chokudai_search`は、次の行で`AVLTreeStates`を生成している。

```python
states = AVLTreeStates(search_depth, state_limit, state_less_function)
```

ヒープ方式で実験する場合は`HeapStates`に置き換える。

```python
states = HeapStates(search_depth, state_limit, state_less_function)
```

赤黒木方式で実験する場合は`RBTreeStates`に置き換える。

```python
states = RBTreeStates(search_depth, state_limit, state_less_function)
```

Python版の`RBTreeStates`はleft-leaning red-black treeとして実装されている。

## 10. 状態コンテナを直接使用する方法

探索関数を使わず、各状態コンテナだけを直接使うこともできる。

### C++

```cpp
AVLTreeStates<State, state_less> states(search_depth, r);

states.add(0, initial_state);

if (!states.empty(0)) {
  State best = states.pop(0);
}

std::size_t count = states.size(0);
```

### Python

```python
states = AVLTreeStates(search_depth, r, state_less)

states.add(0, initial_state)

if not states.empty(0):
    best = states.pop(0)

count = states.size(0)
```

### 共通メソッド

| メソッド | 内容 |
|---|---|
| `add(level, state)` | 指定した層へ状態を追加する |
| `pop(level)` | 指定した層から最良状態を削除して返す |
| `empty(level)` | 指定した層が空なら真を返す |
| `size(level)` | 指定した層に現在保存されている状態数を返す |

`AVLTreeStates`と`RBTreeStates`では、追加後に状態数が`r`を超えると最悪状態を削除する。`HeapStates`はAPIを統一するために`r`を受け取るが、上限としては使用しない。

層番号には`0`以上`search_depth`以下の整数を指定する。範囲外の層番号や空の層に対する`pop`は例外となる。

## 11. 探索中の状態数上限

探索関数は、AVL木または赤黒木の各層に保存する状態数の上限を次のように設定する。

```text
state_limit = chokudai_width * max_loop
```

1層から探索終了までに取り出し得る状態数は、最大で`chokudai_width * max_loop`個である。したがって、それより順位の悪い状態を削除しても、従来のヒープ方式ではその状態が取り出されない。

この性質を保つため、3種類の状態コンテナで次の条件を共通にしている。

- `state_less`による評価順
- 同評価値の場合の生成順
- 各層から最良状態を取り出す処理

## 12. 経路復元

各`State`に盤面の親状態全体を持たせるのではなく、別の配列`path_tree`に次の情報だけを保存する。

```text
親ノードのindex
親状態から取ったAction
```

各状態の`tree_index`は、この配列内の対応ノードを指す。探索終了後、`get_path`が親indexを根まで逆向きに辿り、最後に配列を反転して正しい操作順に戻す。

この方式により、状態ごとに盤面全体の経路を保持する必要がなくなる。ただし、経路木には生成された遷移ごとにノードが1個追加される。状態コンテナから削除された状態の経路ノードも、探索終了までは経路木に残る。

## 13. 引数が不正な場合

探索関数は、次の場合に例外を発生させる。

| 条件 | C++ | Python |
|---|---|---|
| `search_depth < 0` | `std::invalid_argument` | `ValueError` |
| `chokudai_width <= 0` | `std::invalid_argument` | `ValueError` |
| `max_loop <= 0` | `std::invalid_argument` | `ValueError` |

`search_depth = 0`は有効である。この場合は操作を行わないため、空の操作列を返す。

## 14. 実験時の注意点

3種類の状態コンテナを比較するときは、次の条件を揃える。

- 同じ初期状態を使用する
- 同じ`search_depth`、`chokudai_width`、`max_loop`を使用する
- 同じ`state_less`を使用する
- `get_next_states`内で乱数を使う場合は同じシードを使用する
- C++では同じ最適化オプションを使用する
- Codonでは`-release`の有無を揃える
- 実行時間制限を使う場合は、計測開始位置と制限時間を揃える

コンパイル例：

```bash
# C++
g++ -std=c++17 -O2 problem.cpp -o problem

# Codon
codon build -release problem.py -o problem
```

`State`が大きい場合、ヒープ方式は生成状態をすべて保持するため、AVL木・赤黒木方式との差が大きくなりやすい。一方、経路木のメモリ使用量は3方式に共通して残る点に注意する。

## 15. よくある問題

### 最良状態ではなく最悪状態が返ってくる

`state_less`の向きを確認する。この実装は評価値が小さい状態を優先する。

```python
# 正しい例
def state_less(lhs, rhs):
    return lhs.score < rhs.score
```

### 戻り値の長さが`search_depth`より短い

最終層に状態が存在しなかった可能性がある。主な原因は次のとおりである。

- `time_check`が最初から`False`を返した
- 途中の状態で`get_next_states`が空配列を返した
- 問題の遷移規則上、最終層まで到達できなかった

### Pythonで`state_less`が見つからない

別モジュールから公開関数`chokudai_search`だけをimportすると、問題側のグローバル関数は`chokudai_search.py`の名前空間には作られない。別ファイル構成では、9.2節のように`_chokudai_search`へ関数を明示的に渡す。

### `tree_index`に関するエラーが発生する

`State`に書き込み可能な`tree_index`メンバがあることを確認する。探索関数は、初期状態と生成された子状態の`tree_index`を上書きする。

### メモリ使用量が期待ほど減らない

次の要因を確認する。

- 経路木は生成された遷移を保持し続ける
- `State`以外の問題データを各状態が個別にコピーしている
- `chokudai_width * max_loop`が大きい
- Python/CodonとC++ではオブジェクト表現や管理情報の大きさが異なる
