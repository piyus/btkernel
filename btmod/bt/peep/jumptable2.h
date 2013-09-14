#ifndef PEEP_JUMPTABLE_H
#define PEEP_JUMPTABLE_H
#include <types.h>
#include <rbtree.h>

struct tb_t;
int  jumptable2_init(struct rbtree *rb);
void jumptable2_add(struct rbtree *rb, struct tb_t *tb);
void *jumptable2_find(struct rbtree *rb, target_ulong eip_virt);
void jumptable2_remove(struct rbtree *rb, struct tb_t *tb);

#endif
