#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// 简单算术函数用于观察参数寄存器和返回值寄存器的使用方式。
int g(int x) {
  return x+3;
}

// 通过一次函数调用观察 jal、ra 以及编译器可能进行的内联优化。
int f(int x) {
  return g(x);
}

// 输出固定结果，随后结合生成的 call.asm 分析实际 RISC-V 指令。
void main(void) {
  printf("%d %d\n", f(8)+1, 13);
  exit(0);
}
