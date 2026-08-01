struct buf {
  int valid;   // 缓冲区是否已经包含有效磁盘数据
  int disk;    // 磁盘设备当前是否拥有该缓冲区
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  uint timestamp; // refcnt 降为零时记录最近一次释放顺序
  struct buf *prev; // 哈希桶双向链表的前驱节点
  struct buf *next;
  uchar data[BSIZE];
};
