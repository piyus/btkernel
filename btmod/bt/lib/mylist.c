#include <types.h>
#include <debug.h>
#ifndef __PEEPGEN__
//#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>
#include "hypercall.h"
#endif
#include "mylist.h"

/* Our doubly linked mylists have two header elements: the "head"
   just before the first element and the "tail" just after the
   last element.  The `prev' link of the front header is null, as
   is the `next' link of the back header.  Their other two links
   point toward each other via the interior elements of the mylist.

   An empty mylist looks like this:

                      +------+     +------+
                  <---| head |<--->| tail |--->
                      +------+     +------+

   A mylist with two elements in it looks like this:

        +------+     +-------+     +-------+     +------+
    <---| head |<--->|   1   |<--->|   2   |<--->| tail |<--->
        +------+     +-------+     +-------+     +------+

   The symmetry of this arrangement eliminates lots of special
   cases in mylist processing.  For example, take a look at
   mylist_remove(): it takes only two pointer assignments and no
   conditionals.  That's a lot simpler than the code would be
   without header elements.

   (Because only one of the pointers in each header element is used,
   we could in fact combine them into a single header element
   without sacrificing this simplicity.  But using two separate
   elements allows us to do a little bit of checking on some
   operations, which can be valuable.) */

static bool is_sorted (struct mylist_elem *a, struct mylist_elem *b,
                       mylist_less_func *less, void *aux) UNUSED;

/* Returns true if ELEM is a head, false otherwise. */
static inline bool
is_head (struct mylist_elem *elem)
{
  return elem != NULL && elem->prev == NULL && elem->next != NULL;
}

/* Returns true if ELEM is an interior element,
   false otherwise. */
static inline bool
is_interior (struct mylist_elem *elem)
{
  return elem != NULL && elem->prev != NULL && elem->next != NULL;
}

/* Returns true if ELEM is a tail, false otherwise. */
static inline bool
is_tail (struct mylist_elem *elem)
{
  return elem != NULL && elem->prev != NULL && elem->next == NULL;
}

/* Initializes MYLIST as an empty mylist. */
void
mylist_init (struct mylist *mylist)
{
  ASSERT (mylist != NULL);
  mylist->head.prev = NULL;
  mylist->head.next = &mylist->tail;
  mylist->tail.prev = &mylist->head;
  mylist->tail.next = NULL;
}

/* Returns the beginning of MYLIST.  */
struct mylist_elem *
mylist_begin (struct mylist *mylist)
{
  ASSERT (mylist != NULL);
  return mylist->head.next;
}

/* Returns the element after ELEM in its mylist.  If ELEM is the
   last element in its mylist, returns the mylist tail.  Results are
   undefined if ELEM is itself a mylist tail. */
struct mylist_elem *
mylist_next (struct mylist_elem *elem)
{
  ASSERT (is_head (elem) || is_interior (elem));
  return elem->next;
}

/* Returns MYLIST's tail.

   mylist_end() is often used in iterating through a mylist from
   front to back.  See the big comment at the top of mylist.h for
   an example. */
struct mylist_elem *
mylist_end (struct mylist *mylist)
{
  ASSERT (mylist != NULL);
  return &mylist->tail;
}

/* Returns the MYLIST's reverse beginning, for iterating through
   MYLIST in reverse order, from back to front. */
struct mylist_elem *
mylist_rbegin (struct mylist *mylist) 
{
  ASSERT (mylist != NULL);
  return mylist->tail.prev;
}

/* Returns the element before ELEM in its mylist.  If ELEM is the
   first element in its mylist, returns the mylist head.  Results are
   undefined if ELEM is itself a mylist head. */
struct mylist_elem *
mylist_prev (struct mylist_elem *elem)
{
  ASSERT (is_interior (elem) || is_tail (elem));
  return elem->prev;
}

/* Returns MYLIST's head.

   mylist_rend() is often used in iterating through a mylist in
   reverse order, from back to front.  Here's typical usage,
   following the example from the top of mylist.h:

      for (e = mylist_rbegin (&foo_mylist); e != mylist_rend (&foo_mylist);
           e = mylist_prev (e))
        {
          struct foo *f = mylist_entry (e, struct foo, elem);
          ...do something with f...
        }
*/
struct mylist_elem *
mylist_rend (struct mylist *mylist) 
{
  ASSERT (mylist != NULL);
  return &mylist->head;
}

/* Return's MYLIST's head.

   mylist_head() can be used for an alternate style of iterating
   through a mylist, e.g.:

      e = mylist_head (&mylist);
      while ((e = mylist_next (e)) != mylist_end (&mylist)) 
        {
          ...
        }
*/
struct mylist_elem *
mylist_head (struct mylist *mylist) 
{
  ASSERT (mylist != NULL);
  return &mylist->head;
}

/* Return's MYLIST's tail. */
struct mylist_elem *
mylist_tail (struct mylist *mylist) 
{
  ASSERT (mylist != NULL);
  return &mylist->tail;
}

/* Inserts ELEM just before BEFORE, which may be either an
   interior element or a tail.  The latter case is equivalent to
   mylist_push_back(). */
void
mylist_insert (struct mylist_elem *before, struct mylist_elem *elem)
{
  ASSERT (is_interior (before) || is_tail (before));
  ASSERT (elem != NULL);

  elem->prev = before->prev;
  elem->next = before;
  before->prev->next = elem;
  before->prev = elem;
}

/* Removes elements FIRST though LAST (exclusive) from their
   current mylist, then inserts them just before BEFORE, which may
   be either an interior element or a tail. */
void
mylist_splice (struct mylist_elem *before,
             struct mylist_elem *first, struct mylist_elem *last)
{
  ASSERT (is_interior (before) || is_tail (before));
  if (first == last)
    return;
  last = mylist_prev (last);

  ASSERT (is_interior (first));
  ASSERT (is_interior (last));

  /* Cleanly remove FIRST...LAST from its current mylist. */
  first->prev->next = last->next;
  last->next->prev = first->prev;

  /* Splice FIRST...LAST into new mylist. */
  first->prev = before->prev;
  last->next = before;
  before->prev->next = first;
  before->prev = last;
}

/* Inserts ELEM at the beginning of MYLIST, so that it becomes the
   front in MYLIST. */
void
mylist_push_front (struct mylist *mylist, struct mylist_elem *elem)
{
  mylist_insert (mylist_begin (mylist), elem);
}

/* Inserts ELEM at the end of MYLIST, so that it becomes the
   back in MYLIST. */
void
mylist_push_back (struct mylist *mylist, struct mylist_elem *elem)
{
  mylist_insert (mylist_end (mylist), elem);
}

/* Removes ELEM from its mylist and returns the element that
   followed it.  Undefined behavior if ELEM is not in a mylist.

   It's not safe to treat ELEM as an element in a mylist after
   removing it.  In particular, using mylist_next() or mylist_prev()
   on ELEM after removal yields undefined behavior.  This means
   that a naive loop to remove the elements in a mylist will fail:

   ** DON'T DO THIS **
   for (e = mylist_begin (&mylist); e != mylist_end (&mylist); e = mylist_next (e))
     {
       ...do something with e...
       mylist_remove (e);
     }
   ** DON'T DO THIS **

   Here is one correct way to iterate and remove elements from a
   mylist:

   for (e = mylist_begin (&mylist); e != mylist_end (&mylist); e = mylist_remove (e))
     {
       ...do something with e...
     }

   If you need to free() elements of the mylist then you need to be
   more conservative.  Here's an alternate strategy that works
   even in that case:

   while (!mylist_empty (&mylist))
     {
       struct mylist_elem *e = mylist_pop_front (&mylist);
       ...do something with e...
     }
*/
struct mylist_elem *
mylist_remove (struct mylist_elem *elem)
{
  ASSERT (is_interior (elem));
  elem->prev->next = elem->next;
  elem->next->prev = elem->prev;
  return elem->next;
}

/* Removes the front element from MYLIST and returns it.
   Undefined behavior if MYLIST is empty before removal. */
struct mylist_elem *
mylist_pop_front (struct mylist *mylist)
{
  struct mylist_elem *front = mylist_front (mylist);
	if (!front) PANIC("%s", "");
  mylist_remove (front);
  return front;
}

/* Removes the back element from MYLIST and returns it.
   Undefined behavior if MYLIST is empty before removal. */
struct mylist_elem *
mylist_pop_back (struct mylist *mylist)
{
  struct mylist_elem *back = mylist_back (mylist);
  mylist_remove (back);
  return back;
}

/* Returns the front element in MYLIST.
   Undefined behavior if MYLIST is empty. */
struct mylist_elem *
mylist_front (struct mylist *mylist)
{
  ASSERT (!mylist_empty (mylist));
  return mylist->head.next;
}

/* Returns the back element in MYLIST.
   Undefined behavior if MYLIST is empty. */
struct mylist_elem *
mylist_back (struct mylist *mylist)
{
  ASSERT (!mylist_empty (mylist));
  return mylist->tail.prev;
}

/* Returns the number of elements in MYLIST.
   Runs in O(n) in the number of elements. */
size_t
mylist_size (struct mylist *mylist)
{
  struct mylist_elem *e;
  size_t cnt = 0;

  for (e = mylist_begin (mylist); e != mylist_end (mylist); e = mylist_next (e))
    cnt++;
  return cnt;
}

/* Returns true if MYLIST is empty, false otherwise. */
bool
mylist_empty (struct mylist *mylist)
{
  return mylist_begin (mylist) == mylist_end (mylist);
}

#if 0
/* Swaps the `struct mylist_elem *'s that A and B point to. */
static void
myswap (struct mylist_elem **a, struct mylist_elem **b) 
{
  struct mylist_elem *t = *a;
  *a = *b;
  *b = t;
}

/* Reverses the order of MYLIST. */
void
mylist_reverse (struct mylist *mylist)
{
  if (!mylist_empty (mylist)) 
    {
      struct mylist_elem *e;

      for (e = mylist_begin (mylist); e != mylist_end (mylist); e = e->prev)
        myswap (&e->prev, &e->next);
      myswap (&mylist->head.next, &mylist->tail.prev);
      myswap (&mylist->head.next->prev, &mylist->tail.prev->next);
    }
}
#endif

/* Returns true only if the mylist elements A through B (exclusive)
   are in order according to LESS given auxiliary data AUX. */
static bool
is_sorted (struct mylist_elem *a, struct mylist_elem *b,
           mylist_less_func *less, void *aux)
{
  if (a != b)
    while ((a = mylist_next (a)) != b) 
      if (less (a, mylist_prev (a), aux))
        return false;
  return true;
}

/* Finds a run, starting at A and ending not after B, of mylist
   elements that are in nondecreasing order according to LESS
   given auxiliary data AUX.  Returns the (exclusive) end of the
   run.
   A through B (exclusive) must form a non-empty range. */
static struct mylist_elem *
find_end_of_run (struct mylist_elem *a, struct mylist_elem *b,
                 mylist_less_func *less, void *aux)
{
  ASSERT (a != NULL);
  ASSERT (b != NULL);
  ASSERT (less != NULL);
  ASSERT (a != b);
  
  do 
    {
      a = mylist_next (a);
    }
  while (a != b && !less (a, mylist_prev (a), aux));
  return a;
}

/* Merges A0 through A1B0 (exclusive) with A1B0 through B1
   (exclusive) to form a combined range also ending at B1
   (exclusive).  Both input ranges must be nonempty and sorted in
   nondecreasing order according to LESS given auxiliary data
   AUX.  The output range will be sorted the same way. */
static void
inplace_merge (struct mylist_elem *a0, struct mylist_elem *a1b0,
               struct mylist_elem *b1,
               mylist_less_func *less, void *aux)
{
  ASSERT (a0 != NULL);
  ASSERT (a1b0 != NULL);
  ASSERT (b1 != NULL);
  ASSERT (less != NULL);
  ASSERT (is_sorted (a0, a1b0, less, aux));
  ASSERT (is_sorted (a1b0, b1, less, aux));

  while (a0 != a1b0 && a1b0 != b1)
    if (!less (a1b0, a0, aux)) 
      a0 = mylist_next (a0);
    else 
      {
        a1b0 = mylist_next (a1b0);
        mylist_splice (a0, mylist_prev (a1b0), a1b0);
      }
}

/* Sorts MYLIST according to LESS given auxiliary data AUX, using a
   natural iterative merge sort that runs in O(n lg n) time and
   O(1) space in the number of elements in MYLIST. */
void
mylist_sort (struct mylist *mylist, mylist_less_func *less, void *aux)
{
  size_t output_run_cnt;        /* Number of runs output in current pass. */

  ASSERT (mylist != NULL);
  ASSERT (less != NULL);

  /* Pass over the mylist repeatedly, merging adjacent runs of
     nondecreasing elements, until only one run is left. */
  do
    {
      struct mylist_elem *a0;     /* Start of first run. */
      struct mylist_elem *a1b0;   /* End of first run, start of second. */
      struct mylist_elem *b1;     /* End of second run. */

      output_run_cnt = 0;
      for (a0 = mylist_begin (mylist); a0 != mylist_end (mylist); a0 = b1)
        {
          /* Each iteration produces one output run. */
          output_run_cnt++;

          /* Locate two adjacent runs of nondecreasing elements
             A0...A1B0 and A1B0...B1. */
          a1b0 = find_end_of_run (a0, mylist_end (mylist), less, aux);
          if (a1b0 == mylist_end (mylist))
            break;
          b1 = find_end_of_run (a1b0, mylist_end (mylist), less, aux);

          /* Merge the runs. */
          inplace_merge (a0, a1b0, b1, less, aux);
        }
    }
  while (output_run_cnt > 1);

  ASSERT (is_sorted (mylist_begin (mylist), mylist_end (mylist), less, aux));
}

/* Inserts ELEM in the proper position in MYLIST, which must be
   sorted according to LESS given auxiliary data AUX.
   Runs in O(n) average case in the number of elements in MYLIST. */
void
mylist_insert_ordered (struct mylist *mylist, struct mylist_elem *elem,
                     mylist_less_func *less, void *aux)
{
  struct mylist_elem *e;

  ASSERT (mylist != NULL);
  ASSERT (elem != NULL);
  ASSERT (less != NULL);

  for (e = mylist_begin (mylist); e != mylist_end (mylist); e = mylist_next (e))
    if (less (elem, e, aux))
      break;
  return mylist_insert (e, elem);
}

/* Iterates through MYLIST and removes all but the first in each
   set of adjacent elements that are equal according to LESS
   given auxiliary data AUX.  If DUPLICATES is non-null, then the
   elements from MYLIST are appended to DUPLICATES. */
void
mylist_unique (struct mylist *mylist, struct mylist *duplicates,
             mylist_less_func *less, void *aux)
{
  struct mylist_elem *elem, *next;

  ASSERT (mylist != NULL);
  ASSERT (less != NULL);
  if (mylist_empty (mylist))
    return;

  elem = mylist_begin (mylist);
  while ((next = mylist_next (elem)) != mylist_end (mylist))
    if (!less (elem, next, aux) && !less (next, elem, aux)) 
      {
        mylist_remove (next);
        if (duplicates != NULL)
          mylist_push_back (duplicates, next);
      }
    else
      elem = next;
}

/* Returns the element in MYLIST with the largest value according
   to LESS given auxiliary data AUX.  If there is more than one
   maximum, returns the one that appears earlier in the mylist.  If
   the mylist is empty, returns its tail. */
struct mylist_elem *
mylist_max (struct mylist *mylist, mylist_less_func *less, void *aux)
{
  struct mylist_elem *max = mylist_begin (mylist);
  if (max != mylist_end (mylist)) 
    {
      struct mylist_elem *e;
      
      for (e = mylist_next (max); e != mylist_end (mylist); e = mylist_next (e))
        if (less (max, e, aux))
          max = e; 
    }
  return max;
}

/* Returns the element in MYLIST with the smallest value according
   to LESS given auxiliary data AUX.  If there is more than one
   minimum, returns the one that appears earlier in the mylist.  If
   the mylist is empty, returns its tail. */
struct mylist_elem *
mylist_min (struct mylist *mylist, mylist_less_func *less, void *aux)
{
  struct mylist_elem *min = mylist_begin (mylist);
  if (min != mylist_end (mylist)) 
    {
      struct mylist_elem *e;
      
      for (e = mylist_next (min); e != mylist_end (mylist); e = mylist_next (e))
        if (less (e, min, aux))
          min = e; 
    }
  return min;
}
