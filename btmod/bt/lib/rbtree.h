#ifndef LIB_RBTREE_H
#define LIB_RBTREE_H
#include <linux/stddef.h>
#include <linux/types.h>

struct rbtree_elem {
  struct rbtree_elem *left;
  struct rbtree_elem *right;
  struct rbtree_elem *parent;
  bool color ;
};

typedef bool rbtree_less_func(struct rbtree_elem const *a,
    struct rbtree_elem const *b, void *aux);
typedef void rbtree_print_func(struct rbtree_elem const *a, void *aux);

struct rbtree {
  struct rbtree_elem *root;
  rbtree_less_func *less;
  void *aux;
};

/* Converts pointer to rbtree element RBTREE_ELEM into a pointer to
 * the structure that RBTREE_ELEM is embedded inside.  Supply the
 * name of the outer structure STRUCT and the member name MEMBER
 * of the hash element.  See list.h for an example. */
#define rbtree_entry(RBTREE_ELEM, STRUCT, MEMBER)                           \
  ((STRUCT *) ((uint8_t *) &(RBTREE_ELEM)->parent                           \
    - offsetof (STRUCT, MEMBER.parent)))

void rbtree_init(struct rbtree *tree, rbtree_less_func *less, void *aux);
void rbtree_insert(struct rbtree *tree, struct rbtree_elem *elem);
void rbtree_delete(struct rbtree *tree, struct rbtree_elem *elem);
unsigned int rbtree_height(struct rbtree const *tree);
void rbtree_inorder(struct rbtree const *tree, rbtree_print_func *print,
    void *aux);

struct rbtree_elem *rbtree_find(struct rbtree const *tree,
    struct rbtree_elem const *elem);
struct rbtree_elem *rbtree_find_first(struct rbtree const *tree,
    struct rbtree_elem const *elem);

/* rbtree traversal. */
struct rbtree_elem *rbtree_begin (struct rbtree const *);
struct rbtree_elem *rbtree_next (struct rbtree_elem const *);
struct rbtree_elem *rbtree_end (struct rbtree const *);

struct rbtree_elem *rbtree_rbegin (struct rbtree const *);
struct rbtree_elem *rbtree_prev (struct rbtree_elem const *);
struct rbtree_elem *rbtree_rend (struct rbtree const *);


#endif /* lib/rbtree.h */
