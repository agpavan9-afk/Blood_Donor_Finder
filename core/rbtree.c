/*
 * rbtree.c — Red-Black Tree implementation for Donor Registry
 *
 * Full implementation of a Red-Black Tree with insert, delete, search,
 * in-order traversal, and donor-specific update operations.
 *
 * INSERT algorithm (CLRS Chapter 13):
 *   1. Standard BST insert; color the new node RED.
 *   2. Fix violations by walking up toward the root:
 *      - Case A: Uncle is RED  => recolor parent & uncle BLACK, grandparent RED.
 *      - Case B: Uncle is BLACK, node is an inner child => rotate to outer child.
 *      - Case C: Uncle is BLACK, node is an outer child => rotate grandparent,
 *                swap colors of parent and grandparent.
 *   3. Ensure the root is always BLACK.
 *
 * DELETE algorithm (CLRS Chapter 13):
 *   1. Standard BST delete (replace with successor if two children).
 *   2. If the removed node was BLACK, fix "double-black" violations:
 *      - Case 1: Sibling is RED => rotate parent, recolor.
 *      - Case 2: Sibling is BLACK with two BLACK children => recolor sibling RED.
 *      - Case 3: Sibling's far child is BLACK, near child is RED => rotate sibling.
 *      - Case 4: Sibling's far child is RED => rotate parent, recolor, done.
 *
 * All rotations and fix-ups preserve the four Red-Black invariants.
 */
#include "rbtree.h"

#include <stdlib.h>
#include <string.h>

/* ================================================================== */
/*  Internal helpers                                                  */
/* ================================================================== */

/*
 * Left rotation around node x:
 *
 *      x                 y
 *     / \               / \
 *    a   y     =>      x   c
 *       / \           / \
 *      b   c         a   b
 */
static void rotate_left(RBTree *t, RBNode *x) {
    RBNode *y = x->right;
    x->right = y->left;
    if (y->left != t->nil)
        y->left->parent = x;
    y->parent = x->parent;
    if (x->parent == t->nil)
        t->root = y;
    else if (x == x->parent->left)
        x->parent->left = y;
    else
        x->parent->right = y;
    y->left = x;
    x->parent = y;
}

/*
 * Right rotation around node x:
 *
 *        x             y
 *       / \           / \
 *      y   c   =>   a   x
 *     / \               / \
 *    a   b             b   c
 */
static void rotate_right(RBTree *t, RBNode *x) {
    RBNode *y = x->left;
    x->left = y->right;
    if (y->right != t->nil)
        y->right->parent = x;
    y->parent = x->parent;
    if (x->parent == t->nil)
        t->root = y;
    else if (x == x->parent->right)
        x->parent->right = y;
    else
        x->parent->left = y;
    y->right = x;
    x->parent = y;
}

/* Fix Red-Black violations after inserting node z (which is RED). */
static void insert_fixup(RBTree *t, RBNode *z) {
    while (z->parent->color == RB_RED) {
        if (z->parent == z->parent->parent->left) {
            RBNode *uncle = z->parent->parent->right;
            if (uncle->color == RB_RED) {
                /* Case A: Uncle is RED — recolor and move up. */
                z->parent->color = RB_BLACK;
                uncle->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    /* Case B: z is a right (inner) child — left rotate parent. */
                    z = z->parent;
                    rotate_left(t, z);
                }
                /* Case C: z is a left (outer) child — right rotate grandparent. */
                z->parent->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                rotate_right(t, z->parent->parent);
            }
        } else {
            /* Symmetric: parent is a right child of grandparent. */
            RBNode *uncle = z->parent->parent->left;
            if (uncle->color == RB_RED) {
                z->parent->color = RB_BLACK;
                uncle->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->left) {
                    z = z->parent;
                    rotate_right(t, z);
                }
                z->parent->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                rotate_left(t, z->parent->parent);
            }
        }
    }
    t->root->color = RB_BLACK;   /* Invariant 2: root is always BLACK. */
}

/* Replace subtree rooted at u with subtree rooted at v (transplant). */
static void transplant(RBTree *t, RBNode *u, RBNode *v) {
    if (u->parent == t->nil)
        t->root = v;
    else if (u == u->parent->left)
        u->parent->left = v;
    else
        u->parent->right = v;
    v->parent = u->parent;
}

/* Fix double-black violations after deleting a BLACK node.
 * `x` is the node that replaced the deleted node. */
static void delete_fixup(RBTree *t, RBNode *x) {
    while (x != t->root && x->color == RB_BLACK) {
        if (x == x->parent->left) {
            RBNode *w = x->parent->right;   /* sibling */
            if (w->color == RB_RED) {
                /* Case 1: Sibling is RED. */
                w->color = RB_BLACK;
                x->parent->color = RB_RED;
                rotate_left(t, x->parent);
                w = x->parent->right;
            }
            if (w->left->color == RB_BLACK && w->right->color == RB_BLACK) {
                /* Case 2: Sibling and both its children are BLACK. */
                w->color = RB_RED;
                x = x->parent;
            } else {
                if (w->right->color == RB_BLACK) {
                    /* Case 3: Sibling's far child is BLACK, near child is RED. */
                    w->left->color = RB_BLACK;
                    w->color = RB_RED;
                    rotate_right(t, w);
                    w = x->parent->right;
                }
                /* Case 4: Sibling's far child is RED. */
                w->color = x->parent->color;
                x->parent->color = RB_BLACK;
                w->right->color = RB_BLACK;
                rotate_left(t, x->parent);
                x = t->root;   /* terminates the loop */
            }
        } else {
            /* Symmetric: x is a right child. */
            RBNode *w = x->parent->left;
            if (w->color == RB_RED) {
                w->color = RB_BLACK;
                x->parent->color = RB_RED;
                rotate_right(t, x->parent);
                w = x->parent->left;
            }
            if (w->right->color == RB_BLACK && w->left->color == RB_BLACK) {
                w->color = RB_RED;
                x = x->parent;
            } else {
                if (w->left->color == RB_BLACK) {
                    w->right->color = RB_BLACK;
                    w->color = RB_RED;
                    rotate_left(t, w);
                    w = x->parent->left;
                }
                w->color = x->parent->color;
                x->parent->color = RB_BLACK;
                w->left->color = RB_BLACK;
                rotate_right(t, x->parent);
                x = t->root;
            }
        }
    }
    x->color = RB_BLACK;
}

/* Recursively free all nodes in the subtree (post-order). */
static void free_subtree(RBTree *t, RBNode *node) {
    if (node == t->nil) return;
    free_subtree(t, node->left);
    free_subtree(t, node->right);
    free(node);
}

/* Recursive in-order helper. */
static void inorder_walk(const RBTree *t, RBNode *node,
                         void (*visit)(const RBNode *, void *), void *ctx) {
    if (node == t->nil) return;
    inorder_walk(t, node->left, visit, ctx);
    visit(node, ctx);
    inorder_walk(t, node->right, visit, ctx);
}

/* ================================================================== */
/*  Public API                                                        */
/* ================================================================== */

RBTree *rbt_create(void) {
    RBTree *t = (RBTree *)malloc(sizeof(RBTree));
    if (!t) return NULL;
    /* Allocate the sentinel NIL node — always BLACK, shared by all leaves. */
    t->nil = (RBNode *)calloc(1, sizeof(RBNode));
    if (!t->nil) { free(t); return NULL; }
    t->nil->color = RB_BLACK;
    t->nil->left = t->nil->right = t->nil->parent = t->nil;
    t->root = t->nil;
    t->size = 0;
    return t;
}

void rbt_destroy(RBTree *t) {
    if (!t) return;
    free_subtree(t, t->root);
    free(t->nil);
    free(t);
}

int rbt_insert(RBTree *t, uint64_t key,
               int blood_group, double lat, double lon, double eta,
               int consent, int available, int verified, int active,
               int64_t consent_at, int64_t avail_at, int64_t eta_at) {
    if (!t) return -2;

    /* Standard BST walk to find the insertion point. */
    RBNode *parent = t->nil;
    RBNode *curr = t->root;
    while (curr != t->nil) {
        parent = curr;
        if (key < curr->key)
            curr = curr->left;
        else if (key > curr->key)
            curr = curr->right;
        else
            return -1;   /* duplicate key */
    }

    /* Allocate and initialise the new node. */
    RBNode *z = (RBNode *)malloc(sizeof(RBNode));
    if (!z) return -2;
    z->key                    = key;
    z->blood_group            = blood_group;
    z->latitude               = lat;
    z->longitude              = lon;
    z->eta_minutes            = eta;
    z->consent_current        = consent;
    z->available              = available;
    z->blood_group_verified   = verified;
    z->profile_active         = active;
    z->consent_checked_at     = consent_at;
    z->availability_checked_at = avail_at;
    z->eta_checked_at         = eta_at;
    z->color  = RB_RED;       /* New nodes always start RED. */
    z->left   = t->nil;
    z->right  = t->nil;
    z->parent = parent;

    if (parent == t->nil)
        t->root = z;
    else if (key < parent->key)
        parent->left = z;
    else
        parent->right = z;

    t->size++;
    insert_fixup(t, z);
    return 0;
}

int rbt_delete(RBTree *t, uint64_t key) {
    if (!t) return -1;

    /* Find the node to delete. */
    RBNode *z = t->root;
    while (z != t->nil) {
        if (key < z->key)
            z = z->left;
        else if (key > z->key)
            z = z->right;
        else
            break;
    }
    if (z == t->nil) return -1;   /* not found */

    RBNode *y = z;
    RBColor y_orig_color = y->color;
    RBNode *x;

    if (z->left == t->nil) {
        x = z->right;
        transplant(t, z, z->right);
    } else if (z->right == t->nil) {
        x = z->left;
        transplant(t, z, z->left);
    } else {
        /* Two children: replace z with its in-order successor. */
        y = z->right;
        while (y->left != t->nil) y = y->left;
        y_orig_color = y->color;
        x = y->right;
        if (y->parent == z) {
            x->parent = y;   /* needed when x is the sentinel */
        } else {
            transplant(t, y, y->right);
            y->right = z->right;
            y->right->parent = y;
        }
        transplant(t, z, y);
        y->left = z->left;
        y->left->parent = y;
        y->color = z->color;
    }

    free(z);
    t->size--;

    if (y_orig_color == RB_BLACK)
        delete_fixup(t, x);

    return 0;
}

RBNode *rbt_search(const RBTree *t, uint64_t key) {
    if (!t) return NULL;
    RBNode *curr = t->root;
    while (curr != t->nil) {
        if (key < curr->key)
            curr = curr->left;
        else if (key > curr->key)
            curr = curr->right;
        else
            return curr;
    }
    return t->nil;
}

RBNode *rbt_minimum(const RBTree *t, RBNode *node) {
    if (!t || node == t->nil) return t ? t->nil : NULL;
    while (node->left != t->nil) node = node->left;
    return node;
}

RBNode *rbt_maximum(const RBTree *t, RBNode *node) {
    if (!t || node == t->nil) return t ? t->nil : NULL;
    while (node->right != t->nil) node = node->right;
    return node;
}

size_t rbt_size(const RBTree *t) {
    return t ? t->size : 0;
}

int rbt_update_availability(RBTree *t, uint64_t key,
                            int available, int consent, int64_t now) {
    RBNode *node = rbt_search(t, key);
    if (!node || node == t->nil) return -1;
    node->available              = available;
    node->consent_current        = consent;
    node->availability_checked_at = now;
    node->consent_checked_at     = now;
    return 0;
}

void rbt_inorder(const RBTree *t,
                 void (*visit)(const RBNode *, void *), void *ctx) {
    if (!t || !visit) return;
    inorder_walk(t, t->root, visit, ctx);
}
