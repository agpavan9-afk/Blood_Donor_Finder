/*
 * skiplist.c — Skip List implementation for blood-group indexed lookup
 *
 * Implements a probabilistic skip list sorted by (blood_group, donor_ref).
 * The structure allows O(log n) expected time for insert, delete, and search,
 * and efficient range queries across blood group intervals.
 *
 * Level generation uses a simple coin-flip approach: each new node starts
 * at level 1 and is promoted with probability SL_PROBABILITY (0.5) up to
 * SL_MAX_LEVEL.  This yields O(log n) expected height.
 *
 * INSERT:
 *   1. Walk from the highest level down, recording the predecessor at each
 *      level in an update[] array.
 *   2. Generate a random level for the new node.
 *   3. Link the new node's forward pointers into the list at each level.
 *
 * DELETE:
 *   1. Walk from the highest level down, recording predecessors.
 *   2. If the target node is found, unlink it from each level.
 *   3. Free the node and shrink the skip list level if needed.
 *
 * RANGE QUERY:
 *   1. Walk to the first node with blood_group >= lo.
 *   2. Scan level 0 until blood_group > hi.
 */
#include "skiplist.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================== */
/*  Internal helpers                                                  */
/* ================================================================== */

/* Compare two (blood_group, donor_ref) composite keys.
 * Returns negative if a < b, zero if equal, positive if a > b. */
static int sl_compare(int bg_a, uint64_t ref_a, int bg_b, uint64_t ref_b) {
    if (bg_a != bg_b) return bg_a < bg_b ? -1 : 1;
    if (ref_a != ref_b) return ref_a < ref_b ? -1 : 1;
    return 0;
}

/* Random number generator state (simple xorshift32). */
static unsigned int sl_rng_state = 0;

static void sl_seed_rng(void) {
    if (sl_rng_state == 0)
        sl_rng_state = (unsigned int)time(NULL) ^ 0xDEADBEEF;
}

static unsigned int sl_rand(void) {
    unsigned int x = sl_rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    sl_rng_state = x;
    return x;
}

/* Generate a random level for a new node.
 * Each level is included with probability SL_PROBABILITY (0.5).
 * Result is in [1, SL_MAX_LEVEL]. */
static int random_level(void) {
    int lvl = 1;
    while (lvl < SL_MAX_LEVEL && (sl_rand() & 1))
        lvl++;
    return lvl;
}

/* Allocate a skip list node with the given level (number of forward ptrs). */
static SLNode *sl_new_node(int level, int blood_group,
                           uint64_t donor_ref, double score) {
    SLNode *node = (SLNode *)malloc(sizeof(SLNode));
    if (!node) return NULL;
    node->forward = (SLNode **)calloc((size_t)level, sizeof(SLNode *));
    if (!node->forward) { free(node); return NULL; }
    node->blood_group = blood_group;
    node->donor_ref   = donor_ref;
    node->score       = score;
    node->level       = level;
    return node;
}

static void sl_free_node(SLNode *node) {
    if (!node) return;
    free(node->forward);
    free(node);
}

/* ================================================================== */
/*  Public API                                                        */
/* ================================================================== */

SkipList *sl_create(void) {
    sl_seed_rng();
    SkipList *sl = (SkipList *)malloc(sizeof(SkipList));
    if (!sl) return NULL;
    /* The header is a dummy node with the maximum level. */
    sl->header = sl_new_node(SL_MAX_LEVEL, -1, 0, 0.0);
    if (!sl->header) { free(sl); return NULL; }
    sl->level = 1;
    sl->size  = 0;
    return sl;
}

void sl_destroy(SkipList *sl) {
    if (!sl) return;
    SLNode *curr = sl->header->forward[0];
    while (curr) {
        SLNode *next = curr->forward[0];
        sl_free_node(curr);
        curr = next;
    }
    sl_free_node(sl->header);
    free(sl);
}

int sl_insert(SkipList *sl, int blood_group, uint64_t donor_ref, double score) {
    if (!sl || blood_group < 0 || blood_group > 7) return -2;

    /* update[i] will point to the last node at level i that is < new node. */
    SLNode *update[SL_MAX_LEVEL];
    SLNode *curr = sl->header;

    /* Walk from the highest level down to level 0. */
    for (int i = sl->level - 1; i >= 0; i--) {
        while (curr->forward[i] &&
               sl_compare(curr->forward[i]->blood_group,
                          curr->forward[i]->donor_ref,
                          blood_group, donor_ref) < 0) {
            curr = curr->forward[i];
        }
        update[i] = curr;
    }

    /* Check for duplicate. */
    curr = curr->forward[0];
    if (curr &&
        sl_compare(curr->blood_group, curr->donor_ref,
                   blood_group, donor_ref) == 0) {
        return -1;   /* already exists */
    }

    int new_level = random_level();
    if (new_level > sl->level) {
        for (int i = sl->level; i < new_level; i++)
            update[i] = sl->header;
        sl->level = new_level;
    }

    SLNode *new_node = sl_new_node(new_level, blood_group, donor_ref, score);
    if (!new_node) return -2;

    /* Link the new node into each level. */
    for (int i = 0; i < new_level; i++) {
        new_node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = new_node;
    }

    sl->size++;
    return 0;
}

int sl_delete(SkipList *sl, int blood_group, uint64_t donor_ref) {
    if (!sl) return -1;

    SLNode *update[SL_MAX_LEVEL];
    SLNode *curr = sl->header;

    for (int i = sl->level - 1; i >= 0; i--) {
        while (curr->forward[i] &&
               sl_compare(curr->forward[i]->blood_group,
                          curr->forward[i]->donor_ref,
                          blood_group, donor_ref) < 0) {
            curr = curr->forward[i];
        }
        update[i] = curr;
    }

    curr = curr->forward[0];
    if (!curr ||
        sl_compare(curr->blood_group, curr->donor_ref,
                   blood_group, donor_ref) != 0) {
        return -1;   /* not found */
    }

    /* Unlink the node from each level. */
    for (int i = 0; i < sl->level; i++) {
        if (update[i]->forward[i] != curr) break;
        update[i]->forward[i] = curr->forward[i];
    }

    sl_free_node(curr);
    sl->size--;

    /* Shrink the list level if the top levels are now empty. */
    while (sl->level > 1 && sl->header->forward[sl->level - 1] == NULL)
        sl->level--;

    return 0;
}

size_t sl_find_by_group(const SkipList *sl, int blood_group,
                        uint64_t *out_refs, size_t capacity) {
    if (!sl || !out_refs || capacity == 0) return 0;

    /* Walk to the first node with the target blood group. */
    SLNode *curr = sl->header;
    for (int i = sl->level - 1; i >= 0; i--) {
        while (curr->forward[i] &&
               curr->forward[i]->blood_group < blood_group) {
            curr = curr->forward[i];
        }
    }

    /* Scan level 0 for all matching nodes. */
    curr = curr->forward[0];
    size_t count = 0;
    while (curr && curr->blood_group == blood_group && count < capacity) {
        out_refs[count++] = curr->donor_ref;
        curr = curr->forward[0];
    }
    return count;
}

size_t sl_range_query(const SkipList *sl, int lo, int hi,
                      uint64_t *out_refs, size_t capacity) {
    if (!sl || !out_refs || capacity == 0 || lo > hi) return 0;

    /* Walk to the first node with blood_group >= lo. */
    SLNode *curr = sl->header;
    for (int i = sl->level - 1; i >= 0; i--) {
        while (curr->forward[i] &&
               curr->forward[i]->blood_group < lo) {
            curr = curr->forward[i];
        }
    }

    /* Scan level 0 while blood_group <= hi. */
    curr = curr->forward[0];
    size_t count = 0;
    while (curr && curr->blood_group <= hi && count < capacity) {
        out_refs[count++] = curr->donor_ref;
        curr = curr->forward[0];
    }
    return count;
}

size_t sl_size(const SkipList *sl) {
    return sl ? sl->size : 0;
}
