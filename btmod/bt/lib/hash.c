/* Hash table.

   This data structure is thoroughly documented in the Tour of
   Pintos for Project 3.

   See hash.h for basic information. */

#ifdef __builtin_prefetch
#undef __builtin_prefetch
#endif

#include <types.h>
#include <hash.h>
#include <lib/utils.h>
#include <debug.h>
#ifndef __PEEPGEN__
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include "hypercall.h"
#include "peep/tb.h"
#endif
#include "bt_vcpu.h"
#include "mem/malloc.h"

#define mylist_elem_to_hash_elem(LIST_ELEM)                       \
        mylist_entry(LIST_ELEM, struct hash_elem, mylist_elem)

static struct mylist *find_bucket (struct hash *, struct hash_elem *);
static struct hash_elem *find_elem (struct hash *, struct mylist *,
                                    struct hash_elem *);
/*static void
find_iterator (struct hash *h, struct mylist *bucket, struct hash_elem *e,
    struct hash_iterator *i);*/
static void insert_elem (struct hash *, struct mylist *, struct hash_elem *);
static void remove_elem (struct hash *, struct hash_elem *);
static void rehash (struct hash *);

/* Initializes a hash table of size SIZE. */
bool
hash_init_size (struct hash *h, size_t size, hash_hash_func *hash,
    hash_equal_func *equal, void *aux)
{
  h->min_bucket_count = size;
  h->elem_cnt = 0;
  h->bucket_cnt = h->min_bucket_count;
  h->buckets = (struct mylist *)btalloc(sizeof *h->buckets * h->bucket_cnt, PAGE_KERNEL);
  h->hash = hash;
  h->equal = equal;
  h->aux = aux;

  if (h->buckets != NULL) 
    {
      hash_clear (h, NULL);
      return true;
    }
  else
    return false;
}

/* Initializes hash table H to compute hash values using HASH and
   compare hash elements using LESS, given auxiliary data AUX. */
bool
hash_init (struct hash *h, hash_hash_func *hash, hash_equal_func *equal,
    void *aux)
{
  return hash_init_size(h, 4, hash, equal, aux);
}

/* Removes all the elements from H.
   
   If DESTRUCTOR is non-null, then it is called for each element
   in the hash.  DESTRUCTOR may, if appropriate, deallocate the
   memory used by the hash element.  However, modifying hash
   table H while hash_clear() is running, using any of the
   functions hash_clear(), hash_destroy(), hash_insert(),
   hash_replace(), or hash_delete(), yields undefined behavior,
   whether done in DESTRUCTOR or elsewhere. */
void
hash_clear (struct hash *h, hash_action_func *destructor) 
{
  size_t i;

  for (i = 0; i < h->bucket_cnt; i++) 
    {
      struct mylist *bucket = &h->buckets[i];

      if (destructor != NULL) 
        while (!mylist_empty (bucket)) 
          {
            struct mylist_elem *mylist_elem = mylist_pop_front (bucket);
            struct hash_elem *hash_elem = mylist_elem_to_hash_elem (mylist_elem);
            destructor (hash_elem, h->aux);
          }

      mylist_init (bucket);
    }
  h->elem_cnt = 0;
}

/* Destroys hash table H.

   If DESTRUCTOR is non-null, then it is first called for each
   element in the hash.  DESTRUCTOR may, if appropriate,
   deallocate the memory used by the hash element.  However,
   modifying hash table H while hash_clear() is running, using
   any of the functions hash_clear(), hash_destroy(),
   hash_insert(), hash_replace(), or hash_delete(), yields
   undefined behavior, whether done in DESTRUCTOR or
   elsewhere. */
void
hash_destroy (struct hash *h, hash_action_func *destructor) 
{
  if (destructor != NULL)
    hash_clear (h, destructor);
    btfree(h->buckets, sizeof *h->buckets * h->bucket_cnt);
}

/* Inserts NEW into hash table H and returns a null pointer, if
   no equal element is already in the table.
   If an equal element is already in the table, returns it
   without inserting NEW. */   
struct hash_elem *
hash_insert (struct hash *h, struct hash_elem *new)
{
  struct mylist *bucket = find_bucket (h, new);
  struct hash_elem *old = find_elem (h, bucket, new);

  if (old == NULL) 
    insert_elem (h, bucket, new);

  rehash (h);

  return old; 
}

/* Inserts NEW into hash table H, replacing any equal element
   already in the table, which is returned. */
struct hash_elem *
hash_replace (struct hash *h, struct hash_elem *new) 
{
  struct mylist *bucket = find_bucket (h, new);
  struct hash_elem *old = find_elem (h, bucket, new);

  if (old != NULL)
    remove_elem (h, old);
  insert_elem (h, bucket, new);

  rehash (h);

  return old;
}

/* Finds and returns an element equal to E in hash table H, or a
   null pointer if no equal element exists in the table. */
struct hash_elem *
hash_find (struct hash *h, struct hash_elem *e) 
{
  return find_elem (h, find_bucket (h, e), e);
}

/* Returns a mylist of all items whose hash value is equal to the
   hash value of E. */
struct mylist *
hash_find_bucket (struct hash *h, struct hash_elem *e)
{
  return find_bucket (h, e);
}

struct mylist *
hash_find_bucket_with_hash (struct hash *h, unsigned hashval)
{
  return &h->buckets[hashval & (h->bucket_cnt - 1)];
}

/* Finds, removes, and returns an element equal to E in hash
   table H.  Returns a null pointer if no equal element existed
   in the table.

   If the elements of the hash table are dynamically allocated,
   or own resources that are, then it is the caller's
   responsibility to deallocate them. */
struct hash_elem *
hash_delete (struct hash *h, struct hash_elem *e)
{
  struct hash_elem *found = find_elem (h, find_bucket (h, e), e);
  if (found != NULL) 
    {
      remove_elem (h, found);
      rehash (h); 
    }
  return found;
}

/* Calls ACTION for each element in hash table H in arbitrary
   order. 
   Modifying hash table H while hash_apply() is running, using
   any of the functions hash_clear(), hash_destroy(),
   hash_insert(), hash_replace(), or hash_delete(), yields
   undefined behavior, whether done from ACTION or elsewhere. */
void
hash_apply (struct hash *h, hash_action_func *action) 
{
  size_t i;
  
  ASSERT (action != NULL);

  for (i = 0; i < h->bucket_cnt; i++) 
    {
      struct mylist *bucket = &h->buckets[i];
      struct mylist_elem *elem, *next;

      for (elem = mylist_begin (bucket); elem != mylist_end (bucket); elem = next) 
        {
          next = mylist_next (elem);
          action (mylist_elem_to_hash_elem (elem), h->aux);
        }
    }
}

/* Initializes I for iterating hash table H.

   Iteration idiom:

      struct hash_iterator i;

      hash_first (&i, h);
      while (hash_next (&i))
        {
          struct foo *f = hash_entry (hash_cur (&i), struct foo, elem);
          ...do something with f...
        }

   Modifying hash table H during iteration, using any of the
   functions hash_clear(), hash_destroy(), hash_insert(),
   hash_replace(), or hash_delete(), invalidates all
   iterators. */
void
hash_first (struct hash_iterator *i, struct hash *h) 
{
  ASSERT (i != NULL);
  ASSERT (h != NULL);

  i->hash = h;
  i->bucket = i->hash->buckets;
  i->elem = mylist_elem_to_hash_elem (mylist_head (i->bucket));
}

/* Advances I to the next element in the hash table and returns
   it.  Returns a null pointer if no elements are left.  Elements
   are returned in arbitrary order.

   Modifying a hash table H during iteration, using any of the
   functions hash_clear(), hash_destroy(), hash_insert(),
   hash_replace(), or hash_delete(), invalidates all
   iterators. */
struct hash_elem *
hash_next (struct hash_iterator *i)
{
  ASSERT (i != NULL);

  i->elem = mylist_elem_to_hash_elem (mylist_next (&i->elem->mylist_elem));
  while (i->elem == mylist_elem_to_hash_elem (mylist_end (i->bucket)))
    {
      if (++i->bucket >= i->hash->buckets + i->hash->bucket_cnt)
        {
          i->elem = NULL;
          break;
        }
      i->elem = mylist_elem_to_hash_elem (mylist_begin (i->bucket));
    }
  
  return i->elem;
}

/* Returns the current element in the hash table iteration, or a
   null pointer at the end of the table.  Undefined behavior
   after calling hash_first() but before hash_next(). */
struct hash_elem *
hash_cur (struct hash_iterator *i) 
{
  return i->elem;
}

/* Returns the number of elements in H. */
size_t
hash_size (struct hash *h) 
{
  return h->elem_cnt;
}

/* Returns true if H contains no elements, false otherwise. */
bool
hash_empty (struct hash *h) 
{
  return h->elem_cnt == 0;
}

/* Fowler-Noll-Vo hash constants, for 32-bit word sizes. */
#define FNV_32_PRIME 16777619u
#define FNV_32_BASIS 2166136261u

/* Returns a hash of the SIZE bytes in BUF. */
unsigned
hash_bytes (const void *buf_, size_t size)
{
  /* Fowler-Noll-Vo 32-bit hash, for bytes. */
  const unsigned char *buf = buf_;
  unsigned hash;

  ASSERT (buf != NULL);

  hash = FNV_32_BASIS;
  while (size-- > 0)
    hash = (hash * FNV_32_PRIME) ^ *buf++;

  return hash;
} 

/* Returns a hash of string S. */
unsigned
hash_string (const char *s_) 
{
  const unsigned char *s = (const unsigned char *) s_;
  unsigned hash;

  ASSERT (s != NULL);

  hash = FNV_32_BASIS;
  while (*s != '\0')
    hash = (hash * FNV_32_PRIME) ^ *s++;

  return hash;
}

/* Returns a hash of integer I. */
unsigned
hash_int (int i) 
{
  return hash_bytes (&i, sizeof i);
}

/* Returns the bucket in H that E belongs in. */
static struct mylist *
find_bucket (struct hash *h, struct hash_elem *e) 
{
  size_t bucket_idx = h->hash (e, h->aux) & (h->bucket_cnt - 1);
  return &h->buckets[bucket_idx];
}

/* Searches BUCKET in H for a hash element equal to E.  Returns
   it if found or a null pointer otherwise. */
static struct hash_elem *
find_elem (struct hash *h, struct mylist *bucket, struct hash_elem *e) 
{
  struct mylist_elem *i;

  for (i = mylist_begin (bucket); i != mylist_end (bucket); i = mylist_next (i)) {
    struct hash_elem *hi = mylist_elem_to_hash_elem (i);
    if (h->equal (hi, e, h->aux)) {
      return hi;
		}
  }
  return NULL;
}

/* Searches BUCKET in H for a hash element equal to E.  Returns
   an iterator in I, such that hash_next() on the iterator returns all
   elements equal to E. */
/*static void
find_iterator (struct hash *h, struct mylist *bucket, struct hash_elem *e,
    struct hash_iterator *i)
{
  struct mylist_elem *l;

  i->hash = h;
  i->bucket = bucket;
  i->elem = mylist_elem_to_hash_elem (mylist_head(bucket));
  for (l = mylist_begin (bucket); l != mylist_end (bucket); l = mylist_next (l)) {
    struct hash_elem *hi = mylist_elem_to_hash_elem (l);
    if (h->equal (hi, e, h->aux)) {
      return;
    }
    i->elem = mylist_elem_to_hash_elem(l);
  }
}
*/

/* Returns X with its lowest-order bit set to 1 turned off. */
static inline size_t
turn_off_least_1bit (size_t x) 
{
  return x & (x - 1);
}

/* Returns true if X is a power of 2, otherwise false. */
static inline size_t
my_is_power_of_2 (size_t x) 
{
  return x != 0 && turn_off_least_1bit (x) == 0;
}

/* Element per bucket ratios. */
#define MIN_ELEMS_PER_BUCKET  1 /* Elems/bucket < 1: reduce # of buckets. */
#define BEST_ELEMS_PER_BUCKET 2 /* Ideal elems/bucket. */
#define MAX_ELEMS_PER_BUCKET  4 /* Elems/bucket > 4: increase # of buckets. */

/* Changes the number of buckets in hash table H to match the
   ideal.  This function can fail because of an out-of-memory
   condition, but that'll just make hash accesses less efficient;
   we can still continue. */
static void
rehash (struct hash *h) 
{
  size_t old_bucket_cnt, new_bucket_cnt;
  struct mylist *new_buckets, *old_buckets;
  size_t i;

  ASSERT (h != NULL);

  /* Save old bucket info for later use. */
  old_buckets = h->buckets;
  old_bucket_cnt = h->bucket_cnt;

  /* Calculate the number of buckets to use now.
     We want one bucket for about every BEST_ELEMS_PER_BUCKET.
     We must have at least four buckets, and the number of
     buckets must be a power of 2. */
  new_bucket_cnt = h->elem_cnt / BEST_ELEMS_PER_BUCKET;
  if (new_bucket_cnt < h->min_bucket_count)
    new_bucket_cnt = h->min_bucket_count;
  while (!my_is_power_of_2 (new_bucket_cnt))
    new_bucket_cnt = turn_off_least_1bit (new_bucket_cnt);

  /* Don't do anything if the bucket count wouldn't change. */
  if (new_bucket_cnt == old_bucket_cnt)
    return;

  /* Allocate new buckets and initialize them as empty. */
  new_buckets = (struct mylist *)btalloc(sizeof *new_buckets * new_bucket_cnt, PAGE_KERNEL);
  if (new_buckets == NULL) 
    {
      /* Allocation failed.  This means that use of the hash table will
         be less efficient.  However, it is still usable, so
         there's no reason for it to be an error. */
			ABORT();
      return;
    }
  for (i = 0; i < new_bucket_cnt; i++) 
    mylist_init (&new_buckets[i]);

  /* Install new bucket info. */
  h->buckets = new_buckets;
  h->bucket_cnt = new_bucket_cnt;

  /* Move each old element into the appropriate new bucket. */
  for (i = 0; i < old_bucket_cnt; i++) 
    {
      struct mylist *old_bucket;
      struct mylist_elem *elem, *next;

      old_bucket = &old_buckets[i];
      for (elem = mylist_begin (old_bucket);
           elem != mylist_end (old_bucket); elem = next) 
        {
          struct mylist *new_bucket
            = find_bucket (h, mylist_elem_to_hash_elem (elem));
          next = mylist_next (elem);
          mylist_remove (elem);
          mylist_push_front (new_bucket, elem);
        }
    }
    btfree(old_buckets, sizeof(*old_buckets)*old_bucket_cnt);
}

/* Inserts E into BUCKET (in hash table H). */
static void
insert_elem (struct hash *h, struct mylist *bucket, struct hash_elem *e) 
{
  h->elem_cnt++;
  mylist_push_front (bucket, &e->mylist_elem);
}

/* Removes E from hash table H. */
static void
remove_elem (struct hash *h, struct hash_elem *e) 
{
  h->elem_cnt--;
  mylist_remove (&e->mylist_elem);
}

