/*
 * rbtree.h — Red-Black Tree for Donor Registry
 *
 * A self-balancing binary search tree that guarantees O(log n) worst-case
 * time for insert, delete, and search operations.  Used as the primary
 * in-memory donor registry, keyed by donor_ref (uint64_t).
 *
 * Red-Black Tree invariants maintained by all mutating operations:
 *   1. Every node is either RED or BLACK.
 *   2. The root is always BLACK.
 *   3. No two consecutive RED nodes on any path (red node => black children).
 *   4. Every path from a node to its descendant NIL leaves contains the same
 *      number of BLACK nodes (the "black-height" property).
 *
 * Reference: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms,
 *            Chapter 13 "Red-Black Trees".
 */
#ifndef BDF_RBTREE_H
#define BDF_RBTREE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Types                                                             */
/* ------------------------------------------------------------------ */

typedef enum { RB_RED = 0, RB_BLACK = 1 } RBColor;

typedef struct RBNode {
    uint64_t    key;                    /* donor_ref — unique identifier   */
    int         blood_group;            /* BDFBloodGroup enum value (0-7)  */
    double      latitude;
    double      longitude;
    double      eta_minutes;
    int         consent_current;        /* 0 or 1                          */
    int         available;              /* 0 or 1                          */
    int         blood_group_verified;   /* 0 or 1                          */
    int         profile_active;         /* 0 or 1                          */
    int64_t     consent_checked_at;
    int64_t     availability_checked_at;
    int64_t     eta_checked_at;
    RBColor     color;
    struct RBNode *left, *right, *parent;
} RBNode;

typedef struct {
    RBNode *root;
    RBNode *nil;        /* sentinel NIL node (always BLACK)                */
    size_t  size;       /* number of real nodes (excluding the sentinel)   */
} RBTree;

/* ------------------------------------------------------------------ */
/*  Lifetime                                                          */
/* ------------------------------------------------------------------ */

/* Create an empty Red-Black Tree.  Returns NULL on allocation failure. */
RBTree *rbt_create(void);

/* Free every node and the tree itself. */
void    rbt_destroy(RBTree *t);

/* ------------------------------------------------------------------ */
/*  Mutation                                                          */
/* ------------------------------------------------------------------ */

/* Insert a donor into the tree.
 * Returns  0 on success,
 *         -1 if key already exists (duplicate donor_ref),
 *         -2 on allocation failure. */
int rbt_insert(RBTree *t, uint64_t key,
               int blood_group, double lat, double lon, double eta,
               int consent, int available, int verified, int active,
               int64_t consent_at, int64_t avail_at, int64_t eta_at);

/* Delete the node with the given key.
 * Returns  0 on success,
 *         -1 if key not found. */
int rbt_delete(RBTree *t, uint64_t key);

/* Update availability, consent, and freshness timestamps for a donor.
 * Returns  0 on success,
 *         -1 if key not found. */
int rbt_update_availability(RBTree *t, uint64_t key,
                            int available, int consent, int64_t now);

/* ------------------------------------------------------------------ */
/*  Query                                                             */
/* ------------------------------------------------------------------ */

/* Search for a node by key.  Returns pointer to the node or the tree's
 * sentinel (t->nil) if not found. */
RBNode *rbt_search(const RBTree *t, uint64_t key);

/* Return the node with the smallest / largest key in the subtree rooted
 * at `node`.  Returns t->nil if the subtree is empty. */
RBNode *rbt_minimum(const RBTree *t, RBNode *node);
RBNode *rbt_maximum(const RBTree *t, RBNode *node);

/* Return the number of donors stored in the tree. */
size_t  rbt_size(const RBTree *t);

/* In-order traversal: `visit` is called for every node in ascending key
 * order.  `ctx` is passed through unchanged. */
void rbt_inorder(const RBTree *t,
                 void (*visit)(const RBNode *node, void *ctx), void *ctx);

#ifdef __cplusplus
}
#endif
#endif /* BDF_RBTREE_H */
