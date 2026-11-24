#ifndef BDF_MATCHER_H
#define BDF_MATCHER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Preliminary ABO/Rh red-cell search groups only. This is not crossmatch logic. */
typedef enum {
    BDF_O_NEG = 0, BDF_O_POS, BDF_A_NEG, BDF_A_POS,
    BDF_B_NEG, BDF_B_POS, BDF_AB_NEG, BDF_AB_POS
} BDFBloodGroup;

typedef struct {
    uint64_t donor_ref;             /* opaque database reference; no PII */
    BDFBloodGroup blood_group;
    double latitude;
    double longitude;
    double eta_minutes;             /* supplied by approved routing adapter */
    int consent_current;
    int available;
    int blood_group_verified;
    int profile_active;
    int64_t consent_checked_at;
    int64_t availability_checked_at;
    int64_t eta_checked_at;
} BDFDonorCandidate;

typedef struct {
    BDFBloodGroup requested_group;
    double latitude;
    double longitude;
    double radius_km;
    double max_eta_minutes;
    int64_t now_epoch;
    int64_t max_consent_age_seconds;
    int64_t max_availability_age_seconds;
    int64_t max_eta_age_seconds;
    double eta_weight;              /* 0..1; distance weight is 1 - eta_weight */
} BDFMatchRequest;

typedef struct {
    uint64_t donor_ref;
    double distance_km;
    double eta_minutes;
    double score;                   /* transparent routing score, 0..100 */
} BDFMatch;

/* Returns 0 on success, negative for invalid input/allocation failure.
   count receives the number of ranked matches written (at most output_capacity).
   Severity is deliberately handled at request-allocation level, not donor ranking. */
int bdf_match(const BDFDonorCandidate *candidates, size_t candidate_count,
              const BDFMatchRequest *request, BDFMatch *output,
              size_t output_capacity, size_t *count);

/* Higher priority means earlier staff review. Weights and policy values must be
   approved and configured by the participating institution before live use. */
double bdf_request_priority(int severity_level, double scarcity_score,
                            double waiting_minutes, double severity_weight,
                            double scarcity_weight, double waiting_weight);

const char *bdf_blood_group_name(BDFBloodGroup group);

#ifdef __cplusplus
}
#endif
#endif
