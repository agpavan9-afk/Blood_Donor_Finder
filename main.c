/*
 * main.c — BloodLink: Smart Blood Donor Matching and Emergency Allocation Network
 *
 * A comprehensive demonstration of advanced data structures applied to a
 * real-world blood donor matching problem.  This standalone program
 * showcases six data structures:
 *
 *   1. Red-Black Tree   — O(log n) donor registry (insert/delete/search)
 *   2. Skip List        — O(log n) blood-group indexed lookup & range query
 *   3. KD-Tree          — O(log n + k) geographic spatial search
 *   4. Binary Max-Heap  — top-k candidate ranking
 *   5. Hash Table       — O(1) average donor ID lookup
 *   6. Dijkstra's Algo  — shortest-path road travel time estimation
 *
 * Compile:
 *   gcc -std=c11 -Wall -Wextra -O2 main.c core/rbtree.c core/skiplist.c -lm -o blood_donor_finder
 *
 * Run:
 *   ./blood_donor_finder       (Linux/macOS)
 *   blood_donor_finder.exe     (Windows)
 */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

#include "core/rbtree.h"
#include "core/skiplist.h"

/* MinGW's scanf does not handle %llu; use SCNu64 from inttypes.h. */

/* ================================================================== */
/*  Constants                                                         */
/* ================================================================== */

#define MAX_DONORS    200
#define MAX_NODES     12
#define HASH_SIZE     503
#define MAX_MATCHES   MAX_DONORS
#define MAX_GRAPH_NODES 12
#define CSV_FILE      "donors.csv"

static const char *BLOOD_NAMES[] = {
    "O-", "O+", "A-", "A+", "B-", "B+", "AB-", "AB+"
};

/* ================================================================== */
/*  Type definitions                                                  */
/* ================================================================== */

typedef struct {
    uint64_t id;
    char     name[64];
    char     blood[6];
    int      blood_code;      /* 0=O-, 1=O+, ... 7=AB+ */
    double   lat, lon;
    int      node;            /* graph node index */
    int      available;
    int      consent;
    char     city[32];
    char     area[48];
    int      total_donations;
} Donor;

typedef struct KDNode {
    int donor_index;
    int axis;
    struct KDNode *left, *right;
} KDNode;

typedef struct {
    int    donor_index;
    double score, distance_km, travel_min;
} Match;

typedef struct {
    Match items[MAX_MATCHES];
    int   size;
} PriorityQueue;

typedef struct {
    int used;
    uint64_t id;
    int donor_index;
} HashEntry;

/* ================================================================== */
/*  Global state                                                      */
/* ================================================================== */

static Donor     donors[MAX_DONORS];
static int       donor_count = 0;
static HashEntry id_table[HASH_SIZE];
static RBTree   *rb_registry = NULL;
static SkipList *sl_index    = NULL;

/* Hospital / location network */
static const char *locations[MAX_GRAPH_NODES] = {
    "Victoria Hospital, Bengaluru",
    "NIMHANS, Bengaluru",
    "KIMS Hospital, Bengaluru",
    "Manipal Hospital, Bengaluru",
    "Apollo Hospital, Bengaluru",
    "Fortis Hospital, Bengaluru",
    "Narayana Health, Bengaluru",
    "City Blood Bank, Bengaluru",
    "AIIMS, New Delhi",
    "KEM Hospital, Mumbai",
    "Safdarjung Hospital, Delhi",
    "CMC Vellore"
};

static const double loc_lats[MAX_GRAPH_NODES] = {
    12.9603, 12.9410, 12.9260, 12.9494, 12.9633,
    12.9289, 12.8408, 12.9716, 28.5665, 18.9980, 28.5680, 12.9230
};
static const double loc_lons[MAX_GRAPH_NODES] = {
    77.5736, 77.5955, 77.5600, 77.6490, 77.6407,
    77.6279, 77.6686, 77.5946, 77.2101, 72.8419, 77.2060, 79.1326
};

/* Road travel times (minutes); -1 = no direct road */
static const double roads[MAX_GRAPH_NODES][MAX_GRAPH_NODES] = {
    /*  Vic   NIM   KIMS  Manip Apollo Fort  Nara  BlBk  AIIMS KEM   Safd  CMC  */
    {   0,    15,   12,   25,   28,   22,   35,   8,   -1,   -1,   -1,   -1  }, /* Victoria */
    {   15,   0,    18,   22,   20,   15,   30,   14,  -1,   -1,   -1,   -1  }, /* NIMHANS */
    {   12,   18,   0,    30,   32,   25,   28,   10,  -1,   -1,   -1,   -1  }, /* KIMS */
    {   25,   22,   30,   0,    10,   15,   20,   24,  -1,   -1,   -1,   -1  }, /* Manipal */
    {   28,   20,   32,   10,   0,    12,   22,   26,  -1,   -1,   -1,   -1  }, /* Apollo */
    {   22,   15,   25,   15,   12,   0,    18,   20,  -1,   -1,   -1,   -1  }, /* Fortis */
    {   35,   30,   28,   20,   22,   18,   0,    32,  -1,   -1,   -1,   -1  }, /* Narayana */
    {   8,    14,   10,   24,   26,   20,   32,   0,   -1,   -1,   -1,   -1  }, /* Blood Bank */
    {   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  0,    -1,   12,   -1  }, /* AIIMS */
    {   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  -1,   0,    -1,   -1  }, /* KEM */
    {   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  12,   -1,   0,    -1  }, /* Safdarjung */
    {   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  -1,   -1,   -1,   0   }, /* CMC */
};

/* ================================================================== */
/*  Utility functions                                                 */
/* ================================================================== */

static int blood_code(const char *s) {
    for (int i = 0; i < 8; i++)
        if (strcmp(s, BLOOD_NAMES[i]) == 0) return i;
    return -1;
}

static void clear_input(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

static void press_enter(void) {
    printf("\n  Press ENTER to continue...");
    clear_input();
}

/* ================================================================== */
/*  Hash Table — O(1) average donor ID lookup                         */
/* ================================================================== */

static int hash_id(uint64_t id) {
    return (int)((id * 2654435761u) % HASH_SIZE);
}

static void hash_insert(uint64_t id, int index) {
    int slot = hash_id(id);
    while (id_table[slot].used)
        slot = (slot + 1) % HASH_SIZE;
    id_table[slot].used = 1;
    id_table[slot].id = id;
    id_table[slot].donor_index = index;
}

static int hash_find(uint64_t id) {
    int slot = hash_id(id), start = slot;
    while (id_table[slot].used) {
        if (id_table[slot].id == id) return id_table[slot].donor_index;
        slot = (slot + 1) % HASH_SIZE;
        if (slot == start) break;
    }
    return -1;
}

/* ================================================================== */
/*  KD-Tree — 2D spatial search for nearby donors                     */
/* ================================================================== */

static double kd_coord(int index, int axis) {
    return axis ? donors[index].lon : donors[index].lat;
}

static int kd_sort_axis;
static int kd_compare(const void *a, const void *b) {
    double x = kd_coord(*(const int *)a, kd_sort_axis);
    double y = kd_coord(*(const int *)b, kd_sort_axis);
    return (x > y) - (x < y);
}

static KDNode *build_kd(int *indices, int count, int depth) {
    if (!count) return NULL;
    int axis = depth % 2;
    kd_sort_axis = axis;
    qsort(indices, (size_t)count, sizeof(int), kd_compare);
    int mid = count / 2;
    KDNode *node = (KDNode *)malloc(sizeof(*node));
    if (!node) return NULL;
    node->donor_index = indices[mid];
    node->axis = axis;
    node->left  = build_kd(indices, mid, depth + 1);
    node->right = build_kd(indices + mid + 1, count - mid - 1, depth + 1);
    return node;
}

static void free_kd(KDNode *node) {
    if (!node) return;
    free_kd(node->left);
    free_kd(node->right);
    free(node);
}

/* ================================================================== */
/*  Haversine distance                                                */
/* ================================================================== */

static double deg2rad(double d) { return d * 0.017453292519943295; }

static double distance_km(double lat1, double lon1, double lat2, double lon2) {
    double p1 = deg2rad(lat1), p2 = deg2rad(lat2);
    double dp = p2 - p1, dl = deg2rad(lon2 - lon1);
    double a = sin(dp / 2.0) * sin(dp / 2.0) +
               cos(p1) * cos(p2) * sin(dl / 2.0) * sin(dl / 2.0);
    if (a > 1.0) a = 1.0;
    return 2.0 * 6371.0088 * asin(sqrt(a));
}

/* ================================================================== */
/*  Blood compatibility — ABO/Rh red-cell preliminary filter          */
/* ================================================================== */

static int abo_group(int code) {
    if (code <= 1) return 0;   /* O */
    if (code <= 3) return 1;   /* A */
    if (code <= 5) return 2;   /* B */
    return 3;                  /* AB */
}

static int compatible(int donor_code, int recipient_code) {
    int d = abo_group(donor_code), r = abo_group(recipient_code);
    int d_neg = (donor_code % 2) == 0;
    int r_neg = (recipient_code % 2) == 0;
    int abo_ok = (r == 3) || (d == 0) || (d == r);
    return abo_ok && (!r_neg || d_neg);
}

/* ================================================================== */
/*  Dijkstra's Algorithm — shortest path on road graph                */
/* ================================================================== */

static double road_time(int from, int to) {
    if (from < 0 || from >= MAX_GRAPH_NODES ||
        to < 0 || to >= MAX_GRAPH_NODES) return 999.0;
    double dist[MAX_GRAPH_NODES];
    int    done[MAX_GRAPH_NODES];
    memset(done, 0, sizeof(done));
    for (int i = 0; i < MAX_GRAPH_NODES; i++) dist[i] = 1e9;
    dist[from] = 0;

    for (int k = 0; k < MAX_GRAPH_NODES; k++) {
        int u = -1;
        for (int i = 0; i < MAX_GRAPH_NODES; i++)
            if (!done[i] && (u < 0 || dist[i] < dist[u])) u = i;
        if (u < 0 || dist[u] >= 1e9) break;
        done[u] = 1;
        for (int v = 0; v < MAX_GRAPH_NODES; v++)
            if (roads[u][v] >= 0 && dist[v] > dist[u] + roads[u][v])
                dist[v] = dist[u] + roads[u][v];
    }
    return dist[to] >= 1e9 ? 999.0 : dist[to];
}

/* ================================================================== */
/*  Priority Queue (Binary Max-Heap) — top-k candidate ranking        */
/* ================================================================== */

static int better(Match a, Match b) { return a.score > b.score; }

static void heap_push(PriorityQueue *q, Match m) {
    int i = q->size++;
    while (i > 0) {
        int p = (i - 1) / 2;
        if (better(q->items[p], m)) break;
        q->items[i] = q->items[p];
        i = p;
    }
    q->items[i] = m;
}

static Match heap_pop(PriorityQueue *q) {
    Match out = q->items[0], tail = q->items[--q->size];
    int i = 0;
    while (2 * i + 1 < q->size) {
        int c = 2 * i + 1;
        if (c + 1 < q->size && better(q->items[c + 1], q->items[c])) c++;
        if (better(tail, q->items[c])) break;
        q->items[i] = q->items[c];
        i = c;
    }
    if (q->size) q->items[i] = tail;
    return out;
}

/* ================================================================== */
/*  KD-Tree spatial search + scoring                                  */
/* ================================================================== */

static void collect(KDNode *node, int hospital, int blood, int severity,
                    double lat, double lon, PriorityQueue *q) {
    if (!node) return;
    Donor *d = &donors[node->donor_index];
    double km = distance_km(lat, lon, d->lat, d->lon);

    if (d->available && d->consent &&
        compatible(d->blood_code, blood) && km <= 75.0) {
        double mins = road_time(d->node, hospital);
        double score = severity * 10.0 +
                       fmax(0, 30.0 - km * 0.4) +
                       20.0 +
                       fmax(0, 20.0 - mins * 0.2) +
                       d->total_donations * 2.0;
        heap_push(q, (Match){node->donor_index, score, km, mins});
    }

    double delta;
    if (node->axis == 0)
        delta = (lat - d->lat) * 111.0;
    else
        delta = (lon - d->lon) * 111.0 * cos(deg2rad(lat));

    if (delta <= 75.0) collect(node->left, hospital, blood, severity, lat, lon, q);
    if (delta >= -75.0) collect(node->right, hospital, blood, severity, lat, lon, q);
}

/* ================================================================== */
/*  Seed data — 24 realistic Indian donors                            */
/* ================================================================== */

static void seed_data(void) {
    struct { uint64_t id; const char *name; const char *blood; double lat, lon;
             int node; const char *city; const char *area; int donations; } S[] = {
        {1001, "Arjun Sharma",    "O-",  12.9716, 77.5946, 0, "Bengaluru", "Koramangala", 5},
        {1002, "Priya Nair",      "O+",  12.9820, 77.6010, 1, "Bengaluru", "Indiranagar", 3},
        {1003, "Rahul Verma",     "A+",  12.9560, 77.5840, 2, "Bengaluru", "JP Nagar", 7},
        {1004, "Sneha Patel",     "B-",  12.9980, 77.6100, 3, "Bengaluru", "Whitefield", 2},
        {1005, "Kiran Kumar",     "AB+", 12.9430, 77.6100, 4, "Bengaluru", "Electronic City", 1},
        {1006, "Meera Reddy",     "A-",  12.9900, 77.5700, 5, "Bengaluru", "Yeshwantpur", 4},
        {1007, "Suresh Iyer",     "O+",  13.0100, 77.5850, 6, "Bengaluru", "Hebbal", 6},
        {1008, "Divya Krishnan",  "B+",  12.9650, 77.6200, 3, "Bengaluru", "Marathahalli", 2},
        {1009, "Amit Singh",      "O-",  12.9400, 77.5600, 2, "Bengaluru", "Jayanagar", 8},
        {1010, "Pooja Gupta",     "AB-", 12.9700, 77.6300, 4, "Bengaluru", "Sarjapur", 1},
        {1011, "Vikram Nair",     "A+",  13.0200, 77.5600, 6, "Bengaluru", "Yelahanka", 3},
        {1012, "Ananya Bose",     "B+",  12.9300, 77.6000, 5, "Bengaluru", "BTM Layout", 5},
        {1013, "Ravi Shankar",    "O+",  12.9800, 77.5500, 7, "Bengaluru", "Rajajinagar", 4},
        {1014, "Lakshmi Devi",    "A-",  12.9100, 77.6100, 1, "Bengaluru", "Bommanahalli", 2},
        {1015, "Rohit Kapoor",    "B-",  12.9950, 77.6400, 4, "Bengaluru", "Varthur", 1},
        {1016, "Swati Mishra",    "O-",  12.9550, 77.5750, 0, "Bengaluru", "Wilson Garden", 9},
        {1017, "Deepak Joshi",    "AB+", 12.9850, 77.6050, 3, "Bengaluru", "HSR Layout", 3},
        {1018, "Kavitha Rao",     "B+",  12.9200, 77.5900, 5, "Bengaluru", "Banashankari", 2},
        {1019, "Ajay Menon",      "A+",  13.0050, 77.5950, 6, "Bengaluru", "Devanahalli", 1},
        {1020, "Suma Prakash",    "O+",  12.9750, 77.5800, 7, "Bengaluru", "Shivajinagar", 6},
        {1021, "Rajesh Pandey",   "O+",  28.5700, 77.2100, 8, "Delhi", "Safdarjung Encl.", 4},
        {1022, "Neha Agarwal",    "A+",  28.5620, 77.2050, 10, "Delhi", "Green Park", 3},
        {1023, "Farhan Khan",     "B+",  19.0000, 72.8400, 9, "Mumbai", "Parel", 5},
        {1024, "Sita Raman",      "O-",  12.9230, 79.1326, 11, "Vellore", "CMC Campus", 7},
    };
    int n = (int)(sizeof(S) / sizeof(S[0]));

    rb_registry = rbt_create();
    sl_index    = sl_create();

    for (int i = 0; i < n; i++) {
        Donor *d = &donors[donor_count];
        d->id              = S[i].id;
        strncpy(d->name, S[i].name, sizeof(d->name) - 1);
        strncpy(d->blood, S[i].blood, sizeof(d->blood) - 1);
        d->blood_code      = blood_code(S[i].blood);
        d->lat             = S[i].lat;
        d->lon             = S[i].lon;
        d->node            = S[i].node;
        d->available       = 1;
        d->consent         = 1;
        strncpy(d->city, S[i].city, sizeof(d->city) - 1);
        strncpy(d->area, S[i].area, sizeof(d->area) - 1);
        d->total_donations = S[i].donations;

        hash_insert(d->id, donor_count);

        /* Insert into Red-Black Tree */
        int64_t now = (int64_t)time(NULL);
        rbt_insert(rb_registry, d->id, d->blood_code, d->lat, d->lon,
                   15.0, 1, 1, 1, 1, now, now, now);

        /* Insert into Skip List */
        sl_insert(sl_index, d->blood_code, d->id, 0.0);

        donor_count++;
    }
}

/* ================================================================== */
/*  CSV file persistence                                              */
/* ================================================================== */

static void save_donors(void) {
    FILE *f = fopen(CSV_FILE, "w");
    if (!f) { printf("  Error: Could not open %s for writing.\n", CSV_FILE); return; }
    fprintf(f, "id,name,blood,lat,lon,node,available,consent,city,area,donations\n");
    for (int i = 0; i < donor_count; i++) {
        Donor *d = &donors[i];
        fprintf(f, "%llu,%s,%s,%.6f,%.6f,%d,%d,%d,%s,%s,%d\n",
                (unsigned long long)d->id, d->name, d->blood,
                d->lat, d->lon, d->node, d->available, d->consent,
                d->city, d->area, d->total_donations);
    }
    fclose(f);
    printf("  Saved %d donors to %s\n", donor_count, CSV_FILE);
}

/* ================================================================== */
/*  Menu 1: List all donors                                           */
/* ================================================================== */

static void list_donors(void) {
    printf("\n");
    printf("  ============================================================"
           "==========================\n");
    printf("  %-6s %-20s %-5s %-20s %-12s %-8s %s\n",
           "ID", "Name", "Blood", "Area", "City", "Status", "Donations");
    printf("  ============================================================"
           "==========================\n");
    for (int i = 0; i < donor_count; i++) {
        Donor *d = &donors[i];
        printf("  %-6llu %-20s %-5s %-20s %-12s %-8s %d\n",
               (unsigned long long)d->id, d->name, d->blood, d->area,
               d->city, (d->available && d->consent) ? "Active" : "Inactive",
               d->total_donations);
    }
    printf("  ============================================================"
           "==========================\n");
    printf("  Total: %d donors\n", donor_count);
}

/* ================================================================== */
/*  Menu 2: Create emergency request (KD-Tree + Heap + Dijkstra)      */
/* ================================================================== */

static void request_blood(void) {
    char blood_input[8];
    int node_idx, severity;

    printf("\n  === EMERGENCY BLOOD REQUEST ===\n\n");
    printf("  Available blood groups: O-  O+  A-  A+  B-  B+  AB-  AB+\n");
    printf("  Enter blood group needed: ");
    if (scanf("%7s", blood_input) != 1) return;
    for (int i = 0; blood_input[i]; i++)
        blood_input[i] = (char)toupper((unsigned char)blood_input[i]);
    int code = blood_code(blood_input);
    if (code < 0) { printf("  Invalid blood group.\n"); return; }

    printf("\n  Hospital locations:\n");
    for (int i = 0; i < MAX_GRAPH_NODES; i++)
        printf("    %2d. %s\n", i + 1, locations[i]);
    printf("  Select hospital (1-%d): ", MAX_GRAPH_NODES);
    if (scanf("%d", &node_idx) != 1 || node_idx < 1 || node_idx > MAX_GRAPH_NODES) {
        printf("  Invalid selection.\n"); return;
    }
    node_idx--;

    printf("  Emergency severity (1=Routine, 2=Soon, 3=Urgent, 4=Very Urgent, 5=Critical): ");
    if (scanf("%d", &severity) != 1 || severity < 1 || severity > 5) {
        printf("  Severity must be 1-5.\n"); return;
    }

    /* Build KD-tree and search */
    int indices[MAX_DONORS];
    for (int i = 0; i < donor_count; i++) indices[i] = i;
    KDNode *tree = build_kd(indices, donor_count, 0);
    PriorityQueue q = {{}, 0};
    collect(tree, node_idx, code, severity,
            loc_lats[node_idx], loc_lons[node_idx], &q);
    free_kd(tree);

    printf("\n  === RESULTS for %s at %s (Severity: %d/5) ===\n\n",
           blood_input, locations[node_idx], severity);

    if (!q.size) {
        printf("  No compatible donors found within 75 km.\n");
        return;
    }

    printf("  %-5s %-20s %-6s %-12s %-10s %-10s %s\n",
           "Rank", "Donor", "Group", "Area", "Distance", "ETA", "Score");
    printf("  -----------------------------------------------------------"
           "------------------\n");
    int shown = 0;
    while (q.size && shown < 10) {
        Match m = heap_pop(&q);
        Donor d = donors[m.donor_index];
        shown++;
        printf("  %-5d %-20s %-6s %-12s %7.1f km %7.0f min  %6.1f\n",
               shown, d.name, d.blood, d.area, m.distance_km,
               m.travel_min, m.score);
    }
    printf("\n  Showing top %d of all eligible donors.\n", shown);
    printf("  NOTE: This is a prototype ranking. Do NOT use for clinical decisions.\n");
}

/* ================================================================== */
/*  Menu 3: Register new donor                                        */
/* ================================================================== */

static void register_donor(void) {
    if (donor_count >= MAX_DONORS) {
        printf("  Registry full (max %d donors).\n", MAX_DONORS);
        return;
    }
    Donor *d = &donors[donor_count];
    char blood_input[8];

    printf("\n  === REGISTER NEW DONOR ===\n\n");
    printf("  Donor ID (numeric): ");
    uint64_t id;
    if (scanf("%llu", (unsigned long long *)&id) != 1) return;
    if (hash_find(id) >= 0) {
        printf("  Error: Donor ID %llu already exists.\n", (unsigned long long)id);
        return;
    }
    clear_input();

    printf("  Full name: ");
    if (!fgets(d->name, sizeof(d->name), stdin)) return;
    d->name[strcspn(d->name, "\n")] = 0;

    printf("  Blood group (O-/O+/A-/A+/B-/B+/AB-/AB+): ");
    if (scanf("%7s", blood_input) != 1) return;
    for (int i = 0; blood_input[i]; i++)
        blood_input[i] = (char)toupper((unsigned char)blood_input[i]);
    int bc = blood_code(blood_input);
    if (bc < 0) { printf("  Invalid blood group.\n"); return; }

    printf("  Latitude: ");
    if (scanf("%lf", &d->lat) != 1) return;
    printf("  Longitude: ");
    if (scanf("%lf", &d->lon) != 1) return;
    clear_input();

    printf("  City: ");
    if (!fgets(d->city, sizeof(d->city), stdin)) return;
    d->city[strcspn(d->city, "\n")] = 0;

    printf("  Area/neighbourhood: ");
    if (!fgets(d->area, sizeof(d->area), stdin)) return;
    d->area[strcspn(d->area, "\n")] = 0;

    d->id              = id;
    strncpy(d->blood, blood_input, sizeof(d->blood) - 1);
    d->blood_code      = bc;
    d->node            = 7;  /* default: City Blood Bank */
    d->available       = 1;
    d->consent         = 1;
    d->total_donations = 0;

    hash_insert(d->id, donor_count);
    int64_t now = (int64_t)time(NULL);
    rbt_insert(rb_registry, d->id, d->blood_code, d->lat, d->lon,
               20.0, 1, 1, 1, 1, now, now, now);
    sl_insert(sl_index, d->blood_code, d->id, 0.0);

    donor_count++;
    printf("\n  Donor %s (ID: %llu, %s) registered successfully!\n",
           d->name, (unsigned long long)id, blood_input);
    printf("  Red-Black Tree size: %zu  |  Skip List size: %zu\n",
           rbt_size(rb_registry), sl_size(sl_index));
}

/* ================================================================== */
/*  Menu 4: Toggle donor availability                                 */
/* ================================================================== */

static void toggle_availability(void) {
    uint64_t id;
    printf("\n  Donor ID: ");
    if (scanf("%llu", (unsigned long long *)&id) != 1) return;
    int idx = hash_find(id);
    if (idx < 0) { printf("  Donor not found.\n"); return; }
    donors[idx].available = !donors[idx].available;
    int64_t now = (int64_t)time(NULL);
    rbt_update_availability(rb_registry, id, donors[idx].available,
                            donors[idx].consent, now);
    printf("  %s is now %s.\n", donors[idx].name,
           donors[idx].available ? "AVAILABLE" : "UNAVAILABLE");
}

/* ================================================================== */
/*  Menu 5: Red-Black Tree Demo                                       */
/* ================================================================== */

static int rbt_print_count;
static void rbt_print_visitor(const RBNode *node, void *ctx) {
    (void)ctx;
    rbt_print_count++;
    printf("    %3d. ID %-6llu | Group: %-4s | (%9.4f, %9.4f) | %s | %s\n",
           rbt_print_count,
           (unsigned long long)node->key,
           (node->blood_group >= 0 && node->blood_group <= 7)
               ? BLOOD_NAMES[node->blood_group] : "???",
           node->latitude, node->longitude,
           node->color == RB_RED ? "RED  " : "BLACK",
           (node->available && node->consent_current) ? "Active" : "Inactive");
}

static void rbtree_demo(void) {
    printf("\n  ========================================\n");
    printf("  RED-BLACK TREE DEMONSTRATION\n");
    printf("  ========================================\n\n");

    printf("  The Red-Black Tree is a self-balancing BST that guarantees\n");
    printf("  O(log n) worst-case time for insert, delete, and search.\n\n");
    printf("  Current tree size: %zu donors\n\n", rbt_size(rb_registry));

    printf("  --- In-Order Traversal (sorted by Donor ID) ---\n\n");
    rbt_print_count = 0;
    rbt_inorder(rb_registry, rbt_print_visitor, NULL);

    /* Search demo */
    printf("\n  --- Search Demo ---\n");
    uint64_t search_ids[] = {1001, 1010, 1024, 9999};
    for (int i = 0; i < 4; i++) {
        RBNode *found = rbt_search(rb_registry, search_ids[i]);
        if (found != rb_registry->nil) {
            int idx = hash_find(search_ids[i]);
            printf("    Search(%llu): FOUND — %s, %s, %s [%s]\n",
                   (unsigned long long)search_ids[i],
                   idx >= 0 ? donors[idx].name : "?",
                   BLOOD_NAMES[found->blood_group],
                   found->color == RB_RED ? "RED" : "BLACK",
                   idx >= 0 ? donors[idx].city : "?");
        } else {
            printf("    Search(%llu): NOT FOUND\n",
                   (unsigned long long)search_ids[i]);
        }
    }

    /* Min/Max demo */
    RBNode *mn = rbt_minimum(rb_registry, rb_registry->root);
    RBNode *mx = rbt_maximum(rb_registry, rb_registry->root);
    if (mn != rb_registry->nil)
        printf("\n    Minimum key: %llu\n", (unsigned long long)mn->key);
    if (mx != rb_registry->nil)
        printf("    Maximum key: %llu\n", (unsigned long long)mx->key);

    printf("\n  All operations: O(log n) guaranteed.\n");
    printf("  Tree height is at most 2*log2(n+1) = %.0f for n=%zu.\n",
           2.0 * log2((double)rbt_size(rb_registry) + 1), rbt_size(rb_registry));
}

/* ================================================================== */
/*  Menu 6: Skip List Demo                                            */
/* ================================================================== */

static void skiplist_demo(void) {
    printf("\n  ========================================\n");
    printf("  SKIP LIST DEMONSTRATION\n");
    printf("  ========================================\n\n");

    printf("  The Skip List is a probabilistic data structure providing\n");
    printf("  O(log n) expected time for search, insert, and delete.\n");
    printf("  Donors are indexed by blood group for fast group queries.\n\n");
    printf("  Current skip list size: %zu donors\n\n", sl_size(sl_index));

    printf("  --- Blood Group Query Results ---\n\n");
    for (int bg = 0; bg < 8; bg++) {
        uint64_t refs[MAX_DONORS];
        size_t found = sl_find_by_group(sl_index, bg, refs, MAX_DONORS);
        printf("    %s donors (%zu found): ", BLOOD_NAMES[bg], found);
        for (size_t j = 0; j < found && j < 8; j++) {
            int idx = hash_find(refs[j]);
            if (idx >= 0)
                printf("%s", donors[idx].name);
            else
                printf("ID:%llu", (unsigned long long)refs[j]);
            if (j < found - 1 && j < 7) printf(", ");
        }
        if (found > 8) printf(" ...");
        printf("\n");
    }

    /* Range query demo */
    printf("\n  --- Range Query Demo ---\n\n");
    printf("  Finding all donors with Rh-negative blood (O-, A-, B-, AB-):\n");
    uint64_t neg_refs[MAX_DONORS];
    size_t neg_count = 0;
    /* Rh-negative groups are codes 0, 2, 4, 6 */
    for (int bg = 0; bg <= 6; bg += 2) {
        size_t c = sl_find_by_group(sl_index, bg, neg_refs + neg_count,
                                     MAX_DONORS - neg_count);
        neg_count += c;
    }
    printf("    Found %zu Rh-negative donors:\n", neg_count);
    for (size_t j = 0; j < neg_count; j++) {
        int idx = hash_find(neg_refs[j]);
        if (idx >= 0)
            printf("      - %s (%s), %s\n", donors[idx].name,
                   donors[idx].blood, donors[idx].area);
    }

    /* Compatible donors range query */
    printf("\n  Finding all donors compatible with AB+ (universal recipient):\n");
    uint64_t all_refs[MAX_DONORS];
    size_t all_count = sl_range_query(sl_index, 0, 7, all_refs, MAX_DONORS);
    printf("    %zu donors (all groups are compatible with AB+).\n", all_count);
}

/* ================================================================== */
/*  Menu 7: Graph Routing Demo (Dijkstra)                             */
/* ================================================================== */

static void graph_demo(void) {
    printf("\n  ========================================\n");
    printf("  DIJKSTRA ROUTING DEMONSTRATION\n");
    printf("  ========================================\n\n");

    printf("  Weighted graph of %d hospital locations.\n", MAX_GRAPH_NODES);
    printf("  Dijkstra's algorithm finds shortest-path travel times.\n\n");

    printf("  --- Travel Time Matrix (Bengaluru network) ---\n\n");
    printf("  %-8s", "From\\To");
    for (int j = 0; j < 8; j++)  /* Show Bengaluru network only */
        printf("  %-6d", j + 1);
    printf("\n  ");
    for (int j = 0; j < 8 * 8 + 8; j++) printf("-");
    printf("\n");
    for (int i = 0; i < 8; i++) {
        printf("  Loc %-3d", i + 1);
        for (int j = 0; j < 8; j++) {
            double t = road_time(i, j);
            if (t >= 999.0)
                printf("  %-6s", "---");
            else
                printf("  %-6.0f", t);
        }
        printf("  %s\n", locations[i]);
    }

    printf("\n  --- Example Shortest Paths ---\n\n");
    int examples[][2] = {{0, 6}, {1, 4}, {2, 7}, {3, 5}};
    for (int i = 0; i < 4; i++) {
        int f = examples[i][0], t = examples[i][1];
        double time_val = road_time(f, t);
        printf("    %s -> %s\n", locations[f], locations[t]);
        printf("    Shortest travel time: %.0f minutes\n\n", time_val);
    }
}

/* ================================================================== */
/*  Menu 8: Delete donor (RB-Tree + Skip List demo)                   */
/* ================================================================== */

static void delete_donor(void) {
    uint64_t id;
    printf("\n  Donor ID to remove: ");
    if (scanf("%llu", (unsigned long long *)&id) != 1) return;
    int idx = hash_find(id);
    if (idx < 0) { printf("  Donor not found.\n"); return; }

    printf("  Removing %s (ID: %llu) from registry...\n",
           donors[idx].name, (unsigned long long)id);

    int rb_rc = rbt_delete(rb_registry, id);
    int sl_rc = sl_delete(sl_index, donors[idx].blood_code, id);

    printf("  Red-Black Tree delete: %s\n", rb_rc == 0 ? "OK" : "FAILED");
    printf("  Skip List delete:      %s\n", sl_rc == 0 ? "OK" : "FAILED");

    /* Mark as inactive (don't remove from array to keep indices stable) */
    donors[idx].available = 0;
    donors[idx].consent   = 0;

    printf("  RB-Tree size now: %zu  |  Skip List size: %zu\n",
           rbt_size(rb_registry), sl_size(sl_index));
}

/* ================================================================== */
/*  Menu 9: Search donor by ID (Hash + RB-Tree comparison)            */
/* ================================================================== */

static void search_donor(void) {
    uint64_t id;
    printf("\n  Donor ID to search: ");
    if (scanf("%llu", (unsigned long long *)&id) != 1) return;

    printf("\n  --- Hash Table Lookup (O(1) average) ---\n");
    int idx = hash_find(id);
    if (idx >= 0) {
        Donor *d = &donors[idx];
        printf("    Found: %s | %s | %s, %s | %s\n",
               d->name, d->blood, d->area, d->city,
               d->available ? "Available" : "Unavailable");
    } else {
        printf("    Not found.\n");
    }

    printf("\n  --- Red-Black Tree Search (O(log n) worst-case) ---\n");
    RBNode *node = rbt_search(rb_registry, id);
    if (node != rb_registry->nil) {
        printf("    Found: ID %llu | Group: %s | Coords: (%.4f, %.4f)\n",
               (unsigned long long)node->key,
               BLOOD_NAMES[node->blood_group],
               node->latitude, node->longitude);
        printf("    Node color: %s\n",
               node->color == RB_RED ? "RED" : "BLACK");
    } else {
        printf("    Not found.\n");
    }
}

/* ================================================================== */
/*  Main menu                                                         */
/* ================================================================== */

int main(void) {
    seed_data();

    printf("\n");
    printf("  ================================================================\n");
    printf("  |                                                              |\n");
    printf("  |   BloodLink — Smart Blood Donor Matching Network             |\n");
    printf("  |   Advanced Data Structures Implementation                    |\n");
    printf("  |                                                              |\n");
    printf("  |   Data Structures: Red-Black Tree, Skip List, KD-Tree,      |\n");
    printf("  |                    Max-Heap, Hash Table, Dijkstra Graph      |\n");
    printf("  |                                                              |\n");
    printf("  |   Loaded: %d donors across %d hospital locations            |\n",
           donor_count, MAX_GRAPH_NODES);
    printf("  |                                                              |\n");
    printf("  ================================================================\n");

    for (;;) {
        int choice;
        printf("\n");
        printf("  +------------------------------------------------------+\n");
        printf("  |  MAIN MENU                                           |\n");
        printf("  +------------------------------------------------------+\n");
        printf("  |  1.  View all donors                                 |\n");
        printf("  |  2.  Create emergency blood request                  |\n");
        printf("  |  3.  Register new donor                              |\n");
        printf("  |  4.  Toggle donor availability                       |\n");
        printf("  |  5.  Red-Black Tree demo                             |\n");
        printf("  |  6.  Skip List demo                                  |\n");
        printf("  |  7.  Dijkstra graph routing demo                     |\n");
        printf("  |  8.  Delete donor (RB-Tree + Skip List)              |\n");
        printf("  |  9.  Search donor by ID (Hash vs RB-Tree)            |\n");
        printf("  | 10.  Save donors to CSV                              |\n");
        printf("  |  0.  Exit                                            |\n");
        printf("  +------------------------------------------------------+\n");
        printf("  Choose (0-10): ");

        if (scanf("%d", &choice) != 1) break;
        switch (choice) {
            case 1:  list_donors();        break;
            case 2:  request_blood();      break;
            case 3:  register_donor();     break;
            case 4:  toggle_availability(); break;
            case 5:  rbtree_demo();        break;
            case 6:  skiplist_demo();      break;
            case 7:  graph_demo();         break;
            case 8:  delete_donor();       break;
            case 9:  search_donor();       break;
            case 10: save_donors();        break;
            case 0:
                printf("\n  Thank you for using BloodLink. Goodbye!\n\n");
                rbt_destroy(rb_registry);
                sl_destroy(sl_index);
                return 0;
            default:
                printf("  Please choose 0-10.\n");
                break;
        }
    }

    rbt_destroy(rb_registry);
    sl_destroy(sl_index);
    return 0;
}
