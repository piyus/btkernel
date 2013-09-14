#include "rbtree.h"
#include <debug.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <utils.h>
#include "hypercall.h"

#define _BLACK_ 0
#define _RED_ 1

#define _INFINITY_ 0x7fffffff


typedef struct rbtree_elem *rbnode_t ;

static void leftrotate(rbnode_t *tree, rbnode_t node) ;
static void rightrotate(rbnode_t *tree, rbnode_t node) ;
static void rb_delete_fixup(rbnode_t *Tree, rbnode_t node) ;
/*
static bool node_insert_at(struct rbtree_elem **tree, struct rbtree_elem *node,
    struct rbtree_elem *new_node, rbtree_less_func *less);
    */
static struct rbtree_elem *successor(struct rbtree_elem const *node) ;
static struct rbtree_elem *predecessor(struct rbtree_elem const *node) ;
static void node_delete(struct rbtree *tree, struct rbtree_elem *node) ;
static void node_insert(struct rbtree_elem **rootp,
    struct rbtree_elem *new_node, rbtree_less_func *less, void *aux);
//static void rbnode_copy(rbnode_t dst, rbnode_t src) ;
/*static int inorder_check(struct rbtree_elem *root, rbtree_less_func *less,
    void *aux);
static void rbtree_inorder_check(struct rbtree *tree);*/
static struct rbtree_elem const *minimum(struct rbtree_elem const *root);
static struct rbtree_elem const *maximum(struct rbtree_elem const *root);
static unsigned int height(struct rbtree_elem const *root);
static void rb_insert(rbnode_t *tree, rbnode_t node);
static void rbtree_elem_replace(struct rbtree_elem **rootp,
    struct rbtree_elem *b, struct rbtree_elem const *a);
/*
static bool search_pred_succ(rbnode_t root, ipaddr_t IP, rbnode_t *succ,
    rbnode_t *pred);
    */
struct rbtree_elem nil_store;
struct rbtree_elem *nil = &nil_store;


void
rbtree_init(struct rbtree *tree, rbtree_less_func *less, void *aux) {
  //nil = malloc(sizeof(struct rbtree_elem)); //sentinel
  //nil->low=1 ; nil->high=0 ;
  nil->color = _BLACK_;   //sentinel
  nil->left = nil->right = nil->parent = nil;
  tree->root = nil;
  tree->less = less;
  //lessf = less;
  tree->aux = aux;
}

void
rbtree_insert(struct rbtree *tree, struct rbtree_elem *elem)
{
  rbnode_t root;

  //rbtree_inorder_check(tree) ;
  root = tree->root;
#if 0
  if (node = rbtree_find(tree, elem)) {
    //rbtree_inorder_check(tree) ;
    return false;
  }
#endif

  //rbtree_inorder_check(tree) ;
  elem->left = elem->right = elem->parent = nil;
  elem->color = _RED_;

  node_insert(&tree->root, elem, tree->less, tree->aux);

  //if (node->parent!=nil && node->parent->left==node) ASSERT(node->high<node->parent->low) ;
  //if (node->parent!=nil && node->parent->right==node) ASSERT(node->low>node->parent->high) ;
  //if (node->right!=nil) ASSERT(node->high<node->right->low) ;
  //if (node->left!=nil) ASSERT(node->low>node->left->high) ;
  //rbtree_inorder_check(tree);
}

#if 0
bool
rbtree_insert(struct rbtree *tree, ipaddr_t IP)
{
  rbnode_t root, node, parent, succ, pred;

  root = tree->root;
  if (search_pred_succ(root, IP, &succ, &pred)) {
    return false;
  }

  if (pred != nil && pred->high == IP - 1) {
    if (succ != nil && succ->low == IP+1) {
      //log_printf("succ!=nil & pred!=nil.\n") ;

      ipaddr_t high = succ->high ;
      //inorder_check(*tree) ;
      node_delete(&tree->root, succ) ;
      //inorder_check(*tree);
      node = pred;
      pred->high = high;
      //if (pred->parent!=nil && pred->parent->left==pred) ASSERT(pred->high<pred->parent->low) ;
      //if (pred->right!=nil) ASSERT(pred->high<pred->right->low) ;
    } else {
      node = pred;
      pred->high = IP;
    }
  } else if (succ!=nil && succ->low==IP+1) {
    node = succ;
    succ->low=IP;
  } else {
    rbnode_t new_node;
    new_node = malloc(sizeof(struct rbtree_elem));
    ASSERT(new_node);
    new_node->low = new_node->high = IP;
    new_node->left = new_node->right = new_node->parent = nil;
    new_node->color = _RED_;

    node = new_node;
    node_insert(&tree->root, new_node);
  }
  //if (node->parent!=nil && node->parent->left==node) ASSERT(node->high<node->parent->low) ;
  //if (node->parent!=nil && node->parent->right==node) ASSERT(node->low>node->parent->high) ;
  //if (node->right!=nil) ASSERT(node->high<node->right->low) ;
  //if (node->left!=nil) ASSERT(node->low>node->left->high) ;
  //inorder_check(*tree) ;
  return true;
}

static bool
node_insert_at(struct rbtree_elem **root, struct rbtree_elem *node,
    struct rbtree_elem *new_node, rbtree_less_func *less)
{
  bool inserted;

  inserted = false;

  while (!inserted) {
    ASSERT(node!=nil);
    if (less(node, new_node)) /*(node->high < lowIP)*/ {
      if (node->right == nil) {
        node->right = new_node;
        new_node->parent = node;
        inserted = true;
      } else {
        node = node->right;
      }
    } else {
      ASSERT(is_less(new_node, node));
      if (node->left == nil) {
        node->left = new_node;
        new_node->parent = node;
        inserted = true;
      } else {
        node = node->left;
      }
    }
  }

  rb_insert(root, new_node) ;
  return true;
}
#endif

static void
node_insert(rbnode_t *rootp, rbnode_t new_node, rbtree_less_func *less,
		void *aux)
{
  rbnode_t root = *rootp;

  if (root == nil) {
    *rootp =  new_node;
  } else {
    bool inserted = false;

    while (!inserted) {
      ASSERT(root != nil);
      //ASSERT(less(root, new_node, aux) || less(new_node, root, aux));
      if (less(root, new_node, aux)) {
        if (root->right == nil) {
          root->right = new_node;
          new_node->parent = root;
          inserted = true;
        } else {
          root = root->right;
        }
      } else {
        //ASSERT(less(new_node, root, aux));
        if (root->left == nil) {
          root->left = new_node;
          new_node->parent = root;
          inserted = true;
        } else {
          root = root->left;
        }
      }
    }
  }

  rb_insert(rootp, new_node);
}

static struct rbtree_elem *
find_first_equal_entry(struct rbtree const *tree,
		struct rbtree_elem const *elem, struct rbtree_elem *root)
{
	while (1) {
		struct rbtree_elem *pred;
		pred = predecessor(root);
		if (pred == nil || tree->less(pred, elem, tree->aux)) {
			break;
		}
		ASSERT(   !tree->less(pred, elem, tree->aux)
				&& !tree->less(elem, pred, tree->aux));
		root = pred;
	}
	ASSERT(root != nil);
	ASSERT(   !tree->less(elem, root, tree->aux)
			&& !tree->less(root, elem, tree->aux));

	return root;
}

struct rbtree_elem *
rbtree_find(struct rbtree const *tree, struct rbtree_elem const *elem)
{
  struct rbtree_elem *root = tree->root;
  bool root_is_less, root_is_greater;

  while (root != nil) {
    root_is_less = tree->less(root, elem, tree->aux);
    if (root_is_less) {
      root = root->right;
    } else {
      root_is_greater = tree->less(elem, root, tree->aux);
      if (root_is_greater) {
        root = root->left;
      } else {
        return find_first_equal_entry(tree, elem, root);
      }
    }
  }
  return NULL;
}

/* Find the first element that is greater than or equal to ELEM. */
struct rbtree_elem *
rbtree_find_first(struct rbtree const *tree, struct rbtree_elem const *elem)
{
	struct rbtree_elem *root = tree->root, *ret = rbtree_end(tree);
	bool root_is_less, root_is_greater;

	while (root != nil) {
		root_is_less = tree->less(root, elem, tree->aux);
		if (root_is_less) {
			root = root->right;
		} else {
			root_is_greater = tree->less(elem, root, tree->aux);
			ret = root;
			if (root_is_greater) {
				root = root->left;
			} else {
				return find_first_equal_entry(tree, elem, root);
			}
		}
	}
	ASSERT(ret == rbtree_end(tree) || tree->less(elem, ret, tree->aux));
	return ret;
}

#if 0
bool
rbtree_delete(struct rbtree *tree, ipaddr_t IP)
{
  struct rbtree_elem *parent, *succ, *pred, *node;

  if (!rbtree_search(tree, IP, &node)) {
    return false;
  }

  if (node->low == IP) {
    if (node->high == IP) {
      node_delete(&tree->root, node) ;
    } else {
      node->low++ ;
    }
  } else if (node->high == IP) {
    node->high--;
  } else {
    rbnode_t new_node;
    new_node = malloc(sizeof(struct rbtree_elem)) ;
    ASSERT(new_node);
    ASSERT(node->low < IP && node->high > IP) ;
    new_node->low=IP+1 ; new_node->high=node->high ;
    new_node->left=new_node->right=new_node->parent=nil ;
    new_node->color = _RED_ ;
    node->high = IP - 1;
    node_insert_at(&tree->root, node, new_node);
  }
  return true;
}
#endif

void
rbtree_delete(struct rbtree *tree, struct rbtree_elem *elem)
{
#if 0
  struct rbtree_elem *parent, *succ, *pred, *node;

  //rbtree_inorder_check(tree) ;
  if (!(node = rbtree_find(tree, elem))) {
    //rbtree_inorder_check(tree) ;
    return false;
  }
#endif

  node_delete(tree, elem) ;
  //rbtree_inorder_check(tree) ;
}


static void
node_delete(struct rbtree *tree, struct rbtree_elem *node)
{
  struct rbtree_elem **rootp;
  rbnode_t root, delnode, child_delnode, parent;
  rootp = &tree->root;

  root = *rootp;

  /* Identify delnode. */
  if (node->left == nil || node->right == nil) {
    delnode = node ;
  } else {
    delnode = successor(node) ;
  }

  /* Delete delnode from the tree. */
  if (delnode->left != nil) {
    child_delnode = delnode->left ;
  } else {
    child_delnode = delnode->right ;
  }

  parent = delnode->parent ;
  child_delnode->parent = parent ;

  if (parent == nil) {
    *rootp = child_delnode ;
  } else {
    if (delnode == parent->left) {
      parent->left = child_delnode;
    } else {
      parent->right = child_delnode ;
    }
  }

  if (delnode->color == _BLACK_) {
    rb_delete_fixup(rootp, child_delnode);
  }

  /* Replace node with delnode, thus effectively deleting node. */
  if (delnode != node) {
    /*
    log_printf("\nmoving delnode[%p] to node[%p]'s position. root=%p\n",
        delnode, node, *rootp);
        */
    rbtree_elem_replace(rootp, delnode, node);
    node->parent = node->left = node->right = nil;
  }
  //rbtree_inorder_check(tree);
  //free(node);
}

static void
rb_delete_fixup(struct rbtree_elem **rootp, struct rbtree_elem *node)
{
  struct rbtree_elem *root, *parent, *sibling ;

  root = *rootp;
  parent = node->parent;
  while (node != root && node->color == _BLACK_) {
    if (node == parent->left) {
      sibling = parent->right;
      if (sibling->color == _RED_) {
        sibling->color = _BLACK_;
        parent->color = _RED_;
        leftrotate(rootp, parent);
        parent = node->parent;
        sibling = parent->right;
      }
      if (   sibling->left->color == _BLACK_
          && sibling->right->color == _BLACK_) {
        sibling->color = _RED_;
        node = parent;
        parent = node->parent;
      } else {
        if (sibling->right->color == _BLACK_) {
          sibling->left->color = _BLACK_;
          sibling->color = _RED_;
          rightrotate(rootp, sibling);
          parent = node->parent;
          sibling = parent->right;
        }
        sibling->color = parent->color;
        parent->color = _BLACK_;
        sibling->right->color = _BLACK_;
        leftrotate(rootp, parent);
        node = root;
      }
    } else { //node != parent->left
      sibling = parent->left ;
      if (sibling->color == _RED_) {
        sibling->color = _BLACK_;
        parent->color = _RED_;
        rightrotate(rootp, parent);
        parent = node->parent;
        sibling = parent->left;
      }
      if (   sibling->left->color == _BLACK_
          && sibling->right->color==_BLACK_) {
        sibling->color = _RED_;
        node = parent;
        parent = node->parent;
      } else {
        if (sibling->left->color == _BLACK_) {
          sibling->right->color = _BLACK_;
          sibling->color = _RED_;
          leftrotate(rootp, sibling);
          parent = node->parent;
          sibling = parent->left;
        }
        sibling->color = parent->color ;
        parent->color = _BLACK_ ;
        sibling->left->color = _BLACK_ ;
        rightrotate(rootp, parent);
        node = root ;
      }
    }
  }
  node->color = _BLACK_ ;
}

static void
rb_insert(rbnode_t *tree, rbnode_t node)
{
  rbnode_t root, uncle, parent;
  root = *tree;
  node->color = _RED_;
  while (node != root && node->parent->color == _RED_) {
    parent = node->parent ;
    if (parent == parent->parent->left) {
      uncle = parent->parent->right;
      if (uncle!=nil && uncle->color == _RED_) {
        parent->color = _BLACK_;
        uncle->color = _BLACK_;
        parent->parent->color = _RED_;
        node = parent->parent;
      } else {
        if (node == parent->right) {
          node = parent;
          leftrotate(tree, node);
          parent = node->parent;
        }
        parent->color = _BLACK_;
        parent->parent->color = _RED_;
        rightrotate(tree,parent->parent);
      }
    } else {
      uncle = parent->parent->left;
      if (uncle != nil && uncle->color == _RED_) {
        parent->color = _BLACK_;
        uncle->color = _BLACK_;
        parent->parent->color = _RED_;
        node = parent->parent;
      } else {
        if (node == parent->left) {
          node = parent;
          rightrotate(tree, node);
          parent = node->parent;
        }
        parent->color = _BLACK_;
        parent->parent->color = _RED_;
        leftrotate(tree,parent->parent);
      }
    }
  }
  (*tree)->color = _BLACK_;
}

static void
leftrotate(rbnode_t *tree, rbnode_t node)
{
  rbnode_t rchild, parent;

  rchild = node->right;
  parent = node->parent;
  node->right = rchild->left ;

  if (rchild->left!=nil) {
    rchild->left->parent = node ;
  }
  rchild->parent = parent ;

  if (parent == nil) {
    ASSERT(*tree == node);
    *tree = rchild ;
  } else if (node==parent->left) {
    parent->left = rchild ;
  } else {
    parent->right = rchild ;
  }
  rchild->left = node ;
  node->parent = rchild ;
}

static void
rightrotate(rbnode_t *tree, rbnode_t node)
{
  rbnode_t lchild, parent;

  lchild = node->left;
  parent = node->parent;
  node->left= lchild->right;
  if (lchild->right!=nil) {
    lchild->right->parent = node;
  }
  lchild->parent = parent;

  if (parent == nil) {
    ASSERT(*tree == node);
    *tree = lchild ;
  } else if (node == parent->left) {
    parent->left = lchild;
  }
  else {
    parent->right = lchild ;
  }
  lchild->right = node ;
  node->parent = lchild ;
}

static void
inorder(struct rbtree_elem const *root, rbtree_print_func *print, void *aux)
{
  if (root == nil) {
    return ;
  }
  inorder(root->left, print, aux);
  //dprintk("%p[%s] (left %p, right %p, parent %p) ", root,
   //   root->color?" red ":"black", root->left, root->right, root->parent);
  print(root, aux);
  //log_printf("(%d,%d) ", root->low, root->high);
  inorder(root->right, print, aux);
}

/*static int
inorder_check(struct rbtree_elem *root, rbtree_less_func *less, void *aux)
{
  int left_bh, right_bh;
  if (root == nil) {
    return 0;
  }
  left_bh = inorder_check(root->left, less, aux);

  if (   root->parent != nil
      && root->parent->left == root) {
    //ASSERT(less(root, root->parent, aux));
    ASSERT(!less(root->parent, root, aux));
  }
  if (   root->parent != nil
      && root->parent->right == root) {
    //ASSERT(less(root->parent, root, aux));
    ASSERT(!less(root, root->parent, aux));
  }
  if (root->left != nil) {
    //ASSERT(less(root->left, root, aux));
    ASSERT(!less(root, root->left, aux));
  }
  if (root->right != nil) {
    //ASSERT(less(root, root->right, aux));
    ASSERT(!less(root->right, root, aux));
  }
  right_bh = inorder_check(root->right, less, aux) ;
  if (left_bh != right_bh) {
    dprintk("%p: left_bh = %d, right_bh = %d\n", root, left_bh, right_bh);
  }
  ASSERT(left_bh == right_bh);
  return (root->color == _BLACK_)?(left_bh + 1):left_bh;
}
*/

/*static void
rbtree_inorder_check(struct rbtree *tree)
{
  inorder_check(tree->root, tree->less, tree->aux);
}*/

static unsigned int
height(struct rbtree_elem const *root)
{
  if (root == nil) {
    //ASSERT(root->left == NULL);
    //ASSERT(root->right == NULL);
    return 0 ;
  } else {
    return max(height(root->left), height(root->right)) + 1;
  }
}

static struct rbtree_elem const *
maximum(struct rbtree_elem const *root)
{
  while (root->right != nil) {
    root = root->right;
  }
  return root;
}

static struct rbtree_elem const *
minimum(struct rbtree_elem const *root)
{
  while (root->left != nil) {
    root = root->left;
  }
  return root;
}

static struct rbtree_elem *
successor(struct rbtree_elem const *node)
{
  rbnode_t parent, right;

  right = node->right;
  if (right != nil) {
    return (struct rbtree_elem *)minimum(right);
  }
  parent = node->parent;
  while (parent != nil && node == parent->right) {
    node = parent ;
    parent = parent->parent ;
  }
  return parent ;
}

static struct rbtree_elem *
predecessor(struct rbtree_elem const *node)
{
  struct rbtree_elem *parent, *left;
  
  left = node->left ;
  if (left != nil) {
    return (struct rbtree_elem *)maximum(left) ;
  }
  parent = node->parent ;
  while (parent != nil && node == parent->left) {
    node = parent ;
    parent = parent->parent ;
  }
  return parent ;
}

static void
rbtree_elem_replace(struct rbtree_elem **rootp, struct rbtree_elem *b,
    struct rbtree_elem const *a)
{
  b->left = a->left;
  b->right = a->right;
  b->parent = a->parent;
  b->color = a->color;

  if (a->parent != nil) {
    if (a->parent->left == a) {
      b->parent->left = b;
    }
    if (a->parent->right == a) {
      b->parent->right = b;
    }
  }
  if (a->left != nil) {
    ASSERT(a->left->parent == a);
    b->left->parent = b;
  }
  if (a->right != nil) {
    ASSERT(a->right->parent == a);
    b->right->parent = b;
  }
  if (*rootp == a) {
    *rootp = b;
  }
}

#if 0
static void
rbnode_copy(rbnode_t dst, rbnode_t src)
{
  dst->low = src->low;
  dst->high = src->high;
}

static bool
search_pred_succ(rbnode_t root, ipaddr_t IP, rbnode_t *succ, rbnode_t *pred)
{
  ipaddr_t successor, predecessor;

  successor = _INFINITY_;
  predecessor = 0;
  *succ = *pred = nil ;

  while (root != nil) {
    if (root->low <= IP && root->high >= IP) {
      return true;
    }
    if (root->low > IP) {
      if (successor >= root->low) {
        successor = root->low;
        *succ = root;
      }
      root = root->left;
    } else {
      ASSERT(root->high < IP);
      if (predecessor <= root->high) {
        predecessor = root->high;
        *pred = root;
      }
      root = root->right;
    }
  }
  return false;
}
#endif

unsigned int
rbtree_height(struct rbtree const *tree)
{
  return height(tree->root);
}

void
rbtree_inorder(struct rbtree const *tree, rbtree_print_func *print, void *aux)
{
  inorder(tree->root, print, aux);
}

/* Returns the beginning of RBTREE.  */
struct rbtree_elem *
rbtree_begin (struct rbtree const *tree)
{
	struct rbtree_elem *min;

  ASSERT (tree != NULL);
	min = (struct rbtree_elem *)minimum(tree->root);
	if (min == nil) {
		return NULL;
	} else {
		return min;
	}
}

/* Returns the beginning of RBTREE.  */
struct rbtree_elem *
rbtree_rbegin (struct rbtree const *tree)
{
	struct rbtree_elem *max;

  ASSERT (tree != NULL);
	max = (struct rbtree_elem *)maximum(tree->root);
	if (max == nil) {
		return NULL;
	} else {
		return max;
	}
}

struct rbtree_elem *
rbtree_rend (struct rbtree const *tree)
{
  ASSERT (tree != NULL);
	return NULL;
}



/* Returns the element after ELEM in its rbtree.  If ELEM is the
   last element in its rbtree, returns the rbtree tail.  Results are
   undefined if ELEM is itself a rbtree tail. */
struct rbtree_elem *
rbtree_next (struct rbtree_elem const *elem)
{
	struct rbtree_elem *next;
	next = successor(elem);
	if (next == nil) {
		return NULL;
	} else {
		return next;
	}
}

/* Returns the element before ELEM in its rbtree.  If ELEM is the
   first element in its rbtree, returns NULL. */
struct rbtree_elem *
rbtree_prev (struct rbtree_elem const *elem)
{
	struct rbtree_elem *pred;
	ASSERT(elem != NULL);
	pred = predecessor(elem);
	if (pred == nil) {
		return NULL;
	} else {
		return pred;
	}
}


/* Returns RBTREE's tail.

   rbtree_end() is often used in iterating through a rbtree from
   beginning to end. */
struct rbtree_elem *
rbtree_end (struct rbtree const *tree)
{
  ASSERT (tree != NULL);
	return NULL;
}
