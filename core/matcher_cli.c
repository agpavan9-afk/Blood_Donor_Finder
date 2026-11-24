#include "matcher.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_SIZE 1024

static int only_space_after(const char *line, int pos) {
    if (pos < 0) return 0;
    while (line[pos] == ' ' || line[pos] == '\r' || line[pos] == '\n' || line[pos] == '\t') pos++;
    return line[pos] == '\0';
}

static int priority_mode(void) {
    char line[LINE_SIZE]; int severity, used = 0;
    double scarcity, waiting, severity_weight, scarcity_weight, waiting_weight;
    if (!fgets(line, sizeof line, stdin) ||
        sscanf(line, "%d\t%lf\t%lf\t%lf\t%lf\t%lf%n", &severity, &scarcity, &waiting,
               &severity_weight, &scarcity_weight, &waiting_weight, &used) != 6 || !only_space_after(line, used)) {
        fputs("invalid priority input\n", stderr); return 2;
    }
    double score = bdf_request_priority(severity, scarcity, waiting,
                                        severity_weight, scarcity_weight, waiting_weight);
    if (!(score == score)) { fputs("invalid priority values\n", stderr); return 2; }
    printf("%.6f\n", score);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--priority") == 0) return priority_mode();
    if (argc != 1) { fputs("unsupported command\n", stderr); return 2; }
    char line[LINE_SIZE];
    BDFMatchRequest request;
    int group, used = 0;
    long long now, max_consent_age, max_availability_age, max_eta_age;
    if (!fgets(line, sizeof line, stdin) ||
        sscanf(line, "%d\t%lf\t%lf\t%lf\t%lf\t%lld\t%lld\t%lld\t%lld\t%lf%n",
               &group, &request.latitude, &request.longitude, &request.radius_km,
               &request.max_eta_minutes, &now, &max_consent_age, &max_availability_age, &max_eta_age,
               &request.eta_weight, &used) != 10 || !only_space_after(line, used)) {
        fputs("invalid request input\n", stderr); return 2;
    }
    request.requested_group = (BDFBloodGroup)group;
    request.now_epoch = (int64_t)now;
    request.max_consent_age_seconds = (int64_t)max_consent_age;
    request.max_availability_age_seconds = (int64_t)max_availability_age;
    request.max_eta_age_seconds = (int64_t)max_eta_age;

    BDFDonorCandidate *candidates = NULL;
    size_t count = 0, capacity = 0;
    while (fgets(line, sizeof line, stdin)) {
        BDFDonorCandidate d;
        unsigned long long ref;
        long long consent_at, available_at, eta_at;
        int donor_group, consent, available, verified, active;
        used = 0;
        int fields = sscanf(line, "%llu\t%d\t%lf\t%lf\t%lf\t%d\t%d\t%d\t%d\t%lld\t%lld\t%lld%n",
                            &ref, &donor_group, &d.latitude, &d.longitude, &d.eta_minutes,
                            &consent, &available, &verified, &active, &consent_at, &available_at, &eta_at, &used);
        if (fields != 12 || !only_space_after(line, used)) {
            free(candidates); fputs("invalid candidate input\n", stderr); return 2;
        }
        if (count == capacity) {
            size_t next = capacity ? capacity * 2 : 32;
            if (next < capacity || next > SIZE_MAX / sizeof(*candidates)) {
                free(candidates); fputs("too many candidates\n", stderr); return 2;
            }
            BDFDonorCandidate *grown = (BDFDonorCandidate *)realloc(candidates, next * sizeof(*candidates));
            if (!grown) { free(candidates); fputs("out of memory\n", stderr); return 2; }
            candidates = grown; capacity = next;
        }
        d.donor_ref = (uint64_t)ref; d.blood_group = (BDFBloodGroup)donor_group;
        d.consent_current = consent; d.available = available; d.blood_group_verified = verified;
        d.profile_active = active; d.consent_checked_at = (int64_t)consent_at;
        d.availability_checked_at = (int64_t)available_at;
        d.eta_checked_at = (int64_t)eta_at;
        candidates[count++] = d;
    }

    BDFMatch *matches = count ? (BDFMatch *)malloc(count * sizeof(*matches)) : NULL;
    if (count && !matches) { free(candidates); fputs("out of memory\n", stderr); return 2; }
    size_t matched = 0;
    int rc = bdf_match(candidates, count, &request, matches, count, &matched);
    if (rc != 0) {
        free(matches); free(candidates); fputs("matcher rejected input\n", stderr); return 2;
    }
    for (size_t i = 0; i < matched; i++)
        printf("%" PRIu64 "\t%.3f\t%.2f\t%.3f\n", matches[i].donor_ref,
               matches[i].distance_km, matches[i].eta_minutes, matches[i].score);
    free(matches); free(candidates);
    return 0;
}
