# Local web service

This folder contains the Express/SQLite adapter and the browser interface for the synthetic Bengaluru demonstration. The adapter launches `core/bdf_matcher_cli` for each matching request; there is no JavaScript matching fallback.

## Start the service

Run these commands from the project root:

```powershell
npm install
npm run build:c
npm start
```

Open [http://127.0.0.1:3000](http://127.0.0.1:3000). Serving the page through Express keeps the UI and API on the same origin.

Configuration is read from process environment variables:

| Variable | Default | Meaning |
| --- | --- | --- |
| `HOST` | `127.0.0.1` | Listening interface |
| `PORT` | `3000` | Listening port |
| `DB_FILE` | `blood_donor_finder.db` | SQLite file, relative to the project root unless absolute |
| `MATCHER_BINARY` | `core/bdf_matcher_cli.exe` on Windows | Compiled matcher path |

## Implemented boundary

- Three server-owned synthetic hospital locations and 20 synthetic donor fixtures.
- Request validation and trusted server-side hospital coordinates.
- Donor availability timestamps, consent/profile/verification flags, and ETA freshness limits.
- C KD-tree search and bounded-heap ranking.
- Match responses containing opaque donor references, distance, ETA, and score—not donor names or coordinates.
- Local request statistics, audit events, rate limiting, security headers, and graceful shutdown.
- Accessible staff and donor demonstration views.

## Important limitation

The service has no login, hospital authorization, real donor verification, clinical eligibility workflow, production routing, or notification integration. It binds to loopback by default for that reason. Use only synthetic data. The returned candidates require clinical verification and crossmatching and must not be treated as approved donors or transfusion matches.
