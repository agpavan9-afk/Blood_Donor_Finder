# Blood Donor Finder

Blood Donor Finder is a Bengaluru-focused research and education prototype for shortlisting blood-donor records for hospital requests. Its matching core is written in C11; a Node.js/Express service connects the compiled engine to SQLite and a responsive HTML/CSS/JavaScript interface.

> **Safety status:** this repository uses synthetic demonstration data. It is not approved for clinical decisions, donor outreach, or storage of real donor or patient data.

## What the project demonstrates

- A C KD-tree for geographic radius search.
- A bounded binary max-heap for ranked candidates.
- Preliminary ABO/Rh red-cell compatibility and freshness filters.
- C request-priority calculation for severity, scarcity, and waiting time.
- A separate C console demonstration of a Red-Black Tree, Skip List, KD-tree, heap, hash table, and Dijkstra graph routing.
- An Express API, SQLite persistence, audit events, request statistics, and a professional staff/donor demonstration UI.
- Opaque donor references in match responses; matching coordinates and donor names are not returned together.

The C engine only creates a technical shortlist. It does not establish donation eligibility, blood-component suitability, crossmatch compatibility, or authorization to contact a donor.

## Application flow

1. A synthetic donor profile updates its availability flag.
2. Hospital staff select one of the configured Bengaluru pilot hospitals, a blood group, and the required units.
3. The server supplies trusted hospital coordinates and current synthetic donor snapshots to the C executable through a tab-separated interface.
4. The C engine filters and ranks compatible, current, available records by distance and travel-time score.
5. Staff see opaque candidate references and must still follow an approved clinical verification and outreach process.

## Requirements

- Node.js 20.17.0 or newer
- npm
- A C11 GCC-compatible compiler on `PATH`

On Windows, verify the active compiler before building:

```powershell
Get-Command gcc
gcc --version
```

The build scripts use `gcc` from `PATH`; they do not contain a machine-specific compiler path. Set `CC` if a different compiler command is required.

## Run locally

From the project directory:

```powershell
npm install
npm run build:c
npm test
npm start
```

Then open [http://127.0.0.1:3000](http://127.0.0.1:3000). Do not open `service/index.html` directly; the page is designed to be served by the local API.

The first start creates `blood_donor_finder.db` and seeds 20 synthetic donor records. Database and executable files are excluded from Git.

## Commands

| Command | Purpose |
| --- | --- |
| `npm run build:c` | Strictly compile the console demo and C matcher |
| `npm run check` | Check the Node.js source files for syntax errors |
| `npm test` | Rebuild and test C behavior, UI script syntax, API integration, privacy boundaries, and persistence |
| `npm start` | Start the local service on `127.0.0.1:3000` |

The optional environment variables are `HOST`, `PORT`, `DB_FILE`, `MATCHER_BINARY`, and `CC`. Keep `HOST=127.0.0.1` for this unauthenticated local demonstration.

## Main API routes

| Method | Route | Purpose |
| --- | --- | --- |
| `GET` | `/api/health` | Service status |
| `GET` | `/api/hospitals` | Configured synthetic hospital locations |
| `GET` | `/api/donors` | Public synthetic donor directory fields |
| `PUT` | `/api/donors/:ref/availability` | Set synthetic availability to `0` or `1` |
| `POST` | `/api/requests` | Create a hospital request |
| `GET` | `/api/requests` | List recent requests |
| `GET` | `/api/match/:requestId` | Run the compiled C matcher |
| `GET` | `/api/stats` | Read local daily counters |

## Production gap

Before any real pilot, the local adapter must be replaced or extended with hospital SSO/MFA, role and tenant authorization, an approved consent and donor-identity workflow, clinical eligibility checks, verified blood-centre data, a real routing provider, encrypted production storage, secrets management, notification controls, immutable audit retention, monitoring, backup and recovery, incident response, security testing, and hospital/legal approval under applicable Indian requirements. The static ETA fixtures and score weights in this repository are demonstration values only.

The root README is the single source of setup, workflow, safety, and deployment-gap guidance for this prototype.
