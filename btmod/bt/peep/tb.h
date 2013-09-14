#ifndef PEEP_TB_H
#define PEEP_TB_H
#include <linux/types.h>
#include <hash.h>
#include <rbtree.h>
#include <types.h>

#ifndef MAX_TU_SIZE
#define MAX_TU_SIZE 2000
#endif

struct btmod_vcpu;
struct tb_t;
struct btmod;

typedef struct tb_t {
  target_ulong eip_virt;
  target_ulong end_eip;
  size_t edge_len, code_len;
	size_t num_insns;
	size_t num_jumps;
  uint8_t *tc_ptr;
  uint8_t *edge_ptr;
  uint16_t  *eip_boundaries;
  uint16_t *tc_boundaries;
	uint16_t *jump_boundaries;
  unsigned alignment:2;
  struct rbtree_elem jumptable2_elem;
  struct rbtree_elem tc_elem;
} tb_t;

void tb_malloc(struct btmod *bt, tb_t *tb);
tb_t * tb_find(struct rbtree *rb, const void *tc_ptr);
void tb_add(struct rbtree *rb, tb_t *tb);
target_ulong tb_tc_ptr_to_eip_virt(struct rbtree *rb, uint32_t tc_ptr);
void tb_init(struct rbtree *rb);
tb_t *alloc_a_tb(struct btmod *bt);
uint32_t eip_virt_to_tc_ptr(tb_t *tb, uint32_t eip_virt);
void print_asm_info(struct btmod_vcpu *v, tb_t *tb);
void tb_add_jump(struct btmod *bt, tb_t *tb, uint16_t off);

#endif
