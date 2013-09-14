#ifndef __LIB_KERNEL_HASH_H
#define __LIB_KERNEL_HASH_H

/* Hash table.

   This data structure is thoroughly documented in the Tour of
   Pintos for Project 3.

   This is a standard hash table with chaining.  To locate an
   element in the table, we compute a hash function over the
   element's data and use that as an index into an array of
   doubly linked mylists, then linearly search the mylist.

   The chain mylists do not use dynamic allocation.  Instead, each
   structure that can potentially be in a hash must embed a
   struct hash_elem member.  All of the hash functions operate on
   these `struct hash_elem's.  The hash_entry macro allows
   conversion from a struct hash_elem back to a structure object
   that contains it.  This is the same technique used in the
   linked mylist implementation.  Refer to lib/kernel/mylist.h for a
   detailed explanation. */

#include <linux/stddef.h>
#include <linux/types.h>
#include <mylist.h>

/* Hash element. */
struct hash_elem 
  {
    struct mylist_elem mylist_elem;
  };

/* Converts pointer to hash element HASH_ELEM into a pointer to
   the structure that HASH_ELEM is embedded inside.  Supply the
   name of the outer structure STRUCT and the member name MEMBER
   of the hash element.  See the big comment at the top of the
   file for an example. */
#define hash_entry(HASH_ELEM, STRUCT, MEMBER)                   \
        ((STRUCT *) ((uint8_t *) &(HASH_ELEM)->mylist_elem        \
                     - offsetof (STRUCT, MEMBER.mylist_elem)))

/* Computes and returns the hash value for hash element E, given
   auxiliary data AUX. */
typedef unsigned hash_hash_func (const struct hash_elem *e, void *aux);

/* Compares the value of two hash elements A and B, given
   auxiliary data AUX.  Returns true if A is equal to B, or
   false if A is greater than or less than B. */
typedef bool hash_equal_func (const struct hash_elem *a,
                              const struct hash_elem *b,
                              void *aux);

/* Performs some operation on hash element E, given auxiliary
   data AUX. */
typedef void hash_action_func (struct hash_elem *e, void *aux);

/* Hash table. */
struct hash 
  {
    size_t elem_cnt;            /* Number of elements in table. */
    size_t bucket_cnt;          /* Number of buckets, a power of 2. */
    struct mylist *buckets;       /* Array of `bucket_cnt' mylists. */
    hash_hash_func *hash;       /* Hash function. */
    hash_equal_func *equal;     /* Comparison function. */
    void *aux;                  /* Auxiliary data for `hash' and `less'. */
    size_t min_bucket_count;    /* The minimum size of the table. default:4. */
  };

/* A hash table iterator. */
struct hash_iterator 
  {
    struct hash *hash;          /* The hash table. */
    struct mylist *bucket;        /* Current bucket. */
    struct hash_elem *elem;     /* Current hash element in current bucket. */
  };

/* Basic life cycle. */
bool hash_init (struct hash *, hash_hash_func *, hash_equal_func *, void *aux);
bool hash_init_size (struct hash *h, size_t size, hash_hash_func *hash,
    hash_equal_func *equal, void *aux);
void hash_clear (struct hash *, hash_action_func *);
void hash_destroy (struct hash *, hash_action_func *);

/* Search, insertion, deletion. */
struct hash_elem *hash_insert (struct hash *, struct hash_elem *);
struct hash_elem *hash_replace (struct hash *, struct hash_elem *);
struct hash_elem *hash_find (struct hash *, struct hash_elem *);
struct hash_elem *hash_delete (struct hash *, struct hash_elem *);

/* The following functions should be used if duplicate elements are
   allowed. */
struct mylist *hash_find_bucket (struct hash *, struct hash_elem *);
struct mylist *hash_find_bucket_with_hash (struct hash *, unsigned hval);

/* Iteration. */
void hash_apply (struct hash *, hash_action_func *);
void hash_first (struct hash_iterator *, struct hash *);
struct hash_elem *hash_next (struct hash_iterator *);
struct hash_elem *hash_cur (struct hash_iterator *);

/* Information. */
size_t hash_size (struct hash *);
bool hash_empty (struct hash *);

/* Sample hash functions. */
unsigned hash_bytes (const void *, size_t);
unsigned hash_string (const char *);
unsigned hash_int (int);

#endif /* lib/kernel/hash.h */
