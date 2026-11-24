# Blood Donor Finder

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](#)

## Overview
The **Blood Donor Finder** is a prototype platform that matches hospital blood‑group requests with eligible donors using an advanced C matching engine (Red‑Black Tree & Skip List) and a lightweight Node.js/SQLite API.  The project demonstrates:

- **Advanced data structures** in C (Red‑Black Tree, Skip List) and their application to donor availability indexing.
- A **cross‑language stack**: C core → Node.js backend → vanilla HTML/JS UI.
- **Production‑style components** (rate‑limiting, request validation, CORS, environment configuration, audit logging).
- **Synthetic Bengaluru‑area dataset** for safe offline demonstration.

> **⚠️ Disclaimer** – This is a research/education prototype.  It must **not** be used for real clinical decision‑making or donor outreach without full regulatory, privacy, and clinical review.

## Architecture
```
+-------------------+      +-------------------+      +-------------------+
|  C Matching Engine|<---->|   Node.js API     |<---->|   SQLite DB      |
| (rbtree/skiplist) |      | (Express)        |      | (donor, request) |
+-------------------+      +-------------------+      +-------------------+
          ^                         ^                       ^
          |                         |                       |
          |                         |                       |
   +------+-----+          +--------+------+   +------------+---------+
   |  CLI / Demo |          | Front‑end UI |   |   Audit & Logging   |
   +------------+          +-------------+   +----------------------+
```

## Features
- **Red‑Black Tree** and **Skip List** for fast donor insert/search (O(log n)).
- **KD‑Tree** spatial index for geographic proximity.
- **Dijkstra** routing placeholder for ETA calculations.
- **Rate‑limited REST API** (`/api/*`).
- **Dynamic matching** via compiled C binary (`blood_donor_finder.exe`) with a JavaScript fallback.
- **Full UI** (staff dashboard) with dark‑mode toggle, donor registry view, and statistics panel.
- **Audit trail** for every request and match operation.
- **Synthetic data** for 20 Bengaluru donors and demo hospital requests.

## Quick‑Start (Windows)
```powershell
# 1. Install Node.js dependencies (if not already present)
npm install

# 2. Build the C demo (requires MinGW gcc)
make

# 3. Run the backend (default port 3000)
node service\server.mjs

# 4. Open the UI in a browser
start service\index.html
```

The UI talks to `http://localhost:3000/api/...`.  The C binary is expected at `../blood_donor_finder.exe` relative to the `service` folder (the `Makefile` places it in the project root).

## API Reference (selected endpoints)
| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/api/health` | Simple health check |
| `GET` | `/api/stats` | Daily aggregated statistics |
| `GET` | `/api/donors` | List all donors (public fields) |
| `GET` | `/api/donors/:ref` | Detailed donor record |
| `PUT` | `/api/donors/:ref/availability` | Toggle donor availability |
| `GET` | `/api/blood-groups` | Static catalogue of blood groups |
| `GET` | `/api/hospitals` | Demo hospital list |
| `POST`| `/api/requests` | Create a new hospital blood request |
| `GET`| `/api/match/:requestId` | Run the C matcher (75 km radius) |
| `GET`| `/api/match-js/:requestId` | JavaScript fallback matcher |

## Build & Test
- **Compile C core**: `make` (produces `blood_donor_finder.exe`).
- **Run unit tests** (if added): `make test` (placeholder).
- **Lint**: `npm run lint` (optional, uses eslint).

## Documentation
- `REPORT.md` – detailed academic report (≈ 800 words) covering data‑structure design, algorithmic complexity, system architecture, and safety considerations.
- `HOSPITAL_PILOT_BLUEPRINT.md` – pilot roadmap and compliance checklist (updated with Stage 1 completion table).

## License
MIT License – see `LICENSE` file.
