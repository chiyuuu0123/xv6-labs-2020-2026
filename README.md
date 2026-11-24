# xv6-labs-2020

本仓库保存 MIT 6.S081 2020 版 xv6 Labs 的课程设计实现。

- 课程资源：[MIT 6.S081 2020 xv6](https://pdos.csail.mit.edu/6.828/2020/xv6.html)
- 实验范围：Tools、Guidance、Lab Utilities 至 Lab Networking
- 官方评分合计：**985/985**
- 完整实验报告：按课程要求单独提交，报告中附有本仓库链接、相关代码、结果与分析

## 分支与评分

| Lab | 分支 | 官方得分 |
|---|---|---:|
| Utilities | `util` | 100/100 |
| System Calls | `syscall` | 35/35 |
| Page Tables | `pgtbl` | 66/66 |
| Traps | `traps` | 85/85 |
| Lazy Allocation | `lazy` | 119/119 |
| Copy-on-Write Fork for xv6 | `cow` | 110/110 |
| Multithreading | `thread` | 60/60 |
| Locks | `lock` | 70/70 |
| File System | `fs` | 100/100 |
| Mmap | `mmap` | 140/140 |
| Networking | `net` | 100/100 |

## 使用方式

```bash
git clone https://github.com/chiyuuu0123/xv6-labs-2020-2026.git
cd xv6-labs-2020-2026
git switch <分支名>
make clean
make grade
```

例如验收 Utilities：

```bash
git switch util
make clean
make grade
```

每个实验分支均可独立编译和评分。切换分支前请先确认工作区没有未提交修改。
