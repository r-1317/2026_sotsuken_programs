#include "a02-common.hpp"

// 削除なしのヒープ版：1000手目までの探索を繰り返すchokudai search。
int main(int argc, char** argv) {
  return ahc063::run<HeapStates>(argc, argv);
}
