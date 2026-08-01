#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// 用户程序入口：校验参数后调用 xv6 的 sleep 系统调用，让进程休眠指定滴答数。
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
