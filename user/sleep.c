#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc != 2){
    fprintf(2, "Usage: sleep ticks\n");
    exit(1);
  }

  // Shell 传入的是字符串，系统调用需要整数形式的时钟滴答数。
  int ticks = atoi(argv[1]);
  if(sleep(ticks) < 0){
    fprintf(2, "sleep: system call failed\n");
    exit(1);
  }

  exit(0);
}
