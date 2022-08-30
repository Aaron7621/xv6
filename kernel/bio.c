// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

extern uint ticks;

struct {
    struct spinlock lock;
    struct buf buf[NBUF];

    // Linked list of all buffers, through prev/next.
    // Sorted by how recently the buffer was used.
    // head.next is most recent, head.prev is least.
//  struct buf head;
    struct buf table[NBUCKET];
    struct spinlock locks[NBUCKET];
} bcache;



void
binit(void)
{
//    printf(">>>2>>>\n");
    for (int i = 0; i < NBUCKET; ++i) {
        initlock(&bcache.locks[i], "bucketlock");
        bcache.table[0].next = 0;
    }
//    struct buf *b;
//    printf(">>>1>>>\n");
    for (int i=0;i<NBUF;i++) {
//        printf(">>>3>>>\n");
        struct buf *b = &bcache.buf[i];
        initsleeplock(&b->lock, "buffer");

//        printf("%p\n", b);
        b->refcnt = 0;
        b->ticks = 0;
        b->next = bcache.table[0].next;
        bcache.table[0].next = b;
    }
    initlock(&bcache.lock, "bcache");

}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
//    printf("blockno = %d\n",blockno);
    struct buf *b;

    uint i = blockno % NBUCKET;
//  acquire(&bcache.lock);

    // Is the block already cached? Search its own bucket first
    acquire(&bcache.locks[i]);
    for(b = bcache.table[i].next; b; b = b->next){
        if(b->dev == dev && b->blockno == blockno){
//            printf(">>>1>>>\n");
            b->refcnt++;
//      release(&bcache.lock);
            release(&bcache.locks[i]);
            acquiresleep(&b->lock);
            return b;
        }
    }
    release(&bcache.locks[i]);

    acquire(&bcache.lock);
    for(b = bcache.table[i].next; b; b = b->next){
        if(b->dev == dev && b->blockno == blockno){
//            printf(">>>1>>>\n");
            acquire(&bcache.locks[i]);
            b->refcnt++;
            release(&bcache.locks[i]);
            release(&bcache.lock);
            acquiresleep(&b->lock);
            return b;
        }
    }

//    acquire(&bcache.lock);
    // Not cached.
    // Recycle the least recently used (LRU) unused buffer.
//    uint min_ticks = 4294967295;
    struct buf *t = 0;

    int holding_bucket = -1;
    for (int j = 0; j < NBUCKET; ++j) {
        acquire(&bcache.locks[j]);
//        struct buf *x = &bcache.table[j];
//        printf("<<%p>>\n", x -> next);
        for(b = &bcache.table[j]; b->next; b = b->next){
//            printf(">>>2>>>\n");

            if(b->next->refcnt == 0 && (!t || b->next->ticks < t->next->ticks)) {
//                printf(">>>3>>>\n");
                if (holding_bucket != j)
                {
                    if (holding_bucket != -1) release(&bcache.locks[holding_bucket]);
                    holding_bucket = j;
                }
                t = b;
            }
        }
        if (j != holding_bucket) release(&bcache.locks[j]);
    }
    if (!t)
        panic("bget: no buffers");

    b = t->next;
    if (holding_bucket != i)
    {
        acquire(&bcache.locks[i]);
        t->next = b->next;
        release(&bcache.locks[holding_bucket]);
        b->next = bcache.table[i].next;
        bcache.table[i].next = b;
    }
//    acquire(&bcache.locks[i]);
//    int j = t->blockno % NBUCKET;
//    if (j != i) acquire(&bcache.locks[j]);
//    if (t->next) t->next->prev = t->prev;
//    if (t->prev) t->prev->next = t->next;
//    t->next = bcache.table[i].next;
//    t->prev = &bcache.table[i];
//    if (bcache.table[i].next) bcache.table[i].next->prev = t;
//    bcache.table[i].next = t;
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
//    if (j != i) release(&bcache.locks[j]);
//    acquire(&bcache.lock);
    release(&bcache.locks[i]);
    release(&bcache.lock);
    acquiresleep(&b->lock);
    return b;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
    struct buf *b;

    b = bget(dev, blockno);
    if(!b->valid) {
        virtio_disk_rw(b, 0);
        b->valid = 1;
    }
    return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
    if(!holdingsleep(&b->lock))
        panic("bwrite");
    virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
    if(!holdingsleep(&b->lock))
        panic("brelse");

    releasesleep(&b->lock);

    int i = b->blockno % NBUCKET;
    acquire(&bcache.locks[i]);
    b->refcnt --;
    if (!b->refcnt) b->ticks = ticks;
    release(&bcache.locks[i]);
}

void
bpin(struct buf *b) {
    int i = b->blockno % NBUCKET;
    acquire(&bcache.locks[i]);
//    acquire(&bcache.lock);
    b->refcnt++;
//    release(&bcache.lock);
    release(&bcache.locks[i]);
}

void
bunpin(struct buf *b) {
    int i = b->blockno % NBUCKET;
    acquire(&bcache.locks[i]);
//    acquire(&bcache.lock);
    b->refcnt--;
//    release(&bcache.lock);
    release(&bcache.locks[i]);

}


