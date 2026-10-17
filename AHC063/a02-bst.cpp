#include "a02-common.hpp"

// 赤黒木版：各層の上位 R 個だけを保持し、1000手目までの探索を繰り返す。
int main(int argc, char** argv) {
  return ahc063::run<RBTreeStates>(argc, argv);
}
