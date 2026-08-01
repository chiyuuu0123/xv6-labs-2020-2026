#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"
#include "kernel/fcntl.h"

// 返回路径最后一个分量的起始位置，用于与目标文件名比较。
static char *
base_name(char *path)
{
  char *p = path + strlen(path);
  while(p > path && *(p - 1) != '/')
    p--;
  return p;
}

// 递归遍历目录树；文件直接比较名称，目录则逐项拼接子路径后继续查找。
static void
find(char *path, char *target)
{
  char buf[512];
  char *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, O_RDONLY)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }
  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  if(st.type == T_FILE){
    if(strcmp(base_name(path), target) == 0)
      printf("%s\n", path);
    close(fd);
    return;
  }

  if(st.type != T_DIR){
    close(fd);
    return;
  }

  if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)){
    fprintf(2, "find: path too long\n");
    close(fd);
    return;
  }

  strcpy(buf, path);
  p = buf + strlen(buf);
  *p++ = '/';

  // xv6 目录文件由定长 dirent 连续组成，因此按结构体大小逐项读取。
  while(read(fd, &de, sizeof(de)) == sizeof(de)){
    if(de.inum == 0)
      continue;

    memmove(p, de.name, DIRSIZ);
    p[DIRSIZ] = 0;

    // “.”和“..”会重新指向当前或父目录，递归进入会形成无限循环。
    if(strcmp(p, ".") == 0 || strcmp(p, "..") == 0)
      continue;

    find(buf, target);
  }

  close(fd);
}

int
main(int argc, char *argv[])
{
  if(argc != 3){
    fprintf(2, "Usage: find path name\n");
    exit(1);
  }

  find(argv[1], argv[2]);
  exit(0);
}
