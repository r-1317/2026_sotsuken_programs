#include "a02-common.hpp"

// 従来法：生成した未探索状態を二分ヒープにすべて保存する。
int main(int argc, char** argv) {
  return ahc042::run<HeapStates>(argc, argv);
}
