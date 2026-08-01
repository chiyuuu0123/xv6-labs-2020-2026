#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

#define LINE_SIZE 512

// 把一行标准输入拆分为附加参数，随后 fork 子进程执行目标命令。
static int
run_line(char *line, int length, char **args, int fixed_argc)
{
  int argc = fixed_argc;
  char *p = line;
  char *end = line + length;

  *end = 0;
  while(p < end){
    // 空格和制表符只用于分隔参数，不作为参数内容传给 exec。
    while(p < end && (*p == ' ' || *p == '\t'))
      *p++ = 0;
    if(p >= end)
      break;

    if(argc >= MAXARG - 1){
      fprintf(2, "xargs: too many arguments\n");
      return -1;
    }
    args[argc++] = p;

    while(p < end && *p != ' ' && *p != '\t')
      p++;
  }
  args[argc] = 0;

  // 空行不产生一次无意义的命令执行。
  if(argc == fixed_argc)
    return 0;

  int pid = fork();
  if(pid < 0){
    fprintf(2, "xargs: fork failed\n");
    return -1;
  }
  if(pid == 0){
    exec(args[0], args);
    fprintf(2, "xargs: exec %s failed\n", args[0]);
    exit(1);
  }

  wait(0);
  return 0;
}

int
main(int argc, char *argv[])
{
  if(argc < 2){
    fprintf(2, "Usage: xargs command [initial-arguments ...]\n");
    exit(1);
  }
  if(argc - 1 >= MAXARG){
    fprintf(2, "xargs: too many initial arguments\n");
    exit(1);
  }

  char *args[MAXARG];
  int fixed_argc = 0;
  for(int i = 1; i < argc; i++)
    args[fixed_argc++] = argv[i];

  // 逐字节读取便于同时处理换行、超长输入以及末行没有换行符的情况。
  char line[LINE_SIZE + 1];
  int length = 0;
  char ch;

  while(read(0, &ch, 1) == 1){
    if(ch == '\n'){
      if(run_line(line, length, args, fixed_argc) < 0)
        exit(1);
      length = 0;
      continue;
    }

    if(length >= LINE_SIZE){
      fprintf(2, "xargs: input line too long\n");
      exit(1);
    }
    line[length++] = ch;
  }

  // 标准输入最后一行即使没有换行符，也必须执行。
  if(length > 0 && run_line(line, length, args, fixed_argc) < 0)
    exit(1);

  exit(0);
}
