#include "peep/jumptable2.h"
#include <debug.h>
#include "peep/tb.h"
#include "bt_vcpu.h"
#include "hypercall.h"

static bool
tb_less(struct rbtree_elem const *_a, struct rbtree_elem const *_b, void *aux)
{
  struct tb_t *a = rbtree_entry(_a, struct tb_t, jumptable2_elem);
  struct tb_t *b = rbtree_entry(_b, struct tb_t, jumptable2_elem);
  return a->end_eip < b->eip_virt;
}

int
jumptable2_init(struct rbtree *rb)
{
  rbtree_init(rb, &tb_less, NULL);
	return 0;
}

void
jumptable2_add(struct rbtree *rb, struct tb_t *tb)
{
  //assert(!rbtree_find(&v->jumptable2, &tb->jumptable2_elem));
  rbtree_insert(rb, &tb->jumptable2_elem);
}

void *
jumptable2_find(struct rbtree *rb, target_ulong eip_virt)
{
  struct tb_t tb;
  struct rbtree_elem *found;
  tb.eip_virt = eip_virt;
  tb.end_eip = eip_virt;
  if ((found = rbtree_find(rb, &tb.jumptable2_elem))) {
    struct tb_t *ret;
    /*The entries in v->jumptable2 should always be unique. */
    /*ASSERT(   rbtree_next(found) == rbtree_end(&v->jumptable2)
         || tb_less(found, rbtree_next(found), NULL));*/
    ret = rbtree_entry(found, struct tb_t, jumptable2_elem);
    return ret;
  }
  return NULL;
}

void
jumptable2_remove(struct rbtree *rb, tb_t *tb)
{
  rbtree_delete(rb, &tb->jumptable2_elem);
}
