# Local synthetic-data pilot

This service is a local development shell around the C matcher. It uses Node's built-in HTTP server and `node:sqlite` (use Node 24), then calls `bdf_matcher_cli` as a child process. The browser supports separate synthetic staff and donor workflows, ranked sample candidates, and a C-computed request review queue.

## Run locally

1. Build the C executable using a working C11 compiler:

   ```powershell
   C:\msys64\ucrt64\bin\gcc.exe -std=c11 -O2 core\matcher.c core\matcher_cli.c -lm -o core\bdf_matcher_cli.exe
   ```

2. In PowerShell, set separate long random local-only tokens for the two demo roles and enable the synthetic fixture data:

   ```powershell
   $env:BDF_STAFF_TOKEN = 'replace-with-a-random-staff-secret-at-least-24-characters'
   $env:BDF_DONOR_TOKEN = 'replace-with-a-different-donor-secret-at-least-24-characters'
   $env:BDF_DEMO_DATA = '1'
   node service\server.mjs
   ```

3. Open http://127.0.0.1:3000 to use the staff coordination workspace.

The local service binds only to loopback, rejects other bind addresses, and keeps tokens in browser memory. Each token grants one demo role. `BDF_DONOR_REF` can select which synthetic profile the donor portal controls; it defaults to `900000001`. Seed profiles begin opted out and unavailable so the donor can explicitly exercise the opt-in flow. These tokens are development gates, not user identity or hospital access control. The database file is created under `data/` unless `BDF_DB_PATH` is set. Demo candidate rows are synthetic, and their consent/availability/ETA timestamps are refreshed only to exercise the local demo path.

## What is implemented

- C KD-tree spatial search and bounded priority queue for candidate ranking.
- C request priority calculation using configurable severity, scarcity, and wait weights.
- Local SQLite tables for candidate fixture rows, requests, and simple workflow audit events.
- Browser form for sample requests and a results view that reveals only opaque donor references.
- Age limits on consent, availability, and route ETA snapshots before a candidate is returned.
- Separate staff and donor demo portals with role-scoped local tokens.
- Staff request creation, status/group/urgency/ID filters, queue priority, request closure, candidate shortlists, and recent pilot activity.
- Donor sample-profile summary, explicit opt-in or withdrawal, availability controls, and personal preference history.
- Keyboard-friendly navigation, visible focus, responsive layout, live status announcements, and reduced-motion support.

## Not a hospital deployment

This is not production-ready and must never receive real donor or patient data. It has no hospital SSO/MFA, multi-tenant authorization, approved consent flow, production database, encrypted secrets, external routing provider, notification provider, operational monitoring, backup/restore, incident response, or clinical workflow approval. Its local bearer token is not an acceptable substitute for identity management. Demo ETAs are fixed samples and cannot be used for travel planning. The score weights are demonstration values and have not been approved by a hospital.

For a real pilot, retain the C engine but replace this local adapter with hospital-approved identity, APIs, PostgreSQL, routing, consent, and audit integrations. See [the hospital pilot blueprint](../HOSPITAL_PILOT_BLUEPRINT.md).
