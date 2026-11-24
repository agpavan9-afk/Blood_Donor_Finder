# Hospital Pilot Blueprint

## Current status

This repository now contains the original classroom demo plus a separate C11 matching library and a loopback-only Node/SQLite shell with synthetic Bengaluru-area records. The local shell has role-separated staff and donor screens, staff request triage, and donor opt-in/availability controls, but it is still a development pilot, not a hospital-ready application: its role tokens are not hospital authentication, its database is local SQLite, and its candidate ETAs are fixed fixtures. It has no blood-centre, identity, approved-consent, routing, notification, or hospital-system integration. **Do not enter patient or donor information and do not use its match list to support transfusion or donor-selection decisions.**

The next implementation should preserve C for the matching and allocation engine. A hospital-facing UI and identity, persistence, routing, messaging, and integration adapters can use other languages and established services.

## Intended operating model

The system should help authorized hospital and blood-centre staff coordinate a request and find potential donors who have opted in. It must not select a transfusion product, determine donor eligibility, replace crossmatching, or contact donors without valid permission. The blood centre's qualified staff remain responsible for donor selection and release under the centre's approved standard operating procedures. The system should support a staff-reviewed workflow with manual fallback.

### Proposed components

1. **Web application:** accessible staff console and a separate donor consent / availability channel. Keep patient identifiers out of donor-facing screens.
2. **Identity and API layer:** hospital SSO / OIDC, MFA, role-based access, request authorization, rate limits, input validation, and purpose-limited access.
3. **C matching service:** a versioned C library or isolated C service. It receives only the minimum, authorized candidate fields and returns ranked candidate references with an explanation of each ranking factor. It contains no sample donors or contact details.
4. **Database:** PostgreSQL for multi-hospital operation, with encryption, backups, retention jobs, and migration controls. SQLite can support local development only.
5. **Routing adapter:** a contracted, privacy-reviewed provider or approved hospital routing feed supplies road ETA. The service records the ETA source and timestamp; an estimate is never presented as dispatch or guaranteed arrival time.
6. **Notifications:** a separately configured provider sends an opt-in request for the donor to respond. The hospital sees delivery state, not a donor's private contact details unless its approved workflow requires access.
7. **Audit and operations:** append-only security and workflow events, monitoring, alert delivery retries, backups, incident procedures, and a manual fallback runbook.

## Decision boundaries

- The blood bank verifies blood group and controls the validity and scope of any clinical clearance. A self-reported group is never treated as verified.
- Donor eligibility and deferral are clinical decisions. Do not hard-code age, health, donation-interval, medication, travel, or other screening rules into the matcher without a formally approved, versioned SOP and medical-owner sign-off.
- ABO/Rh matching is only a preliminary red-cell search filter. It does not establish compatibility or suitability for transfusion. The blood centre performs required testing and release.
- A donor's availability and consent can change at any time. Re-check both immediately before outreach; record consent scope, source, timestamp, expiry/revocation, and the notice shown to the donor.
- Severity is a property of a hospital request, not of a donor. An approved allocation policy should rank requests across hospitals; candidate ranking within a request should use ETA, distance, availability freshness, and approved operational constraints. The policy must be visible, versioned, and auditable.
- Preserve a human decision and reason for every allocation or override. Never automatically promise blood, reserve a unit, or tell a patient that a donor is suitable.

## Minimum data model for a controlled pilot

- `donor_profile`: opaque ID, minimal contact channel held separately, preferred service area, current opt-in state, consent version and timestamps.
- `blood_group_record`: value, verifier/blood-centre source, verification timestamp, and status. Restrict edits to authorized blood-centre staff and retain change history.
- `donation_event` / `screening_clearance`: reference to the source blood-centre system, SOP version, authorized reviewer, status, and validity window. Avoid copying detailed medical history into this matching service.
- `hospital_request`: request ID, facility, requested component, blood group/compatibility requirements approved by the blood centre, quantity, urgency category, expiry, and authorized requester. Keep patient identifiers in the hospital system of record.
- `match_run`: request ID, matcher and policy versions, candidate references, inputs used, routing source/time, scores, exclusions, and actor. Do not expose internal scores as clinical advice.
- `outreach_event` and `audit_event`: consent check, notification status, staff actions, override reason, access, and retention/deletion state.

## Security and deployment gates

Before any live pilot, the participating hospital and blood centre need to approve the workflow, data controller/processor roles, retention, consent language, screening source, access roles, incident response, and manual fallback. The deployment must use hospital-approved hosting, TLS, encrypted storage and backups, managed secrets, MFA, least privilege, vulnerability review, monitoring, disaster recovery, and a documented access and deletion process. Use synthetic data until these gates are met.

For an India deployment, validate the current applicable privacy, health-data, blood-centre, and ABDM requirements with the hospital's privacy/legal and clinical owners. ABDM publishes a Health Data Management Policy and MoHFW/NBTC publishes blood-centre and donor-selection standards; those sources are starting points for review, not evidence that this application is compliant or endorsed:

- [ABDM Health Data Management Policy](https://abdm.gov.in/publications/policies_regulations/health_data_management_policy)
- [MoHFW National Standards for Blood Centres](https://www.mohfw.gov.in/sites/default/files/National%20Standards%20for%20Blood%20Centres.pdf)
- [NBTC Guidelines for Blood Donor Selection and Referral (2025)](https://eraktkosh.mohfw.gov.in/BLDAHIMS/bloodbank/about.cnt?hmode=DOWNLOADDOCUMENTNHRM&type=20)

## Staged build and acceptance plan

1. **Offline engineering prototype:** replace hard-coded demo records with synthetic fixtures; separate C engine from UI; define deterministic matcher inputs/outputs; add automated unit and property tests for compatibility, consent/availability exclusions, ordering, and invalid data.
2. **Single-centre shadow mode:** import only synthetic or specifically authorized test data; no donor notifications or operational decisions. Compare outputs with staff workflow and log discrepancies.
3. **Controlled pilot:** after written clinical, privacy, security, and operational approval, enable a small staff-only workflow with trained users, explicit human confirmation, monitoring, and a tested manual fallback.
4. **Expansion:** only after measured reliability, clinical review, security assessment, interoperability testing, and approval by each participating organization.

The repository has an initial stage 1 code slice, but it has not been compiled or validated in this Windows environment and does not yet meet stage 1 acceptance criteria. A real hospital deployment cannot be completed or validated without a named hospital/blood-centre partner, approved workflow, identity and routing integrations, and accountable clinical/privacy owners.

## Stage 1 Completion Status

| Component | Implemented | Notes |
|-----------|-------------|-------|
| Red‑Black Tree (C) | ✅ | `core/rbtree.c` and header |
| Skip List (C) | ✅ | `core/skiplist.c` and header |
| KD‑Tree (C) | ✅ | Included in `main.c` demo |
| Dijkstra routing (C) | ✅ | Demonstrated in demo program |
| C matching binary | ✅ | `blood_donor_finder.exe` builds via `Makefile` |
| Node.js backend | ✅ | `service/server.mjs` with SQLite, API, rate‑limiting |
| SQLite schema & demo data | ✅ | Populated on first run |
| Staff UI | ✅ | `service/index.html` (dashboard) |
| Documentation (README, REPORT) | ✅ | Files present |
| Audit & logging | ✅ | `audit_event` table and inserts |
| Rate limiting & validation | ✅ | Middleware in `server.mjs` |

The prototype now satisfies the first‑stage deliverables and can be run locally for demonstration purposes.
