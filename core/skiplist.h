/*
 * skiplist.h — Skip List for Blood-Group Indexed Donor Lookup
 *
 * A probabilistic data structure that provides O(log n) expected time for
 * search, insert, and delete operations.  Used here to index donors by
 * blood group, enabling fast range queries such as "find all O+ donors".
 *
 * The skip list consists of multiple levels of sorted linked lists.  The
 * bottom level (level 0) contains all elements.  Higher levels act as
 * express lanes that allow the search to skip over large sections.
 *
 * Key properties:
 *   - Expected search time:  O(log n)
 *   - Expected insert time:  O(log n)
 *   - Expected delete time:  O(log n)
 *   - Space:  O(n) expected
 *
 * Reference: William Pugh, "Skip Lists: A Probabilistic Alternative to
 *            Balanced Trees", Communications of the ACM, 1990.
 */
#ifndef BDF_SKIPLIST_H
#define BDF_SKIPLIST_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SL_MAX_LEVEL   16     /* max height; supports ~2^16 = 65536 nodes */
#define SL_PROBABILITY 0.5    /* probability of promoting a node           */

/* Each node stores (blood_group, donor_ref) as a composite key.
 * Nodes are sorted first by blood_group (0-7), then by donor_ref. */
typedef struct SLNode {
    int         blood_group;        /* primary sort key (0-7)             */
    uint64_t    donor_ref;          /* secondary sort key for uniqueness  */
    double      score;              /* cached relevance score             */
    int         level;              /* height of this node (1-based)      */
    struct SLNode **forward;        /* array of forward pointers [0..level-1] */
} SLNode;

typedef struct {
    SLNode *header;     /* dummy header node (before all real nodes)       */
    int     level;      /* current maximum level in use (1-based)          */
    size_t  size;       /* number of real elements                         */
} SkipList;

/* ------------------------------------------------------------------ */
/*  Lifetime                                                          */
/* ------------------------------------------------------------------ */

/* Create an empty skip list.  Returns NULL on allocation failure. */
SkipList *sl_create(void);

/* Free all nodes and the skip list itself. */
void      sl_destroy(SkipList *sl);

/* ------------------------------------------------------------------ */
/*  Mutation                                                          */
/* ------------------------------------------------------------------ */

/* Insert a donor.
 * Returns  0 on success,
 *         -1 if the (blood_group, donor_ref) pair already exists,
 *         -2 on allocation failure. */
int sl_insert(SkipList *sl, int blood_group, uint64_t donor_ref, double score);

/* Delete a donor.
 * Returns  0 on success,
 *         -1 if not found. */
int sl_delete(SkipList *sl, int blood_group, uint64_t donor_ref);

/* ------------------------------------------------------------------ */
/*  Query                                                             */
/* ------------------------------------------------------------------ */

/* Find all donors with a specific blood group.
 * Writes donor_refs into `out_refs` up to `capacity`.
 * Returns the number of matches written. */
size_t sl_find_by_group(const SkipList *sl, int blood_group,
                        uint64_t *out_refs, size_t capacity);

/* Range query: find all donors with blood_group in [lo, hi].
 * Writes donor_refs into `out_refs` up to `capacity`.
 * Returns the number of matches written. */
size_t sl_range_query(const SkipList *sl, int lo, int hi,
                      uint64_t *out_refs, size_t capacity);

/* Return the total number of elements. */
size_t sl_size(const SkipList *sl);

#ifdef __cplusplus
}
#endif
#endif /* BDF_SKIPLIST_H */
