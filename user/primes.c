#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// 每一级筛选进程从左侧管道取得本级素数，并把不能整除的数传给下一级进程。
static void
sieve(int read_fd)
{
  int prime;
  if(read(read_fd, &prime, sizeof(prime)) != sizeof(prime)){
    close(read_fd);
    exit(0);
  }

  printf("prime %d\n", prime);

  int next_pipe[2];
  if(pipe(next_pipe) < 0){
    fprintf(2, "primes: pipe failed\n");
    exit(1);
  }

  int pid = fork();
  if(pid < 0){
    fprintf(2, "primes: fork failed\n");
    exit(1);
  }

  if(pid == 0){
    // 下一层只读取过滤后的数据，不再使用本层输入和新管道写端。
    close(next_pipe[1]);
    close(read_fd);
    sieve(next_pipe[0]);
  }

  close(next_pipe[0]);
  int value;
  while(read(read_fd, &value, sizeof(value)) == sizeof(value)){
    if(value % prime != 0){
      if(write(next_pipe[1], &value, sizeof(value)) != sizeof(value)){
        fprintf(2, "primes: write failed\n");
        exit(1);
      }
    }
  }

  // 关闭写端会让下一级 read 返回 0，这是管道传递结束条件的方式。
  close(read_fd);
  close(next_pipe[1]);
  wait(0);
  exit(0);
}

int
main(int argc, char *argv[])
{
  if(argc != 1){
    fprintf(2, "Usage: primes\n");
    exit(1);
  }

  // 主进程只负责生成 2～35，真正的筛选工作由后续进程链完成。
  int first_pipe[2];
  if(pipe(first_pipe) < 0){
    fprintf(2, "primes: pipe failed\n");
    exit(1);
  }

  int pid = fork();
  if(pid < 0){
    fprintf(2, "primes: fork failed\n");
    exit(1);
  }

  if(pid == 0){
    close(first_pipe[1]);
    sieve(first_pipe[0]);
  }

  close(first_pipe[0]);
  for(int value = 2; value <= 35; value++){
    if(write(first_pipe[1], &value, sizeof(value)) != sizeof(value)){
      fprintf(2, "primes: write failed\n");
      exit(1);
    }
  }
  close(first_pipe[1]);

  wait(0);
  exit(0);
}
