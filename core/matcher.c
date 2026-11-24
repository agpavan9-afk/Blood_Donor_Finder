#include "matcher.h"

#include <math.h>
#include <stdlib.h>

typedef struct {
    size_t candidate_index;
    size_t left;
    size_t right;
    unsigned char axis;
} KDNode;

#define NO_NODE ((size_t)-1)
#define EARTH_KM 6371.0088

static int valid_group(BDFBloodGroup g) { return g >= BDF_O_NEG && g <= BDF_AB_POS; }

const char *bdf_blood_group_name(BDFBloodGroup group) {
    static const char *names[] = {"O-", "O+", "A-", "A+", "B-", "B+", "AB-", "AB+"};
    return valid_group(group) ? names[(int)group] : "INVALID";
}

static int abo(BDFBloodGroup g) {
    if (g == BDF_O_NEG || g == BDF_O_POS) return 0;
    if (g == BDF_A_NEG || g == BDF_A_POS) return 1;
    if (g == BDF_B_NEG || g == BDF_B_POS) return 2;
    return 3;
}

/* Preliminary red-cell ABO/Rh compatibility; final compatibility belongs to the blood centre. */
static int red_cell_search_compatible(BDFBloodGroup donor, BDFBloodGroup recipient) {
    int d = abo(donor), r = abo(recipient);
    int donor_rh_negative = ((int)donor % 2) == 0;
    int recipient_rh_negative = ((int)recipient % 2) == 0;
    int abo_ok = (r == 3) || (d == 0) || (d == r);
    return abo_ok && (!recipient_rh_negative || donor_rh_negative);
}

static double to_radians(double degrees) { return degrees * 0.017453292519943295; }

static double distance_km(double lat1, double lon1, double lat2, double lon2) {
    double p1 = to_radians(lat1), p2 = to_radians(lat2);
    double dp = p2 - p1, dl = to_radians(lon2 - lon1);
    double a = sin(dp / 2.0) * sin(dp / 2.0) + cos(p1) * cos(p2) * sin(dl / 2.0) * sin(dl / 2.0);
    if (a > 1.0) a = 1.0;
    return 2.0 * EARTH_KM * asin(sqrt(a));
}

static double coordinate(const BDFDonorCandidate *c, size_t i, unsigned char axis) {
    return axis ? c[i].longitude : c[i].latitude;
}

static int index_less(const BDFDonorCandidate *c, size_t a, size_t b, unsigned char axis) {
    double x = coordinate(c, a, axis), y = coordinate(c, b, axis);
    return x < y || (x == y && c[a].donor_ref < c[b].donor_ref);
}

static void sort_indices(const BDFDonorCandidate *c, size_t *v, size_t n, unsigned char axis) {
    if (n < 2) return;
    size_t i = 0, j = n - 1, pivot = v[n / 2];
    for (;;) {
        while (index_less(c, v[i], pivot, axis)) i++;
        while (index_less(c, pivot, v[j], axis)) j--;
        if (i >= j) break;
        size_t t = v[i]; v[i] = v[j]; v[j] = t;
        i++; if (j) j--;
    }
    size_t cut = i;
    if (cut == 0 || cut >= n) cut = n / 2;
    sort_indices(c, v, cut, axis);
    sort_indices(c, v + cut, n - cut, axis);
}

static size_t build_tree(const BDFDonorCandidate *c, size_t *indices, size_t n,
                         unsigned char axis, KDNode *nodes, size_t *used) {
    if (!n) return NO_NODE;
    sort_indices(c, indices, n, axis);
    size_t mid = n / 2, node_index = (*used)++;
    nodes[node_index].candidate_index = indices[mid];
    nodes[node_index].axis = axis;
    nodes[node_index].left = build_tree(c, indices, mid, (unsigned char)(axis ^ 1), nodes, used);
    nodes[node_index].right = build_tree(c, indices + mid + 1, n - mid - 1,
                                        (unsigned char)(axis ^ 1), nodes, used);
    return node_index;
}

static int result_compare(const void *pa, const void *pb) {
    const BDFMatch *a = (const BDFMatch *)pa, *b = (const BDFMatch *)pb;
    if (a->score != b->score) return a->score > b->score ? -1 : 1;
    if (a->eta_minutes != b->eta_minutes) return a->eta_minutes < b->eta_minutes ? -1 : 1;
    if (a->distance_km != b->distance_km) return a->distance_km < b->distance_km ? -1 : 1;
    return (a->donor_ref > b->donor_ref) - (a->donor_ref < b->donor_ref);
}

typedef struct {
    const BDFDonorCandidate *candidates;
    const BDFMatchRequest *request;
    KDNode *nodes;
    BDFMatch *matches;
    size_t match_count;
    size_t match_capacity;
} Search;

/* Bounded binary max-heap: the least desirable retained match stays at the root. */
static void offer_match(Search *s, BDFMatch match) {
    if (s->match_count < s->match_capacity) {
        size_t i = s->match_count++;
        while (i > 0) {
            size_t parent = (i - 1) / 2;
            if (result_compare(&match, &s->matches[parent]) <= 0) break;
            s->matches[i] = s->matches[parent]; i = parent;
        }
        s->matches[i] = match;
        return;
    }
    if (result_compare(&match, &s->matches[0]) >= 0) return;
    size_t i = 0;
    while (2 * i + 1 < s->match_count) {
        size_t child = 2 * i + 1;
        if (child + 1 < s->match_count && result_compare(&s->matches[child + 1], &s->matches[child]) > 0) child++;
        if (result_compare(&match, &s->matches[child]) >= 0) break;
        s->matches[i] = s->matches[child]; i = child;
    }
    s->matches[i] = match;
}

static void search_tree(Search *s, size_t node_index) {
    if (node_index == NO_NODE) return;
    const KDNode *node = &s->nodes[node_index];
    const BDFDonorCandidate *d = &s->candidates[node->candidate_index];
    const BDFMatchRequest *r = s->request;
    double km = distance_km(r->latitude, r->longitude, d->latitude, d->longitude);
    double consent_age = (double)r->now_epoch - (double)d->consent_checked_at;
    double availability_age = (double)r->now_epoch - (double)d->availability_checked_at;
    double eta_age = (double)r->now_epoch - (double)d->eta_checked_at;
    int state_fresh = availability_age >= 0.0 && availability_age <= (double)r->max_availability_age_seconds;
    int consent_fresh = consent_age >= 0.0 && consent_age <= (double)r->max_consent_age_seconds;
    int eta_fresh = eta_age >= 0.0 && eta_age <= (double)r->max_eta_age_seconds;
    if (d->profile_active == 1 && d->consent_current == 1 && consent_fresh && d->available == 1 && d->blood_group_verified == 1 &&
        state_fresh && eta_fresh && red_cell_search_compatible(d->blood_group, r->requested_group) &&
        km <= r->radius_km && d->eta_minutes <= r->max_eta_minutes) {
        double proximity = 1.0 - km / r->radius_km;
        double eta_score = 1.0 - d->eta_minutes / r->max_eta_minutes;
        double score = 100.0 * ((1.0 - r->eta_weight) * proximity + r->eta_weight * eta_score);
        offer_match(s, (BDFMatch){d->donor_ref, km, d->eta_minutes, score});
    }

    double delta_km;
    if (node->axis == 0) {
        delta_km = (r->latitude - d->latitude) * (EARTH_KM * 0.017453292519943295);
    } else {
        double scale = cos(to_radians(r->latitude));
        delta_km = (r->longitude - d->longitude) * (EARTH_KM * 0.017453292519943295) * scale;
    }
    if (delta_km <= r->radius_km) search_tree(s, node->left);
    if (delta_km >= -r->radius_km) search_tree(s, node->right);
}

int bdf_match(const BDFDonorCandidate *candidates, size_t candidate_count,
              const BDFMatchRequest *request, BDFMatch *output,
              size_t output_capacity, size_t *count) {
    if (!request || !count || (!candidates && candidate_count) || (!output && output_capacity)) return -1;
    *count = 0;
    if (!valid_group(request->requested_group) || !isfinite(request->latitude) ||
        !isfinite(request->longitude) || fabs(request->latitude) > 90.0 || fabs(request->longitude) > 180.0 ||
        !isfinite(request->radius_km) || request->radius_km < 0.01 || request->radius_km > 20050.0 ||
        !isfinite(request->max_eta_minutes) || request->max_eta_minutes < 0.001 || request->max_eta_minutes > 10080.0 ||
        request->max_consent_age_seconds <= 0 || request->max_availability_age_seconds <= 0 ||
        request->max_eta_age_seconds <= 0 || !isfinite(request->eta_weight) ||
        request->eta_weight < 0.0 || request->eta_weight > 1.0) return -1;
    if (candidate_count == 0 || output_capacity == 0) return 0;
    if (candidate_count > SIZE_MAX / sizeof(KDNode) || candidate_count > SIZE_MAX / sizeof(size_t) ||
        output_capacity > SIZE_MAX / sizeof(BDFMatch)) return -2;

    KDNode *nodes = (KDNode *)malloc(candidate_count * sizeof(*nodes));
    size_t *indices = (size_t *)malloc(candidate_count * sizeof(*indices));
    size_t match_capacity = output_capacity < candidate_count ? output_capacity : candidate_count;
    BDFMatch *matches = (BDFMatch *)malloc(match_capacity * sizeof(*matches));
    if (!nodes || !indices || !matches) { free(nodes); free(indices); free(matches); return -2; }
    for (size_t i = 0; i < candidate_count; i++) {
        const BDFDonorCandidate *d = &candidates[i];
        if (d->donor_ref == 0 || (d->consent_current != 0 && d->consent_current != 1) ||
            (d->available != 0 && d->available != 1) ||
            (d->blood_group_verified != 0 && d->blood_group_verified != 1) ||
            (d->profile_active != 0 && d->profile_active != 1) ||
            !valid_group(d->blood_group) || !isfinite(d->latitude) || !isfinite(d->longitude) ||
            fabs(d->latitude) > 90.0 || fabs(d->longitude) > 180.0 || !isfinite(d->eta_minutes) || d->eta_minutes < 0.0) {
            free(nodes); free(indices); free(matches); return -1;
        }
        indices[i] = i;
    }
    size_t used = 0;
    size_t root = build_tree(candidates, indices, candidate_count, 0, nodes, &used);
    Search s = {candidates, request, nodes, matches, 0, match_capacity};
    search_tree(&s, root);
    qsort(matches, s.match_count, sizeof(*matches), result_compare);
    size_t n = s.match_count;
    for (size_t i = 0; i < n; i++) output[i] = matches[i];
    *count = n;
    free(nodes); free(indices); free(matches);
    return 0;
}

double bdf_request_priority(int severity_level, double scarcity_score,
                            double waiting_minutes, double severity_weight,
                            double scarcity_weight, double waiting_weight) {
    if (severity_level < 1 || severity_level > 5 || !isfinite(scarcity_score) ||
        !isfinite(waiting_minutes) || !isfinite(severity_weight) ||
        !isfinite(scarcity_weight) || !isfinite(waiting_weight) ||
        scarcity_score < 0.0 || scarcity_score > 1.0 || waiting_minutes < 0.0 ||
        severity_weight < 0.0 || scarcity_weight < 0.0 || waiting_weight < 0.0) return NAN;
    double priority = severity_level * severity_weight + scarcity_score * scarcity_weight + waiting_minutes * waiting_weight;
    return isfinite(priority) ? priority : NAN;
}
