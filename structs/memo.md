## 概要
- 以下のものを、C++とPythonで、それぞれ実装する。
- `chokudai_search.cpp`と`chokudai_search.py`に実装する。
- Python版は、Codonで実行できることを条件とする。
- C++版とPython版は、なるべく同じような構造になるようにする。

## 作るもの
- chokudai searchの関数
- そのための、盤面を格納する構造体
  - これは3つ用意する
### 構造体の種類
- データ構造ごとに種類を分ける
- HeapStates
  - 一般的なheapのやつ
  - 上限に関わらずすべての盤面を保存する。
  - 上限`r`は使わないが、引数として受け取る
    - ほかの構造体との差し替えが用意にできるように
- AVLTreeStates
  - AVL木を使ったもの
  - 層ごとに上限`r`個までの盤面を持つ
- RBTreeStates
  - 赤黒木を使ったもの
  - 層ごとに上限`r`個までの盤面を持つ

## 構造体のメンバ関数
### 初期化
#### 引数
- 盤面の型
  - Python版では省略
- 探索の深さ
  - 層の数はそれに1を足したもの

### 追加
- 層に、新たに盤面を追加する
#### 引数
- 層番号
- 盤面

### 取り出す
- その層から、最も評価値が小さい(=最も優れている)盤面を取り出す
#### 引数
- 層番号

## chokudai searchの関数
- 関数名は`chokudai_search`
### 引数
- 初期の盤面
- chokudai幅
- 最大ループ数
### 返り値
- 最適な操作の列

### 経路復元用の木
- 配列にて表現する
- 各要素は親のindexと操作からなる
- これを辿ることで操作を復元できる
- 盤面の構造体にこれのindexを入れる

### get_path
- その盤面までの取った操作の配列を返す
  - 操作が探索の深さぶんだけ連なった配列

### 盤面の構造体
- 型の名前を`State`とする
- 毎回異なるので、実装はしない。
- メンバ変数として、経路復元用の木のindexと直前の操作を持っているものとする。

### 操作
- 型の名前を`Action`とする
- 毎回異なるので、実装はしない。

### 中身は実装せず、呼び出しのみする関数
参考コードの`TimeCheck`みたいなやつ。その行にのコメントに、実装する必要がある旨を書く。
- 盤面の大小比較の関数
  - AtCoder Libraryのsegtreeみたいに関数を引数とする。
- その盤面からの新しい盤面
  - この関数は、新たな盤面の配列を返す。
- 制限時間の確認
  - 参考コードにあるようなやつ
- その他、必要な処理の関数

---

参考: 一般的なchokudai searchの実装

```C++
State ChokudaiSearch(State FirstState)
{
  Heap<State>[] HStates = new Heap<State>[MaxTurn + 1];
  for (int i = 0; i <= MaxTurn; i++) HStates[i] = new Heap<State>();
  HStates[0].push(FirstState);
  int ChokudaiWidth = 1; //通称chokudai幅
  while (TimeCheck())
  {
    for (int t = 0; t < MaxTurn; t++)
    {
      for (int i = 0; i < ChokudaiWidth; i++)
      {
        if (HStates[t].top == null) break;
        var NowState = HStates[t].pop();
        foreach (var NextState in NowState.GetAllNextState())
        {
          HStates[t].push(NextState);
        }
      }
    }
  }
  var BestState = HStates[0].pop();
  return BestState;
}
```