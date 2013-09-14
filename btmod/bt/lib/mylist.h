#ifndef __LIB_MYLIST_H
#define __LIB_MYLIST_H

/* Doubly linked list.

   This implementation of a doubly linked list does not require
   use of dynamically allocated memory.  Instead, each structure
   that is a potential list element must embed a struct list_elem
   member.  All of the list functions operate on these `struct
   list_elem's.  The list_entry macro allows conversion from a
   struct list_elem back to a structure object that contains it.

   For example, suppose there is a needed for a list of `struct
   foo'.  `struct foo' should contain a `struct list_elem'
   member, like so:

      struct foo
        {
          struct list_elem elem;
          int bar;
          ...other members...
        };

   Then a list of `struct foo' can be be declared and initialized
   like so:

      struct list foo_list;

      list_init (&foo_list);

   Iteration is a typical situation where it is necessary to
   convert from a struct list_elem back to its enclosing
   structure.  Here's an example using foo_list:

      struct list_elem *e;

      for (e = list_begin (&foo_list); e != list_end (&foo_list);
           e = list_next (e))
        {
          struct foo *f = list_entry (e, struct foo, elem);
          ...do something with f...
        }

   You can find real examples of list usage throughout the
   source; for example, malloc.c, palloc.c, and thread.c in the
   threads directory all use lists.

   The interface for this list is inspired by the list<> template
   in the C++ STL.  If you're familiar with list<>, you should
   find this easy to use.  However, it should be emphasized that
   these lists do *no* type checking and can't do much other
   correctness checking.  If you screw up, it will bite you.

   Glossary of list terms:

     - "front": The first element in a list.  Undefined in an
       empty list.  Returned by list_front().

     - "back": The last element in a list.  Undefined in an empty
       list.  Returned by list_back().

     - "tail": The element figuratively just after the last
       element of a list.  Well defined even in an empty list.
       Returned by list_end().  Used as the end sentinel for an
       iteration from front to back.

     - "beginning": In a non-empty list, the front.  In an empty
       list, the tail.  Returned by list_begin().  Used as the
       starting point for an iteration from front to back.

     - "head": The element figuratively just before the first
       element of a list.  Well defined even in an empty list.
       Returned by list_rend().  Used as the end sentinel for an
       iteration from back to front.

     - "reverse beginning": In a non-empty list, the back.  In an
       empty list, the head.  Returned by list_rbegin().  Used as
       the starting point for an iteration from back to front.

     - "interior element": An element that is not the head or
       tail, that is, a real list element.  An empty list does
       not have any interior elements.
*/

#include <linux/stddef.h>
#include <linux/types.h>

/* List element. */
struct mylist_elem {
  struct mylist_elem *prev;     /* Previous mylist element. */
  struct mylist_elem *next;     /* Next mylist element. */
};

/* List. */
struct mylist {
  struct mylist_elem head;      /* List head. */
  struct mylist_elem tail;      /* List tail. */
};

/* Converts pointer to mylist element MYLIST_ELEM into a pointer to
   the structure that MYLIST_ELEM is embedded inside.  Supply the
   name of the outer structure STRUCT and the member name MEMBER
   of the mylist element.  See the big comment at the top of the
   file for an example. */
#define mylist_entry(MYLIST_ELEM, STRUCT, MEMBER)           \
        ((STRUCT *) ((uint8_t *) &(MYLIST_ELEM)->next     \
                     - offsetof (STRUCT, MEMBER.next)))

/* List initialization.

   A mylist may be initialized by calling mylist_init():

       struct mylist my_mylist;
       mylist_init (&my_mylist);

   or with an initializer using MYLIST_INITIALIZER:

       struct mylist my_mylist = MYLIST_INITIALIZER (my_mylist); */
#define MYLIST_INITIALIZER(NAME) { { NULL, &(NAME).tail }, \
                                 { &(NAME).head, NULL } }


void mylist_init (struct mylist *);

/* List traversal. */
struct mylist_elem *mylist_begin (struct mylist *);
struct mylist_elem *mylist_next (struct mylist_elem *);
struct mylist_elem *mylist_end (struct mylist *);

struct mylist_elem *mylist_rbegin (struct mylist *);
struct mylist_elem *mylist_prev (struct mylist_elem *);
struct mylist_elem *mylist_rend (struct mylist *);

struct mylist_elem *mylist_head (struct mylist *);
struct mylist_elem *mylist_tail (struct mylist *);

/* List insertion. */
void mylist_insert (struct mylist_elem *, struct mylist_elem *);
void mylist_splice (struct mylist_elem *before,
                  struct mylist_elem *first, struct mylist_elem *last);
void mylist_push_front (struct mylist *, struct mylist_elem *);
void mylist_push_back (struct mylist *, struct mylist_elem *);

/* List removal. */
struct mylist_elem *mylist_remove (struct mylist_elem *);
struct mylist_elem *mylist_pop_front (struct mylist *);
struct mylist_elem *mylist_pop_back (struct mylist *);

/* List elements. */
struct mylist_elem *mylist_front (struct mylist *);
struct mylist_elem *mylist_back (struct mylist *);

/* List properties. */
size_t mylist_size (struct mylist *);
bool mylist_empty (struct mylist *);

/* Miscellaneous. */
//void mylist_reverse (struct mylist *);

/* Compares the value of two mylist elements A and B, given
   auxiliary data AUX.  Returns true if A is less than B, or
   false if A is greater than or equal to B. */
typedef bool mylist_less_func (const struct mylist_elem *a,
                             const struct mylist_elem *b,
                             void *aux);

/* Operations on mylists with ordered elements. */
void mylist_sort (struct mylist *,
                mylist_less_func *, void *aux);
void mylist_insert_ordered (struct mylist *, struct mylist_elem *,
                          mylist_less_func *, void *aux);
void mylist_unique (struct mylist *, struct mylist *duplicates,
                  mylist_less_func *, void *aux);

/* Max and min. */
struct mylist_elem *mylist_max (struct mylist *, mylist_less_func *, void *aux);
struct mylist_elem *mylist_min (struct mylist *, mylist_less_func *, void *aux);

/**
 * mylist_for_each_entry	-	iterate over mylist of given type
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your mylist.
 * @member:	the name of the mylist_struct within the struct.
 */
#define mylist_for_each_entry(pos, head, member)				\
	for (pos = mylist_entry((head)->next, typeof(*pos), member);	\
	     prefetch(pos->member.next), &pos->member != (head); 	\
	     pos = mylist_entry(pos->member.next, typeof(*pos), member))

/**
 * mylist_for_each_entry_safe - iterate over mylist of given type safe against removal of mylist entry
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your mylist.
 * @member:	the name of the mylist_struct within the struct.
 */
#define mylist_for_each_entry_safe(pos, n, head, member)			\
	for (pos = mylist_entry((head)->next, typeof(*pos), member),	\
		n = mylist_entry(pos->member.next, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = n, n = mylist_entry(n->member.next, typeof(*n), member))

/**
 * mylist_for_each  - iterate over a mylist
 * @pos:  the &struct mylist_head to use as a loop cursor.
 * @head: the head for your mylist.
 */
#define mylist_for_each(pos, head) \
  for (pos = (head)->next; prefetch(pos->next), pos != (head); \
      pos = pos->next)



#define MYLIST_HEAD_INIT(name) { &(name), &(name) }

#define MYLIST_HEAD(name) \
    struct mylist_head name = MYLIST_HEAD_INIT(name)

static inline void INIT_MYLIST_HEAD(struct mylist_elem *mylist)
{
  mylist->next = mylist;
  mylist->prev = mylist;
}

/*
 * Insert a new entry between two known consecutive entries.
 *
 * This is only for internal mylist manipulation where we know
 * the prev/next entries already!
 */
static inline void __mylist_add(struct mylist_elem *new,
			      struct mylist_elem *prev,
			      struct mylist_elem *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

/**
 * mylist_add - add a new entry
 * @new: new entry to be added
 * @head: mylist head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 */
static inline void mylist_add(struct mylist_elem *new, struct mylist_elem *head)
{
	__mylist_add(new, head, head->next);
}


/**
 * mylist_add_tail - add a new entry
 * @new: new entry to be added
 * @head: mylist head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
static inline void mylist_add_tail(struct mylist_elem *new, struct mylist_elem *head)
{
	__mylist_add(new, head->prev, head);
}

/*
 * Delete a mylist entry by making the prev/next entries
 * point to each other.
 *
 * This is only for internal mylist manipulation where we know
 * the prev/next entries already!
 */
static inline void __mylist_del(struct mylist_elem * prev, struct mylist_elem * next)
{
	next->prev = prev;
	prev->next = next;
}

/********** include/linux/mylist.h **********/
/*
 *  These are non-NULL pointers that will result in page faults
 *  under normal circumstances, used to verify that nobody uses
 *  non-initialized mylist entries.
 */
#define MYLIST_POISON1  NULL //((void *) 0x00100100)
#define MYLIST_POISON2  NULL //((void *) 0x00200200)

/**
 * mylist_del - deletes entry from mylist.
 * @entry: the element to delete from the mylist.
 * Note: mylist_empty() on entry does not return true after this, the entry is
 * in an undefined state.
 */
static inline void mylist_del(struct mylist_elem *entry)
{
	__mylist_del(entry->prev, entry->next);
	entry->next = MYLIST_POISON1;
	entry->prev = MYLIST_POISON2;
}

/**
 * mylist_replace - replace old entry by new one
 * @old : the element to be replaced
 * @new : the new element to insert
 *
 * If @old was empty, it will be overwritten.
 */
static inline void mylist_replace(struct mylist_elem *old,
				struct mylist_elem *new)
{
	new->next = old->next;
	new->next->prev = new;
	new->prev = old->prev;
	new->prev->next = new;
}

static inline void mylist_replace_init(struct mylist_elem *old,
					struct mylist_elem *new)
{
	mylist_replace(old, new);
	INIT_MYLIST_HEAD(old);
}

/**
 * mylist_del_init - deletes entry from mylist and reinitialize it.
 * @entry: the element to delete from the mylist.
 */
static inline void mylist_del_init(struct mylist_elem *entry)
{
	__mylist_del(entry->prev, entry->next);
	INIT_MYLIST_HEAD(entry);
}

/**
 * mylist_move - delete from one mylist and add as another's head
 * @mylist: the entry to move
 * @head: the head that will precede our entry
 */
static inline void mylist_move(struct mylist_elem *mylist, struct mylist_elem *head)
{
	__mylist_del(mylist->prev, mylist->next);
	mylist_add(mylist, head);
}

/**
 * mylist_move_tail - delete from one mylist and add as another's tail
 * @mylist: the entry to move
 * @head: the head that will follow our entry
 */
static inline void mylist_move_tail(struct mylist_elem *mylist,
				  struct mylist_elem *head)
{
	__mylist_del(mylist->prev, mylist->next);
	mylist_add_tail(mylist, head);
}

/**
 * mylist_is_last - tests whether @mylist is the last entry in mylist @head
 * @mylist: the entry to test
 * @head: the head of the mylist
 */
static inline int mylist_is_last(const struct mylist_elem *mylist,
				const struct mylist_elem *head)
{
	return mylist->next == head;
}

/**
 * mylist_empty - tests whether a mylist is empty
 * @head: the mylist to test.
 */
static inline int mylist_head_empty(const struct mylist_elem *head)
{
	return head->next == head;
}

/**
 * mylist_empty_careful - tests whether a mylist is empty and not being modified
 * @head: the mylist to test
 *
 * Description:
 * tests whether a mylist is empty _and_ checks that no other CPU might be
 * in the process of modifying either member (next or prev)
 *
 * NOTE: using mylist_empty_careful() without synchronization
 * can only be safe if the only activity that can happen
 * to the mylist entry is mylist_del_init(). Eg. it cannot be used
 * if another CPU could re-mylist_add() it.
 */
static inline int mylist_empty_careful(const struct mylist_elem *head)
{
	struct mylist_elem *next = head->next;
	return (next == head) && (next == head->prev);
}

/**
 * mylist_is_singular - tests whether a mylist has just one entry.
 * @head: the mylist to test.
 */
static inline int mylist_is_singular(const struct mylist_elem *head)
{
	return !mylist_head_empty(head) && (head->next == head->prev);
}

static inline void __mylist_cut_position(struct mylist_elem *mylist,
		struct mylist_elem *head, struct mylist_elem *entry)
{
	struct mylist_elem *new_first = entry->next;
	mylist->next = head->next;
	mylist->next->prev = mylist;
	mylist->prev = entry;
	entry->next = mylist;
	head->next = new_first;
	new_first->prev = head;
}

/**
 * mylist_cut_position - cut a mylist into two
 * @mylist: a new mylist to add all removed entries
 * @head: a mylist with entries
 * @entry: an entry within head, could be the head itself
 *	and if so we won't cut the mylist
 *
 * This helper moves the initial part of @head, up to and
 * including @entry, from @head to @mylist. You should
 * pass on @entry an element you know is on @head. @mylist
 * should be an empty mylist or a mylist you do not care about
 * losing its data.
 *
 */
static inline void mylist_cut_position(struct mylist_elem *mylist,
		struct mylist_elem *head, struct mylist_elem *entry)
{
	if (mylist_head_empty(head))
		return;
	if (mylist_is_singular(head) &&
		(head->next != entry && head != entry))
		return;
	if (entry == head)
		INIT_MYLIST_HEAD(mylist);
	else
		__mylist_cut_position(mylist, head, entry);
}

static inline void __mylist_head_splice(const struct mylist_elem *mylist,
				 struct mylist_elem *prev,
				 struct mylist_elem *next)
{
	struct mylist_elem *first = mylist->next;
	struct mylist_elem *last = mylist->prev;

	first->prev = prev;
	prev->next = first;

	last->next = next;
	next->prev = last;
}

#if 0
/**
 * mylist_head_splice - join two mylists, this is designed for stacks
 * @mylist: the new mylist to add.
 * @head: the place to add it in the first mylist.
 */
static inline void mylist_head_splice(const struct mylist_elem *mylist,
				struct mylist_elem *head)
{
	if (!mylist_head_empty(mylist))
		__mylist_head_splice(mylist, head, head->next);
}

/**
 * mylist_splice_tail - join two mylists, each mylist being a queue
 * @mylist: the new mylist to add.
 * @head: the place to add it in the first mylist.
 */
static inline void mylist_splice_tail(struct mylist_elem *mylist,
				struct mylist_elem *head)
{
	if (!mylist_head_empty(mylist))
		__mylist_splice(mylist, head->prev, head);
}

/**
 * mylist_splice_init - join two mylists and reinitialise the emptied mylist.
 * @mylist: the new mylist to add.
 * @head: the place to add it in the first mylist.
 *
 * The mylist at @mylist is reinitialised
 */
static inline void mylist_splice_init(struct mylist_elem *mylist,
				    struct mylist_elem *head)
{
	if (!mylist_head_empty(mylist)) {
		__mylist_splice(mylist, head, head->next);
		INIT_MYLIST_HEAD(mylist);
	}
}

/**
 * mylist_splice_tail_init - join two mylists and reinitialise the emptied mylist
 * @mylist: the new mylist to add.
 * @head: the place to add it in the first mylist.
 *
 * Each of the mylists is a queue.
 * The mylist at @mylist is reinitialised
 */
static inline void mylist_splice_tail_init(struct mylist_elem *mylist,
					 struct mylist_elem *head)
{
	if (!mylist_head_empty(mylist)) {
		__mylist_splice(mylist, head->prev, head);
		INIT_MYLIST_HEAD(mylist);
	}
}
#endif

#endif /* lib/list.h */
