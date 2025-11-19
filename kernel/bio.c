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

#define LOCKNAME_SZ 10

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
} bcache;

struct hashbucket{
  char name [LOCKNAME_SZ];
  struct spinlock lock;
  struct buf blist;
}; 

struct hashbucket bhash[NBUFHASH];

struct hashbucket* gethashbuck(uint blockno) {
  return bhash + blockno % NBUFHASH;
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for (int i=0; i<NBUFHASH; i++) {
    snprintf(bhash[i].name, LOCKNAME_SZ, "bcache_%d", i);
    initlock(&bhash[i].lock, bhash[i].name);
  }

  // Create linked list of buffers
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = 0;
    initsleeplock(&b->lock, "buffer");
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b, *blistp;
  struct hashbucket *hashb, *hashb_old;

  hashb = gethashbuck(blockno);

  acquire(&hashb->lock);
  // Is the block already cached?
  for(b = hashb->blist.next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&hashb->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&hashb->lock);

  // Not cached.
  // Locking cache to search for a victim
  acquire(&bcache.lock);

  for(b=bcache.buf; b < bcache.buf + NBUF; b++) {
    // Finding first free buffer
    if (b->refcnt) continue;
    hashb_old = gethashbuck(b->blockno);
    acquire(&hashb_old->lock);
    if (!b->refcnt) break;
    // This buffer has already been taken - keep looking
    release(&hashb_old->lock);
  }

  if (b >= bcache.buf + NBUF)
    panic("bget: no buffers");

  // Found the victim
  // Remove buffer from the hash bucket list if we need to move to a new buffer
  if (hashb_old != hashb) {
    for(blistp = &hashb_old->blist; blistp; blistp = blistp->next) {
      if (blistp->next == b) {
        blistp->next = b->next;
        break;
      }
    }
  }

  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;
  release(&bcache.lock);
  release(&hashb_old->lock);

  // If we sisn't need to move the block into another bucket - return the block
  if (hashb == hashb_old) {
    acquiresleep(&b->lock);
    return b;
  }

  // Now inserting buffer into a new bucket
  acquire(&hashb->lock);
  // Check that no one has already inserted the same block
  for(blistp = hashb->blist.next; blistp; blistp = blistp->next) {
    // If the block is already there
    if (blistp->dev == dev && blistp->blockno == blockno) {
      b->refcnt = 0; // Mark the chosen victim as available
      blistp->refcnt++;
      release(&hashb->lock);
      acquiresleep(&blistp->lock);
      return blistp;
    }
  }
  // The block hasn't been inserted yet, so adding it to the bucket
  b->next = hashb->blist.next;
  hashb->blist.next = b;
  release(&hashb->lock);
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
  b->refcnt--;

  releasesleep(&b->lock);

  //acquire(&bcache.lock);
  /* 
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  release(&bcache.lock);
  */
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


