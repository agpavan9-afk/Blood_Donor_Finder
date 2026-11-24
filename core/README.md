# C matching engine

This dependency-free C11 library is the first deployable-boundary extraction from the original command-line demo. It accepts opaque donor references and no names, phone numbers, hospital patient IDs, or contact details. Build it with CMake from this folder.

## Matching inputs

The caller supplies a request and candidate records from trusted services. The adapter must obtain current donor consent and availability, verified blood group, and a road ETA from approved systems. The engine then applies an approximate ABO/Rh **red-cell search filter**, freshness limits, a KD-tree geographic radius query, and a bounded binary max-heap to retain the best requested number of candidates. Candidate score combines normalized proximity and ETA; ties use ETA, distance, and opaque reference. It ranks candidates only within one request.

Severity is handled separately by `bdf_request_priority`, which orders requests for staff review using institution-configured severity, scarcity, and waiting-time weights. A hospital must approve and version these values before live use. Each participating facility can require a human decision and manual fallback.

## Medical and safety boundary

This is a technical shortlisting component. It does **not** determine whether a person may donate, establish transfusion compatibility, perform crossmatching, select a component, reserve blood, or dispatch a donor. ABO/Rh compatibility here is only a preliminary red-cell search filter. The blood centre owns verified blood-group data and all screening, clinical eligibility, testing, and release decisions under its current approved SOP. A search result must never be described as a safe or suitable transfusion match.

The caller must re-check consent, availability, blood-group verification, profile state, and ETA freshness immediately before any authorized outreach. It must keep sensitive records and audit data in hospital-approved services and pass only the minimum fields to this library. Inputs are untrusted until validated by that adapter.

## C interface

- `bdf_match(...)`: returns up to the requested number of ranked opaque donor references. Returns `0` on success, `-1` for invalid arguments/data, and `-2` for allocation/size failure.
- `bdf_request_priority(...)`: returns a policy score or `NAN` for invalid input. The caller owns policy limits, approval, versioning, and audit.
- `bdf_blood_group_name(...)`: names one of the eight supported ABO/Rh values.

No live deployment should use this component until it has institution-approved policy, test coverage, clinical review, integration and security review, and acceptance in shadow mode. It has not yet been connected to a database, staff identity provider, routing provider, or hospital UI.
