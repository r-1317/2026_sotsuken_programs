#include "a02-common.hpp"

// 提案法：赤黒木（libstdc++ の std::multiset）で各層の上位 R 個を保持する。
int main(int argc, char** argv) {
  return ahc042::run<RBTreeStates>(argc, argv);
}
