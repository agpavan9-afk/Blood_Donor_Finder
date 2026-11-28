# Blood Donor Finder — Technical Report

## 1. Purpose and scope

Blood Donor Finder is an offline engineering prototype that demonstrates how advanced data structures can help rank potential blood donors for a hospital request. It uses synthetic Bengaluru-area records only. It is not a clinical decision system, donor registry, blood inventory system, or production hospital application.

The prototype keeps the matching implementation in C11. A local Node.js service provides persistence and an accessible browser interface so the C engine can be exercised as a complete application.

## 2. Implemented architecture

| Component | Implementation | Responsibility |
|---|---|---|
| Matching engine | `core/matcher.c` and `core/matcher_cli.c` | Validate candidate data, apply preliminary compatibility and freshness filters, perform geographic search, and retain the best-ranked matches |
| Local API | `service/server.mjs` | Validate requests, resolve trusted hospital coordinates, read/write SQLite, invoke the C process, and return privacy-limited results |
| Browser interface | `service/index.html` | Show synthetic operational statistics, donor availability controls, hospital requests, and ranked candidate references |
| Local database | SQLite | Store synthetic donors, hospitals, requests, match state, and audit events |
| Data-structure demonstration | `main.c` and supporting `core` modules | Demonstrate a Red-Black Tree, Skip List, KD-tree, priority queue, hash table, and Dijkstra graph routing |

The operational matcher is compiled as `core/bdf_matcher_cli.exe`. It reads a strict tab-separated request and candidate stream from standard input and writes tab-separated match rows to standard output. It does not receive donor contact details and does not emit them. The service intentionally has no JavaScript matching fallback: a missing, timed-out, or invalid C engine is reported as an error rather than silently changing algorithms.

## 3. Advanced data structures and algorithms

### Operational matching path

- **Two-dimensional KD-tree:** indexes candidate latitude and longitude for a radius query. Tree construction is approximately `O(n log n)` for the prototype; average spatial queries can prune much of the tree, while the worst case remains `O(n)`.
- **Bounded binary max-heap:** retains at most the requested number of best candidates. Its root is the least desirable retained match, allowing a better candidate to replace it in `O(log k)` time, where `k` is the result limit.
- **Haversine distance:** calculates great-circle distance after KD-tree traversal. The pruning logic handles longitude wraparound and polar regions conservatively.
- **Request priority formula:** a separate C function combines severity, scarcity, and waiting time with validated weights. This demonstrates request-level priority calculation; the current UI does not automatically allocate across hospitals.

### Classroom demonstration path

The standalone `blood_donor_finder.exe` additionally demonstrates:

- a Red-Black Tree for balanced `O(log n)` search, insertion, and deletion;
- a probabilistic Skip List with expected `O(log n)` operations;
- a hash table for key-based lookup;
- a priority queue; and
- Dijkstra shortest-path search on a demonstration graph.

These structures satisfy the educational portion of the project, but the Red-Black Tree, Skip List, and Dijkstra demo are not falsely presented as part of the current API matching path.

## 4. Matching workflow

1. Hospital staff select one of the server-owned synthetic hospital records and enter a request.
2. The API validates the blood group, severity, units, and hospital identifier. Client-supplied hospital coordinates are not trusted.
3. SQLite supplies active candidate records to the C process.
4. The C engine excludes records that are inactive, unavailable, unverified, outside consent/availability/ETA freshness limits, incompatible for the preliminary red-cell search, beyond the radius, or beyond the ETA limit.
5. Eligible candidates receive a normalized demonstration score combining proximity and fixture ETA.
6. The bounded heap returns the best candidates; deterministic tie-breakers prefer lower ETA, then shorter distance, then donor reference.
7. The API returns opaque candidate references, distance, ETA, and score. Public responses omit donor coordinates and contact details.

ABO/Rh filtering is only a preliminary search aid. It does not establish donor eligibility, component suitability, crossmatch compatibility, or permission to contact a donor.

## 5. Verification

The automated `npm test` workflow performs:

- strict C11 compilation with `-Wall -Wextra -Wpedantic -Werror`;
- C matcher checks for compatibility, stale records, ranking, request priority, antimeridian distance handling, and invalid input;
- a standalone console smoke test;
- Node.js and browser-script syntax checks; and
- API integration tests covering request validation, availability updates, C-process matching, response privacy, persistence, idempotent statistics, malformed JSON, and UI delivery.

The verified Windows toolchain is MSYS2 UCRT64 GCC 16.1.0. Build scripts use `CC` when supplied, otherwise `gcc` from `PATH`; no machine-specific legacy MinGW path is embedded in the repository.

## 6. Safety, research, and future development

The current database is local and synthetic, ETAs are fixtures, and there is no authentication, hospital tenant isolation, approved donor consent/identity workflow, notification provider, clinical screening integration, or live routing service. It must not receive real patient or donor data or be used for operational decisions.

A controlled hospital pilot would require clinical, privacy, legal, security, and operational ownership; hospital SSO/MFA and role authorization; approved consent and verification flows; encrypted production storage and backups; auditable policy versions; real routing and messaging adapters; monitoring and incident response; independent security testing; and validation against applicable Indian and Karnataka requirements. Only after those gates could performance studies, publication, intellectual-property review, partnerships, or commercialization be evaluated responsibly.
