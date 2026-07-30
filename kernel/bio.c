// Buffer cache with per-bucket lookup locks and serialized eviction.

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13

struct {
  // 普通命中只需要一个桶锁；只有缓存缺失和换出才使用 evict_lock。
  struct spinlock evict_lock;
  struct spinlock bucket_lock[NBUCKET];
  struct buf bucket_head[NBUCKET];
  struct buf buf[NBUF];
  uint clock;
} bcache;

static int
hash(uint dev, uint blockno)
{
  return (dev + blockno) % NBUCKET;
}

static void
remove_from_bucket(struct buf *b)
{
  b->prev->next = b->next;
  b->next->prev = b->prev;
}

static void
insert_into_bucket(int bucket, struct buf *b)
{
  struct buf *head = &bcache.bucket_head[bucket];
  b->next = head->next;
  b->prev = head;
  head->next->prev = b;
  head->next = b;
}

void
binit(void)
{
  initlock(&bcache.evict_lock, "bcache.evict");
  bcache.clock = 0;

  for(int i = 0; i < NBUCKET; i++){
    initlock(&bcache.bucket_lock[i], "bcache.bucket");
    bcache.bucket_head[i].next = &bcache.bucket_head[i];
    bcache.bucket_head[i].prev = &bcache.bucket_head[i];
  }

  // 尚未分配身份的缓冲区统一放在 0 号桶，后续首次换入时移动到目标桶。
  for(struct buf *b = bcache.buf; b < bcache.buf + NBUF; b++){
    initsleeplock(&b->lock, "buffer");
    b->dev = 0;
    b->blockno = 0;
    b->valid = 0;
    b->refcnt = 0;
    b->timestamp = bcache.clock++;
    insert_into_bucket(0, b);
  }
}

// Return a sleep-locked buffer for (dev, blockno).
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int target = hash(dev, blockno);

 retry_lookup:
  acquire(&bcache.bucket_lock[target]);
  struct buf *head = &bcache.bucket_head[target];
  for(b = head->next; b != head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket_lock[target]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket_lock[target]);

  // 所有缺失换入串行化，并在持锁后重查，保证同一块只有一个缓存副本。
  acquire(&bcache.evict_lock);
  acquire(&bcache.bucket_lock[target]);
  for(b = head->next; b != head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket_lock[target]);
      release(&bcache.evict_lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket_lock[target]);

  for(;;){
    struct buf *victim = 0;
    uint oldest = 0;

    // 身份移动由 evict_lock 保护；逐桶读取 refcnt 和时间戳。
    for(b = bcache.buf; b < bcache.buf + NBUF; b++){
      int bucket = hash(b->dev, b->blockno);
      acquire(&bcache.bucket_lock[bucket]);
      if(b->refcnt == 0 && (victim == 0 || b->timestamp < oldest)){
        victim = b;
        oldest = b->timestamp;
      }
      release(&bcache.bucket_lock[bucket]);
    }

    if(victim == 0){
      release(&bcache.evict_lock);
      panic("bget: no buffers");
    }

    int old = hash(victim->dev, victim->blockno);
    acquire(&bcache.bucket_lock[old]);
    if(old != target)
      acquire(&bcache.bucket_lock[target]);

    // 扫描后可能有命中者取得了候选；若如此就在换出锁内重新选择。
    if(victim->refcnt != 0){
      if(old != target)
        release(&bcache.bucket_lock[target]);
      release(&bcache.bucket_lock[old]);
      continue;
    }

    remove_from_bucket(victim);
    victim->dev = dev;
    victim->blockno = blockno;
    victim->valid = 0;
    victim->refcnt = 1;
    insert_into_bucket(target, victim);

    if(old != target)
      release(&bcache.bucket_lock[target]);
    release(&bcache.bucket_lock[old]);
    release(&bcache.evict_lock);

    acquiresleep(&victim->lock);
    return victim;
  }

  // 仅用于让编译器理解控制流；正常路径不会到达。
  goto retry_lookup;
}

struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b = bget(dev, blockno);
  if(!b->valid){
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");
  releasesleep(&b->lock);

  int bucket = hash(b->dev, b->blockno);
  acquire(&bcache.bucket_lock[bucket]);
  b->refcnt--;
  if(b->refcnt == 0)
    b->timestamp = __sync_add_and_fetch(&bcache.clock, 1);
  release(&bcache.bucket_lock[bucket]);
}

void
bpin(struct buf *b)
{
  int bucket = hash(b->dev, b->blockno);
  acquire(&bcache.bucket_lock[bucket]);
  b->refcnt++;
  release(&bcache.bucket_lock[bucket]);
}

void
bunpin(struct buf *b)
{
  int bucket = hash(b->dev, b->blockno);
  acquire(&bcache.bucket_lock[bucket]);
  b->refcnt--;
  if(b->refcnt == 0)
    b->timestamp = __sync_add_and_fetch(&bcache.clock, 1);
  release(&bcache.bucket_lock[bucket]);
}
